# fstab semantics

The parser accepts blank lines and comments, requires the standard six fields, and decodes fstab's octal whitespace/backslash escapes. Dump and pass fields must be nonnegative decimal integers. Unknown options, including `x-systemd.*`, remain opaque strings. Swap records are retained but never described as mounted filesystems.

UUID, LABEL, PARTUUID, and PARTLABEL are resolved only through the corresponding local `/dev/disk/by-*` symlink. No block device is opened or probed. A missing or changing link produces an informational diagnostic; uniqueness is never claimed beyond that local mapping.

Comparison first uses the exact target. It then checks literal or resolved source and explicit ro/rw intent. Multiple current mounts at a target are ambiguous. An absent `noauto` entry is “not expected automatically.” A current mount without an fstab match is informational because dynamic, generated, container, removable, and automount configurations are normal. Namespace visibility can make every comparison incomplete.
