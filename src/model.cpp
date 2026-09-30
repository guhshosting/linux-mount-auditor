#include "mount_auditor/api.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

namespace lma {
namespace {

bool has(const std::vector<std::string>& values, std::string_view value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool identifier(std::string_view value) {
  return value.rfind("UUID=", 0) == 0 || value.rfind("LABEL=", 0) == 0 ||
         value.rfind("PARTUUID=", 0) == 0 ||
         value.rfind("PARTLABEL=", 0) == 0;
}

bool source_match(const FstabEntry& fstab, const Mount& mount) {
  return fstab.spec == mount.source ||
         (fstab.resolved_spec && *fstab.resolved_spec == mount.source);
}

}  // namespace

std::vector<Comparison> compare(const std::vector<Mount>& mounts,
                                const std::vector<FstabEntry>& fstab,
                                bool available) {
  std::vector<Comparison> output;
  std::set<std::uint64_t> matched;
  if (!available) {
    output.push_back({"unavailable", "", "fstab could not be read",
                      std::nullopt, std::nullopt});
  }

  for (const auto& entry : fstab) {
    if (entry.swap) {
      output.push_back({"swap-configuration", entry.target,
                        "swap is a configuration record, not a mounted filesystem",
                        std::nullopt, entry.line});
      continue;
    }

    std::vector<const Mount*> candidates;
    for (const auto& mount : mounts) {
      if (mount.target == entry.target) {
        candidates.push_back(&mount);
      }
    }

    if (candidates.empty()) {
      output.push_back(
          {entry.noauto ? "not-expected-automatically" : "configured-not-current",
           entry.target,
           entry.noauto ? "noauto entry is not currently represented"
                        : "configured target is not visible in this namespace",
           std::nullopt, entry.line});
      continue;
    }
    if (candidates.size() > 1) {
      output.push_back({"ambiguous", entry.target,
                        "multiple current mounts use this target", std::nullopt,
                        entry.line});
      continue;
    }

    const auto& mount = *candidates.front();
    matched.insert(mount.id);
    if (identifier(entry.spec) && !entry.resolved_spec &&
        entry.spec != mount.source) {
      output.push_back(
          {"identifier-unresolved", entry.target,
           "identifier resolution was unavailable or did not establish a source match",
           mount.id, entry.line});
      continue;
    }
    if (!source_match(entry, mount)) {
      std::string detail = "configured source '";
      detail.append(entry.spec);
      detail.append("' differs from current source '");
      detail.append(mount.source);
      detail.push_back('\'');
      output.push_back(
          {"source-mismatch", entry.target, std::move(detail), mount.id, entry.line});
      continue;
    }

    const bool fstab_read_only = has(entry.options, "ro");
    const bool fstab_read_write = has(entry.options, "rw");
    if ((fstab_read_only && !mount.read_only) ||
        (fstab_read_write && mount.read_only)) {
      output.push_back(
          {"option-mismatch", entry.target,
           "configured and current read-only/read-write states differ", mount.id,
           entry.line});
      continue;
    }

    const std::pair<std::string_view, std::string_view> option_pairs[] = {
        {"noexec", "exec"},       {"nosuid", "suid"},
        {"nodev", "dev"},        {"noatime", "atime"},
        {"strictatime", "relatime"}, {"sync", "async"}};
    const bool option_difference = std::any_of(
        std::begin(option_pairs), std::end(option_pairs),
        [&](const auto& pair) {
          return (has(entry.options, pair.first) &&
                  !has(mount.vfs_options, pair.first)) ||
                 (has(entry.options, pair.second) &&
                  has(mount.vfs_options, pair.first));
        });
    if (option_difference) {
      output.push_back(
          {"option-mismatch", entry.target,
           "an explicitly configured VFS option differs from current per-mount options",
           mount.id, entry.line});
      continue;
    }

    output.push_back(
        {"represented", entry.target,
         "persistent entry has an obvious current representation", mount.id,
         entry.line});
  }

  for (const auto& mount : mounts) {
    if (matched.count(mount.id) == 0) {
      output.push_back(
          {"current-without-obvious-fstab", mount.target,
           "current mount has no obvious matching fstab entry; this is commonly legitimate",
           mount.id, std::nullopt});
    }
  }
  return output;
}

std::vector<Finding> audit(const std::vector<Mount>& mounts,
                           const std::vector<FstabEntry>& fstab,
                           const std::vector<Comparison>& comparisons,
                           const std::vector<Diagnostic>& diagnostics) {
  std::vector<Finding> output;
  output.reserve(diagnostics.size());
  std::transform(
      diagnostics.begin(), diagnostics.end(), std::back_inserter(output),
      [](const Diagnostic& diagnostic) {
        return Finding{diagnostic.level == "error" ? "warning" : "info",
                       "unavailable-or-malformed", diagnostic.source,
                       diagnostic.message};
      });

  std::map<std::string, std::vector<const FstabEntry*>> targets;
  for (const auto& entry : fstab) {
    if (!entry.swap) {
      targets[entry.target].push_back(&entry);
    }
  }
  for (const auto& [target, entries] : targets) {
    if (entries.size() > 1) {
      output.push_back(
          {"warning", "duplicate-persistent-target", target,
           "multiple fstab records use this target; intent may be ambiguous"});
    }
  }

  for (const auto& comparison : comparisons) {
    if (comparison.status == "source-mismatch" ||
        comparison.status == "option-mismatch") {
      output.push_back({"warning", "configuration-mismatch", comparison.target,
                        comparison.detail});
    } else if (comparison.status == "configured-not-current" ||
               comparison.status == "identifier-unresolved" ||
               comparison.status == "ambiguous") {
      output.push_back(
          {"info", "review", comparison.target, comparison.detail});
    }
  }

  for (const auto& mount : mounts) {
    if (mount.propagation != "private") {
      std::string message = "mount propagation is ";
      message.append(mount.propagation);
      message.append("; this is not inherently defective");
      output.push_back(
          {"info", "propagation-context", mount.target, std::move(message)});
    }
  }
  return output;
}

}  // namespace lma
