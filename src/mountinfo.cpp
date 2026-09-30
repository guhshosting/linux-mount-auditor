#include "mount_auditor/api.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace lma {
namespace {

template <class T>
bool number(std::string_view input, T& value) {
  if (input.empty()) {
    return false;
  }
  const auto result =
      std::from_chars(input.data(), input.data() + input.size(), value);
  return result.ec == std::errc{} &&
         result.ptr == input.data() + input.size();
}

template <class T>
void append_number(std::string& output, T value) {
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (result.ec == std::errc{}) {
    output.append(buffer, result.ptr);
  }
}

std::vector<std::string> split(std::string_view input, char delimiter) {
  std::vector<std::string> output;
  std::size_t position = 0;
  while (position <= input.size()) {
    auto end = input.find(delimiter, position);
    if (end == std::string_view::npos) {
      end = input.size();
    }
    output.emplace_back(input.substr(position, end - position));
    if (end == input.size()) {
      break;
    }
    position = end + 1U;
  }
  return output;
}

bool has(const std::vector<std::string>& values, std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

void add_numbered_diagnostic(std::vector<Diagnostic>& diagnostics,
                             std::string level, std::string source,
                             std::string_view prefix, std::uint64_t value,
                             std::size_t line) {
  std::string message(prefix);
  append_number(message, value);
  diagnostics.push_back(
      {std::move(level), std::move(source), std::move(message), line});
}

}  // namespace

std::string decode_mountinfo(std::string_view input, bool& ok) {
  ok = true;
  std::string output;
  output.reserve(input.size());
  for (std::size_t index = 0; index < input.size(); ++index) {
    if (input[index] != '\\') {
      output.push_back(input[index]);
      continue;
    }
    if (index + 3U >= input.size()) {
      ok = false;
      output.push_back('\\');
      continue;
    }
    const auto escape = input.substr(index + 1U, 3U);
    if (escape == "040") {
      output.push_back(' ');
      index += 3U;
    } else if (escape == "011") {
      output.push_back('\t');
      index += 3U;
    } else if (escape == "012") {
      output.push_back('\n');
      index += 3U;
    } else if (escape == "134") {
      output.push_back('\\');
      index += 3U;
    } else {
      ok = false;
      output.push_back('\\');
    }
  }
  return output;
}

std::string classify_filesystem(const Mount& mount) {
  static const std::vector<std::string> pseudo = {
      "proc",       "sysfs",     "devpts",   "securityfs", "debugfs",
      "tracefs",    "cgroup",    "cgroup2",  "pstore",     "configfs",
      "mqueue",     "hugetlbfs", "efivarfs", "autofs",     "bpf",
      "rpc_pipefs"};
  static const std::vector<std::string> network = {
      "nfs", "nfs4", "cifs", "smb3", "9p",   "ceph", "afs",
      "sshfs", "glusterfs", "gfs", "gfs2", "ocfs2"};
  if (mount.bind_like) {
    return "bind/subtree mount";
  }
  if (mount.fs_type == "tmpfs" || mount.fs_type == "ramfs" ||
      mount.fs_type == "devtmpfs") {
    return "tmpfs/ram-backed";
  }
  if (mount.fs_type == "overlay" || mount.fs_type == "aufs" ||
      mount.fs_type == "unionfs") {
    return "overlay/union filesystem";
  }
  if (mount.fs_type.rfind("fuse", 0) == 0) {
    return "FUSE filesystem";
  }
  if (std::find(network.begin(), network.end(), mount.fs_type) !=
          network.end() ||
      mount.source.find(":/") != std::string::npos ||
      mount.source.rfind("//", 0) == 0) {
    return "network filesystem";
  }
  if (std::find(pseudo.begin(), pseudo.end(), mount.fs_type) != pseudo.end()) {
    return "pseudo/virtual kernel filesystem";
  }
  if (mount.major != 0U || mount.minor != 0U) {
    return "local block-backed filesystem";
  }
  return "unknown/other";
}

ParseResult<Mount> parse_mountinfo(std::string_view input) {
  ParseResult<Mount> result;
  if (input.size() > kMaxInput) {
    result.complete = false;
    result.diagnostics.push_back(
        {"error", "mountinfo", "input exceeds 16 MiB", 0});
    return result;
  }

  std::set<std::uint64_t> ids;
  std::size_t position = 0;
  std::size_t line = 0;
  while (position < input.size()) {
    auto end = input.find('\n', position);
    if (end == std::string_view::npos) {
      end = input.size();
    }
    const auto text = input.substr(position, end - position);
    ++line;
    position = end < input.size() ? end + 1U : end;

    if (text.size() > kMaxLine) {
      result.complete = false;
      result.diagnostics.push_back(
          {"error", "mountinfo", "line exceeds 1 MiB", line});
      continue;
    }
    if (text.empty()) {
      continue;
    }
    if (result.records.size() >= kMaxRecords) {
      result.complete = false;
      result.diagnostics.push_back(
          {"error", "mountinfo", "record limit exceeded", line});
      break;
    }

    const auto fields = split(text, ' ');
    const auto separator = std::find(fields.begin(), fields.end(), "-");
    if (separator == fields.end() ||
        std::distance(fields.begin(), separator) < 6 ||
        std::distance(separator, fields.end()) < 4) {
      result.complete = false;
      result.diagnostics.push_back(
          {"error", "mountinfo", "malformed fields or separator", line});
      continue;
    }

    Mount mount;
    if (!number(fields[0], mount.id) || !number(fields[1], mount.parent_id)) {
      result.complete = false;
      result.diagnostics.push_back(
          {"error", "mountinfo", "invalid mount or parent ID", line});
      continue;
    }
    const auto colon = fields[2].find(':');
    if (colon == std::string::npos ||
        !number(std::string_view(fields[2]).substr(0, colon), mount.major) ||
        !number(std::string_view(fields[2]).substr(colon + 1U), mount.minor)) {
      result.complete = false;
      result.diagnostics.push_back(
          {"error", "mountinfo", "invalid major:minor", line});
      continue;
    }

    bool root_ok = true;
    bool target_ok = true;
    bool source_ok = true;
    mount.root = decode_mountinfo(fields[3], root_ok);
    mount.target = decode_mountinfo(fields[4], target_ok);
    mount.vfs_options = split(fields[5], ',');
    for (auto field = fields.begin() + 6; field != separator; ++field) {
      mount.optional_fields.push_back(*field);
    }
    mount.fs_type = *(separator + 1);
    mount.source = decode_mountinfo(*(separator + 2), source_ok);
    mount.super_options = split(*(separator + 3), ',');
    if (!root_ok || !target_ok || !source_ok) {
      result.complete = false;
      result.diagnostics.push_back(
          {"warning", "mountinfo", "malformed or unknown escape preserved",
           line});
    }

    if (!ids.insert(mount.id).second) {
      result.complete = false;
      add_numbered_diagnostic(result.diagnostics, "error", "mountinfo",
                              "duplicate mount ID ", mount.id, line);
      continue;
    }
    mount.read_only = has(mount.vfs_options, "ro");

    for (const auto& optional : mount.optional_fields) {
      std::uint64_t value = 0;
      if (optional.rfind("shared:", 0) == 0 &&
          number(std::string_view(optional).substr(7), value)) {
        mount.shared = value;
      } else if (optional.rfind("master:", 0) == 0 &&
                 number(std::string_view(optional).substr(7), value)) {
        mount.master = value;
      } else if (optional.rfind("propagate_from:", 0) == 0 &&
                 number(std::string_view(optional).substr(15), value)) {
        mount.propagate_from = value;
      } else if (optional == "unbindable") {
        mount.unbindable = true;
      }
    }

    if (mount.unbindable) {
      mount.propagation = "unbindable";
    } else if (mount.shared && mount.master) {
      mount.propagation = "shared-and-slave";
    } else if (mount.shared) {
      mount.propagation = "shared";
    } else if (mount.master) {
      mount.propagation = "slave";
    }

    mount.bind_like =
        (mount.root != "/" && mount.fs_type != "btrfs") ||
        has(mount.vfs_options, "bind") || has(mount.vfs_options, "rbind");
    mount.classification = classify_filesystem(mount);
    result.records.push_back(std::move(mount));
  }
  return result;
}

Hierarchy build_hierarchy(const std::vector<Mount>& mounts) {
  Hierarchy hierarchy;
  std::map<std::uint64_t, const Mount*> by_id;
  for (const auto& mount : mounts) {
    by_id[mount.id] = &mount;
  }

  for (const auto& mount : mounts) {
    if (mount.parent_id == mount.id) {
      hierarchy.roots.push_back(mount.id);
      add_numbered_diagnostic(hierarchy.diagnostics, "error", "hierarchy",
                              "self-parent cycle at mount ", mount.id, 0);
    } else if (by_id.count(mount.parent_id) == 0) {
      hierarchy.roots.push_back(mount.id);
      add_numbered_diagnostic(
          hierarchy.diagnostics, "info", "hierarchy",
          "parent outside visible data for mount ", mount.id, 0);
    } else {
      hierarchy.children[mount.parent_id].push_back(mount.id);
    }
  }
  for (auto& [parent, children] : hierarchy.children) {
    (void)parent;
    std::sort(children.begin(), children.end());
  }
  std::sort(hierarchy.roots.begin(), hierarchy.roots.end());

  enum class Color { white, gray, black };
  std::map<std::uint64_t, Color> colors;
  for (const auto& mount : mounts) {
    colors[mount.id] = Color::white;
  }
  for (const auto& mount : mounts) {
    std::uint64_t current = mount.id;
    std::vector<std::uint64_t> path;
    bool stopped = false;
    for (std::size_t depth = 0; depth < kMaxDepth; ++depth) {
      if (by_id.count(current) == 0 || colors[current] == Color::black) {
        stopped = true;
        break;
      }
      if (colors[current] == Color::gray) {
        add_numbered_diagnostic(hierarchy.diagnostics, "error", "hierarchy",
                                "cycle detected at mount ", current, 0);
        stopped = true;
        break;
      }
      colors[current] = Color::gray;
      path.push_back(current);
      const auto parent = by_id[current]->parent_id;
      if (parent == current || by_id.count(parent) == 0) {
        stopped = true;
        break;
      }
      current = parent;
    }
    if (!stopped) {
      add_numbered_diagnostic(
          hierarchy.diagnostics, "warning", "hierarchy",
          "hierarchy depth limit reached from mount ", mount.id, 0);
    }
    for (const auto id : path) {
      colors[id] = Color::black;
    }
  }
  return hierarchy;
}

}  // namespace lma
