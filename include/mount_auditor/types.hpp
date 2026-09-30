#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace lma {
constexpr std::size_t kMaxInput = 16U * 1024U * 1024U;
constexpr std::size_t kMaxLine = 1024U * 1024U;
constexpr std::size_t kMaxRecords = 100000U;
constexpr std::size_t kMaxDepth = 4096U;

struct Diagnostic { std::string level, source, message; std::size_t line{}; };
struct Mount {
  std::uint64_t id{}, parent_id{}; unsigned major{}, minor{};
  std::string root, target, fs_type, source;
  std::vector<std::string> vfs_options, super_options, optional_fields;
  std::optional<std::uint64_t> shared, master, propagate_from;
  bool unbindable{}, read_only{}, bind_like{};
  std::string propagation{"private"}, classification{"unknown/other"};
  std::optional<std::string> sysfs_device, device_mapper_name, partition_parent;
  std::optional<bool> removable, rotational;
};
struct FstabEntry {
  std::string spec, target, fs_type; std::vector<std::string> options;
  std::optional<std::string> resolved_spec;
  int dump{}, pass{}; std::size_t line{};
  bool noauto{}, nofail{}, bind{}, swap{};
};
template<class T> struct ParseResult { std::vector<T> records; std::vector<Diagnostic> diagnostics; bool complete{true}; };
struct Hierarchy { std::map<std::uint64_t, std::vector<std::uint64_t>> children; std::vector<std::uint64_t> roots; std::vector<Diagnostic> diagnostics; };
struct Comparison { std::string status; std::string target; std::string detail; std::optional<std::uint64_t> mount_id; std::optional<std::size_t> fstab_line; };
struct Finding { std::string severity, category, subject, message; };
}
