# Validation report

Initial validation date: 2026-09-17. Environment: Linux x86-64, GCC 13.3.0. Commands with host-dependent behavior were bounded with `timeout`.

## Performed

- Strict independent Debug build with C++17 and `-Wall -Wextra -Wpedantic -Werror`: passed.
- Strict independent Release build (`-O2 -DNDEBUG` with the same warnings): passed.
- Unit, malformed-input, robustness, comparison, JSON, hierarchy, and false-positive suite: 73 checks passed in Debug, Release, and ASan/UBSan builds.
- CLI integration suite: 13 cases passed in Debug, Release, and ASan/UBSan builds. This includes JSON parsing, missing input, usage rejection, fixture commands, and a closed-pipe pipeline.
- ASan and UBSan: passed with leak detection disabled.
- LeakSanitizer: attempted separately; runtime reported that LeakSanitizer cannot operate under this environment's ptrace conditions. No leak result is claimed.
- GCC `-fanalyzer`: attempted with a 60-second bound. GCC 13 emitted diagnostics within libstdc++ allocator/vector internals (including an `unordered_map` allocation it modeled as leaked and synthesized uninitialized allocator state). These do not identify an application-owned unmatched allocation, conflict with RAII ownership, and were not reproduced by ASan/UBSan. They are documented as analyzer false positives rather than suppressed.
- Man page: rendered with `groff -man -Tutf8` without diagnostics.
- Staged installation: executable and uncompressed man page installed to a temporary `/usr` tree; staged executable reported 1.0.0.
- Debian package: generated with `dpkg-deb`, then control metadata and full payload were inspected. It contains only `/usr/bin/linux-mount-auditor` and `/usr/share/man/man1/linux-mount-auditor.1`, with the requested maintainer and repository URL. Package installation was not performed.
- Live namespace validation: `inspect --all`, `tree`, `fstab`, `compare`, and `audit` ran against real `/proc/self/mountinfo`, `/etc/fstab`, `/sys`, and `/dev/disk` visible in the container. JSON outputs parsed successfully. The visible namespace contained 24 mounts including overlay, proc, sysfs, cgroup2, and tmpfs mounts. The container fstab had no records. This describes only the process-visible namespace.
- Source-tree metadata scan: no unintended example-domain, placeholder-contact, or development-marker release metadata was found.
- Safety review: production source has no process creation, shell execution, helper invocation, networking, device opening, write-mode file opening, mount/unmount, filesystem repair/format, swap, systemctl, namespace, fstab-write, or propagation-changing path. The only acquisition calls open named inputs read-only, read bounded bytes, and resolve bounded local symlinks.

## Acceptance conclusions

Fixture tests establish that overlay, many container-style mounts, bind/subtree mounts, NFS/CIFS-like mounts, FUSE, tmpfs, dynamic mounts absent from fstab, absent `noauto` records, repeated sources, non-private propagation, intentionally read-only mounts, and namespace-limited data are not automatically called broken. Findings distinguish malformed input and explicit mismatches from contextual information.

Mountinfo grammar, defined escapes, optional future fields, VFS/superblock separation, IDs, missing parents, duplicate IDs, cycles, deep input, propagation, fstab comments/escapes/identifiers/options/swap, longest-prefix lookup, comparison ambiguity, JSON escaping, integer parsing, input caps, and changing/unavailable local data received direct fixture or live coverage.

## Environment-limited or not performed

- CMake and CTest were unavailable, so canonical CMake configure/CTest were not run. The same declared source units, flags, binaries, and test programs were built directly with GCC.
- `cppcheck` was unavailable.
- `rpmbuild` was unavailable. The RPM spec was reviewed but no RPM was generated or inspected.
- Native Debian `dpkg-buildpackage`/debhelper was unavailable through the missing CMake/debhelper toolchain; a binary Debian package was generated and inspected with `dpkg-deb`.
- Package installation was not performed.
- No mounts, namespaces, propagation, devices, fstab, swap, or system services were altered for testing.

## Preceding archive clean-room validation

The final source-only archive was listed and checked for build directories, binaries, objects, libraries, packages, caches, logs, generated compressed man pages, and VCS metadata. It contained 37 source/documentation/directory entries and no contamination. That exact archive was extracted to a fresh temporary directory, then received a strict Release GCC build plus all 73 unit checks and 13 CLI cases; all passed. Canonical CMake/CTest clean-room validation remains unverified because those programs were unavailable.

## Static-analysis repair validation — 2026-09-29

The preceding archive's SHA-256 was verified as `54e48d0009eb5af175456f7997c57d4da655e03fe9ba27237030f264ae53f4db` before modification. The user separately reported that this preceding archive passed a real Debian 13 / GCC 14.2.0 strict Release build, all five CTest targets, live command and JSON checks, a read-only strace audit, and ASan/UBSan. Those host results were supplied as external validation and were not rerun in this container.

The static-analysis repair made local changes in `src/model.cpp`, `src/render.cpp`, `src/util.cpp`, `src/mountinfo.cpp`, `src/main.cpp`, and `tests/test_main.cpp`:

- Removed a redundant bind-gated source comparison whose source equality predicate had already failed on that branch.
- Replaced the diagnostic-to-finding projection with `std::transform`.
- Replaced optional-number `std::to_string` temporaries in JSON rendering with direct, explicit stream insertion and explicit `null` branches. JSON string helpers now hold escaped output in owned local strings before insertion.
- Reworked error/path construction to use owned strings with `append`, `push_back`, and `std::to_chars`, avoiding nested string-concatenation temporaries that GCC 14.2's analyzer modeled incorrectly.
- Replaced the mount-ID `std::unordered_map` with `std::set`: only membership was required, ordering does not affect output, input is capped at 100,000 records, and logarithmic lookup is acceptable. The reported bucket allocation belonged to the automatic hash container and was therefore a compiler false positive, not an application leak.
- Replaced erase/remove filtering with reserved copy selection to avoid an older analyzer's false positive in libstdc++ vector move assignment.
- Fixed a genuine bounded-read edge case by checking whether one read result itself exceeds the configured limit before subtracting it from that limit.
- Added regression checks for exact bind-source comparison and for a single read exceeding a small limit. The unit suite now contains 75 checks.

Local repair validation used the available GCC 13.3.0 because GCC 14.2.0 is not installed in this container:

- Strict Release with `-Wall -Wextra -Wpedantic -Werror`: passed.
- Direct equivalents of all five CTest targets: passed (75 unit checks plus the help, version, fixture-JSON, and 13-case CLI targets). CMake and CTest themselves remain unavailable locally.
- Unoptimized `-fanalyzer` build with all strict warnings as errors: passed for both the application and unit-test executable. All five direct test equivalents then passed from the analyzer binaries.
- ASan/UBSan: all five direct test equivalents passed with leak detection disabled because LeakSanitizer is unsupported in this environment.
- Behavior comparison: 42 stdout, stderr, and exit-status artifacts from the preceding archive and repaired source were byte-identical. Coverage included human and JSON inspect/fstab/compare/audit, tree, show, check, filters, help/version, no-match, missing-input, and usage-error paths. All JSON artifacts parsed successfully.
- Exhaustive cppcheck could not be rerun because cppcheck is unavailable locally. The two reported constructs were removed without suppressions; a real-host cppcheck rerun remains required for tool-level confirmation.
- Man-page rendering remained clean.

The repaired exact source archive was rebuilt without generated artifacts, freshly extracted, and subjected to the same local strict Release build, five direct test equivalents, unoptimized `-fanalyzer` build, and five analyzer-build test equivalents. All passed. Canonical GCC 14.2.0, CMake/CTest, and exhaustive cppcheck confirmation remain reserved for the subsequent real Debian 13 rerun.

## Final cppcheck style follow-up — 2026-09-29

The user reported that the repaired archive passed GCC 14.2.0 strict Release, all five CTest targets, the GCC 14.2.0 unoptimized analyzer build and its five CTest targets, ASan/UBSan, and version validation on Debian 13. The sole remaining exhaustive cppcheck diagnostic requested `std::any_of` for the option-pair predicate in `src/model.cpp`.

The raw flag-and-break loop was replaced with `std::any_of` over the same `option_pairs` array and the same predicate. No output or audit condition changed. Local GCC 13.3.0 strict Release, all five direct test equivalents, unoptimized `-fanalyzer` plus all five equivalents, and ASan/UBSan plus all five equivalents passed. A focused old-versus-new comparison exercised the true option-mismatch branch through `compare`, `audit`, and `check`; human output, JSON, diagnostics, and exit status were byte-identical. Exhaustive cppcheck was not available locally, so cppcheck exit status is not claimed pending the final Debian 13 rerun.

The final replacement source archive was freshly extracted and passed the same local strict Release and analyzer builds plus both sets of five direct test equivalents.

## Debian dependency metadata follow-up — 2026-09-29

The user reported that the preceding archive passed final Debian 13 validation with GCC 14.2.0: strict Release, all five CTest targets, exhaustive cppcheck 2.17.1 with zero diagnostics, unoptimized `-fanalyzer` and its five CTest targets, ASan/UBSan, live functional and JSON checks, a read-only strace audit, man-page rendering, staged installation, directory-mode inspection, and confirmation that the Debian package contained no maintainer scripts. Those host results were supplied as external validation and were not rerun in this container.

CPack's Debian configuration now sets `CPACK_DEBIAN_PACKAGE_SHLIBDEPS` to `ON`. At DEB generation time CPack invokes the Debian `dpkg-shlibdeps` machinery on installed ELF binaries and uses its computed `${shlibs:Depends}` value, so runtime package relationships come from actual shared-library requirements instead of hard-coded guesses. This setting is generator-specific and does not affect normal source builds or RPM metadata.

Local validation used GCC 13.3.0 and `dpkg-shlibdeps` 1.22.6:

- Strict Release with C++17 and `-Wall -Wextra -Wpedantic -Werror`: passed.
- Direct equivalents of all five CTest targets passed (75 unit checks, help, version, fixture JSON, and the 13-case CLI suite).
- `dpkg-shlibdeps` analyzed the freshly linked executable and generated `libc6 (>= 2.34), libgcc-s1 (>= 3.0), libstdc++6 (>= 13.1)` for this Ubuntu 24.04 build environment.
- A representative Debian package assembled from that executable with the generated dependency value had a non-empty `Depends` field, the requested executable and man-page paths, no maintainer scripts, and mode 0755 for every package directory. Its control metadata and complete payload were inspected with `dpkg-deb`.
- No application source file changed; only CPack configuration and this validation record changed. Application behavior is therefore unaffected by the packaging-only setting.

CMake, CTest, CPack, and rpmbuild are unavailable in this container. Consequently, local CPack DEB generation and RPM regeneration are not claimed; those generator-level checks remain for the Debian packaging host. The source archive was nevertheless freshly extracted and subjected to the strongest locally available strict Release build and all five direct test equivalents.

## Known limitations

Classification is intentionally conservative and finite. Unknown filesystem families remain unknown. Identifier resolution relies on currently visible `/dev/disk/by-*` links. `show` performs lexical longest-prefix matching and does not resolve symlinks. Comparison cannot account for every systemd generator, automounter, host namespace, or dynamic manager. The multi-source snapshot is not atomic. No server reachability or filesystem health is tested.
