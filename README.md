# linux-mount-auditor

`linux-mount-auditor` 1.0.0 is a Linux-only, strictly read-only command-line auditor for the mount namespace visible to the process. It parses `/proc/self/mountinfo`, optionally reads `/etc/fstab`, explains mount topology and propagation, and compares current state with persistent configuration conservatively.

It never mounts, unmounts, probes, repairs, writes devices, changes propagation, invokes a shell, or contacts a network. No external helper commands are used. Current network mounts are described only from local metadata; reachability is never tested.

## Build and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local
```

The build requires CMake 3.16 and a C++17 compiler. GCC is compiled with `-Wall -Wextra -Wpedantic -Werror`.

## Usage

```text
linux-mount-auditor inspect [--all] [--include-pseudo] [--json]
linux-mount-auditor tree
linux-mount-auditor show TARGET
linux-mount-auditor fstab [--fstab PATH]
linux-mount-auditor compare [--fstab PATH]
linux-mount-auditor audit
linux-mount-auditor check
```

Filters include `--type TYPE`, `--source SOURCE`, and `--target TARGET`. `--mountinfo PATH` exists for controlled fixture/offline analysis; both alternate inputs are opened read-only and capped at 16 MiB. `show` uses lexical longest mount-point prefix matching without resolving symlinks and reports all equal best matches; this describes the supplied path spelling, not guaranteed VFS ownership after symlink traversal.

`inspect` omits pseudo and RAM-backed mounts unless `--include-pseudo` or `--all` is used. JSON schema version 1 keeps current mounts, fstab records, comparisons, findings, and diagnostics distinct. JSON is written only to stdout; operational diagnostics use stderr in human mode.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Command completed; `audit` may contain informational/review findings |
| 1 | `check` found warning-level concerns, or `show` found no match |
| 2 | Invalid command line |
| 3 | required mountinfo/fstab data unavailable |
| 4 | partial or malformed input/hierarchy |
| 5 | internal/resource failure |

The snapshot is not atomic: mountinfo, fstab, `/dev/disk`, and any sysfs state can change between reads. Results apply only to the process's current mount namespace, which can differ from the host or another process.

See `docs/` for detailed semantics, privacy, architecture, and limitations. Licensed under MIT.
