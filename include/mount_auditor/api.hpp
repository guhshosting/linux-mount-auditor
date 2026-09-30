#pragma once
#include "mount_auditor/types.hpp"
#include <ostream>
#include <string_view>

namespace lma {
std::string decode_mountinfo(std::string_view, bool&);
ParseResult<Mount> parse_mountinfo(std::string_view);
ParseResult<FstabEntry> parse_fstab(std::string_view);
void resolve_identifiers(std::vector<FstabEntry>&, std::vector<Diagnostic>&);
void enrich_block_devices(std::vector<Mount>&, std::vector<Diagnostic>&);
std::string classify_filesystem(const Mount&);
Hierarchy build_hierarchy(const std::vector<Mount>&);
std::vector<Comparison> compare(const std::vector<Mount>&, const std::vector<FstabEntry>&, bool fstab_available);
std::vector<Finding> audit(const std::vector<Mount>&, const std::vector<FstabEntry>&, const std::vector<Comparison>&, const std::vector<Diagnostic>&);
std::string json_escape(std::string_view);
std::optional<std::string> read_bounded_file(const std::string&, std::size_t, std::string&);
bool path_beneath(std::string_view, std::string_view);
std::string join(const std::vector<std::string>&, std::string_view);
void render_mounts_json(std::ostream&, const std::vector<Mount>&, const std::vector<Diagnostic>&);
void render_fstab_json(std::ostream&, const std::vector<FstabEntry>&, const std::vector<Diagnostic>&);
void render_compare_json(std::ostream&, const std::vector<Comparison>&, const std::vector<Diagnostic>&);
void render_audit_json(std::ostream&, const std::vector<Finding>&, const std::vector<Diagnostic>&);
}
