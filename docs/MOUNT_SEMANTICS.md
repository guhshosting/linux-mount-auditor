# Mount semantics

`/proc/self/mountinfo` is authoritative for mounts visible to this process. Fields before `-` are mount ID, parent ID, major:minor, filesystem root, mount point, VFS options, and optional fields. Fields after it are filesystem type, source, and superblock options. Only mountinfo's defined `\040`, `\011`, `\012`, and `\134` encodings are decoded; unknown escapes are preserved and diagnosed.

VFS options and superblock options are retained separately. `ro` is inferred from the per-mount VFS option list; absence of an arbitrary option is not broadly presented as its opposite. Bind-like evidence is reported when mountinfo exposes a non-root subtree or explicit bind option. A non-root Btrfs subvolume alone is not called a bind mount.

`shared:N`, `master:N`, `propagate_from:N`, and `unbindable` are retained. Shared plus master is described as shared-and-slave. When none is exposed the mount is described as private, consistent with mountinfo propagation representation, but behavior beyond this namespace is not inferred.

Parent IDs form a bounded graph. Missing parents are expected at namespace boundaries and are informational. Duplicate IDs and cycles are malformed/incomplete input. Tree rendering never recursively walks beyond 4,096 levels and also guards visited IDs.

Network classification does not establish reachability, latency, health, or causality. Read-only, noexec, overlays, high mount counts, and unusual propagation likewise do not establish why an application failed.
