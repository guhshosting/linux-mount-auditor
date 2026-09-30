#include "mount_auditor/api.hpp"

#include <ostream>

namespace lma {
namespace {

void write_json_string(std::ostream& output, std::string_view key,
                       std::string_view value, bool comma = true) {
  const std::string escaped = json_escape(value);
  output << '"' << key << "\":\"" << escaped << '"';
  if (comma) {
    output << ',';
  }
}

void write_optional_string(std::ostream& output, std::string_view key,
                           const std::optional<std::string>& value) {
  output << '"' << key << "\":";
  if (value) {
    const std::string escaped = json_escape(*value);
    output << '"' << escaped << '"';
  } else {
    output << "null";
  }
  output << ',';
}

void write_optional_bool(std::ostream& output, std::string_view key,
                         const std::optional<bool>& value) {
  output << '"' << key << "\":";
  if (value) {
    output << (*value ? "true" : "false");
  } else {
    output << "null";
  }
  output << ',';
}

void write_diagnostics(std::ostream& output,
                       const std::vector<Diagnostic>& diagnostics) {
  output << "\"diagnostics\":[";
  for (std::size_t index = 0; index < diagnostics.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& diagnostic = diagnostics[index];
    output << '{';
    write_json_string(output, "level", diagnostic.level);
    write_json_string(output, "source", diagnostic.source);
    write_json_string(output, "message", diagnostic.message);
    output << "\"line\":" << diagnostic.line << '}';
  }
  output << ']';
}

void write_string_list(std::ostream& output, std::string_view key,
                       const std::vector<std::string>& values) {
  output << '"' << key << "\":[";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const std::string escaped = json_escape(values[index]);
    output << '"' << escaped << '"';
  }
  output << ']';
}

template <typename T>
void write_optional_number(std::ostream& output, std::string_view key,
                           const std::optional<T>& value) {
  output << '"' << key << "\":";
  if (value) {
    output << *value;
  } else {
    output << "null";
  }
}

}  // namespace

void render_mounts_json(std::ostream& output, const std::vector<Mount>& mounts,
                        const std::vector<Diagnostic>& diagnostics) {
  output << "{\"schema_version\":1,\"snapshot_atomic\":false,"
            "\"current_mounts\":[";
  for (std::size_t index = 0; index < mounts.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& mount = mounts[index];
    output << '{';
    output << "\"mount_id\":" << mount.id << ",\"parent_id\":"
           << mount.parent_id << ",\"major\":" << mount.major
           << ",\"minor\":" << mount.minor << ',';
    write_json_string(output, "root", mount.root);
    write_json_string(output, "target", mount.target);
    write_json_string(output, "filesystem_type", mount.fs_type);
    write_json_string(output, "source", mount.source);
    write_json_string(output, "classification", mount.classification);
    write_json_string(output, "propagation", mount.propagation);
    write_optional_string(output, "sysfs_device", mount.sysfs_device);
    write_optional_string(output, "device_mapper_name",
                          mount.device_mapper_name);
    write_optional_string(output, "partition_parent", mount.partition_parent);
    write_optional_bool(output, "removable", mount.removable);
    write_optional_bool(output, "rotational", mount.rotational);
    output << "\"read_only\":" << (mount.read_only ? "true" : "false")
           << ",\"bind_like\":" << (mount.bind_like ? "true" : "false")
           << ',';
    write_string_list(output, "vfs_options", mount.vfs_options);
    output << ',';
    write_string_list(output, "superblock_options", mount.super_options);
    output << '}';
  }
  output << "],";
  write_diagnostics(output, diagnostics);
  output << "}\n";
}

void render_fstab_json(std::ostream& output,
                       const std::vector<FstabEntry>& entries,
                       const std::vector<Diagnostic>& diagnostics) {
  output << "{\"schema_version\":1,\"fstab\":[";
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& entry = entries[index];
    output << '{';
    write_json_string(output, "spec", entry.spec);
    write_json_string(output, "target", entry.target);
    write_json_string(output, "filesystem_type", entry.fs_type);
    write_optional_string(output, "resolved_spec", entry.resolved_spec);
    write_string_list(output, "options", entry.options);
    output << ",\"dump\":" << entry.dump << ",\"pass\":" << entry.pass
           << ",\"line\":" << entry.line << '}';
  }
  output << "],";
  write_diagnostics(output, diagnostics);
  output << "}\n";
}

void render_compare_json(std::ostream& output,
                         const std::vector<Comparison>& comparisons,
                         const std::vector<Diagnostic>& diagnostics) {
  output << "{\"schema_version\":1,\"comparison\":[";
  for (std::size_t index = 0; index < comparisons.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& comparison = comparisons[index];
    output << '{';
    write_json_string(output, "status", comparison.status);
    write_json_string(output, "target", comparison.target);
    write_json_string(output, "detail", comparison.detail);
    write_optional_number(output, "mount_id", comparison.mount_id);
    output << ',';
    write_optional_number(output, "fstab_line", comparison.fstab_line);
    output << '}';
  }
  output << "],";
  write_diagnostics(output, diagnostics);
  output << "}\n";
}

void render_audit_json(std::ostream& output,
                       const std::vector<Finding>& findings,
                       const std::vector<Diagnostic>& diagnostics) {
  output << "{\"schema_version\":1,\"findings\":[";
  for (std::size_t index = 0; index < findings.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& finding = findings[index];
    output << '{';
    write_json_string(output, "severity", finding.severity);
    write_json_string(output, "category", finding.category);
    write_json_string(output, "subject", finding.subject);
    write_json_string(output, "message", finding.message, false);
    output << '}';
  }
  output << "],";
  write_diagnostics(output, diagnostics);
  output << "}\n";
}

}  // namespace lma
