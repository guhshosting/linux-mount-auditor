#include "mount_auditor/api.hpp"

#include <cerrno>
#include <charconv>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

namespace lma {
namespace {

void set_io_error(std::string& output, std::string_view operation,
                  const std::string& path, int error_number) {
  output.clear();
  output.reserve(operation.size() + path.size() + 3U + 64U);
  output.append("cannot ");
  output.append(operation);
  output.push_back(' ');
  output.append(path);
  output.append(": ");
  output.append(std::strerror(error_number));
}

template <typename T>
void append_decimal(std::string& output, T value) {
  char buffer[32];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (result.ec == std::errc{}) {
    output.append(buffer, result.ptr);
  }
}

void append_child(std::string& path, std::string_view child) {
  path.push_back('/');
  path.append(child);
}

}  // namespace

std::optional<std::string> read_bounded_file(const std::string& path,
                                             std::size_t limit,
                                             std::string& error) {
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOCTTY);
  if (fd < 0) {
    set_io_error(error, "open", path, errno);
    return std::nullopt;
  }

  std::string output;
  output.reserve(limit < 65536U ? limit : 65536U);
  char buffer[8192];
  while (true) {
    const ssize_t count = ::read(fd, buffer, sizeof(buffer));
    if (count == 0) {
      break;
    }
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      const int read_error = errno;
      set_io_error(error, "read", path, read_error);
      ::close(fd);
      return std::nullopt;
    }
    const auto byte_count = static_cast<std::size_t>(count);
    if (byte_count > limit || output.size() > limit - byte_count) {
      error.clear();
      error.reserve(path.size() + 21U);
      error.append(path);
      error.append(": input exceeds limit");
      ::close(fd);
      return std::nullopt;
    }
    output.append(buffer, byte_count);
  }

  if (::close(fd) != 0) {
    set_io_error(error, "close", path, errno);
    return std::nullopt;
  }
  return output;
}

std::string json_escape(std::string_view input) {
  std::string output;
  output.reserve(input.size());
  constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char value : input) {
    switch (value) {
      case '"':
        output.append("\\\"");
        break;
      case '\\':
        output.append("\\\\");
        break;
      case '\b':
        output.append("\\b");
        break;
      case '\f':
        output.append("\\f");
        break;
      case '\n':
        output.append("\\n");
        break;
      case '\r':
        output.append("\\r");
        break;
      case '\t':
        output.append("\\t");
        break;
      default:
        if (value < 0x20U) {
          output.append("\\u00");
          output.push_back(hex[value >> 4U]);
          output.push_back(hex[value & 15U]);
        } else {
          output.push_back(static_cast<char>(value));
        }
        break;
    }
  }
  return output;
}

std::string join(const std::vector<std::string>& values,
                 std::string_view separator) {
  std::string output;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output.append(separator);
    }
    output.append(values[index]);
  }
  return output;
}

bool path_beneath(std::string_view path, std::string_view mount) {
  if (mount == "/") {
    return !path.empty() && path.front() == '/';
  }
  return path == mount ||
         (path.size() > mount.size() &&
          path.substr(0, mount.size()) == mount && path[mount.size()] == '/');
}

void resolve_identifiers(std::vector<FstabEntry>& entries,
                         std::vector<Diagnostic>& diagnostics) {
  for (auto& entry : entries) {
    const std::pair<std::string_view, std::string_view> kinds[] = {
        {"UUID=", "by-uuid/"},
        {"LABEL=", "by-label/"},
        {"PARTUUID=", "by-partuuid/"},
        {"PARTLABEL=", "by-partlabel/"}};
    for (const auto& kind : kinds) {
      if (entry.spec.rfind(kind.first, 0) != 0) {
        continue;
      }
      const std::string value = entry.spec.substr(kind.first.size());
      if (value.empty() || value.find('/') != std::string::npos) {
        std::string message = "invalid identifier at fstab line ";
        append_decimal(message, entry.line);
        diagnostics.push_back(
            {"warning", "identifier", std::move(message), entry.line});
        break;
      }

      std::string path = "/dev/disk/";
      path.reserve(path.size() + kind.second.size() + value.size());
      path.append(kind.second);
      path.append(value);
      char resolved[PATH_MAX];
      if (::realpath(path.c_str(), resolved) != nullptr) {
        entry.resolved_spec = std::string(resolved);
      } else {
        std::string message = "could not resolve ";
        message.append(entry.spec);
        message.append(" from local /dev/disk links");
        diagnostics.push_back(
            {"info", "identifier", std::move(message), entry.line});
      }
      break;
    }
  }
}

void enrich_block_devices(std::vector<Mount>& mounts,
                          std::vector<Diagnostic>& diagnostics) {
  for (auto& mount : mounts) {
    if (mount.classification != "local block-backed filesystem") {
      continue;
    }

    std::string link = "/sys/dev/block/";
    append_decimal(link, mount.major);
    link.push_back(':');
    append_decimal(link, mount.minor);
    char resolved[PATH_MAX];
    if (::realpath(link.c_str(), resolved) == nullptr) {
      std::string message =
          "block relationship disappeared or is unavailable for ";
      message.append(link);
      diagnostics.push_back(
          {"info", "sysfs", std::move(message), 0});
      continue;
    }

    std::string path(resolved);
    const auto slash = path.find_last_of('/');
    mount.sysfs_device =
        slash == std::string::npos ? path : path.substr(slash + 1U);

    std::string error;
    std::string device_mapper_path = path;
    append_child(device_mapper_path, "dm/name");
    auto device_mapper_name =
        read_bounded_file(device_mapper_path, 4096U, error);
    if (device_mapper_name) {
      while (!device_mapper_name->empty() &&
             (device_mapper_name->back() == '\n' ||
              device_mapper_name->back() == '\r')) {
        device_mapper_name->pop_back();
      }
      mount.device_mapper_name = *device_mapper_name;
    }

    error.clear();
    std::string partition_path = path;
    append_child(partition_path, "partition");
    const auto partition = read_bounded_file(partition_path, 64U, error);
    std::string base = path;
    if (partition) {
      const std::string parent = path.substr(0, path.find_last_of('/'));
      const auto parent_slash = parent.find_last_of('/');
      mount.partition_parent = parent_slash == std::string::npos
                                   ? parent
                                   : parent.substr(parent_slash + 1U);
      base = parent;
    }

    const auto read_boolean = [](const std::string& boolean_path)
        -> std::optional<bool> {
      std::string boolean_error;
      const auto value =
          read_bounded_file(boolean_path, 64U, boolean_error);
      if (!value) {
        return std::nullopt;
      }
      return !value->empty() && (*value)[0] == '1';
    };

    std::string removable_path = base;
    append_child(removable_path, "removable");
    mount.removable = read_boolean(removable_path);
    std::string rotational_path = base;
    append_child(rotational_path, "queue/rotational");
    mount.rotational = read_boolean(rotational_path);
  }
}

}  // namespace lma
