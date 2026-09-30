# Architecture

The executable is a thin CLI over separately testable components:

- `util.cpp`: bounded `open`/`read`, JSON escaping, lexical mount-prefix checks, and local identifier resolution.
- `mountinfo.cpp`: mountinfo grammar, defined octal decoding, classification, propagation metadata, and bounded hierarchy validation.
- `fstab.cpp`: comment-aware six-field parser with fstab octal decoding.
- `model.cpp`: conservative matching and findings.
- `render.cpp`: deterministic human-independent JSON schema version 1.

Inputs are capped at 16 MiB, lines at 1 MiB, records at 100,000, and hierarchy walks at 4,096 steps. Parsing never invokes commands. Acquisition uses `O_RDONLY|O_CLOEXEC|O_NOCTTY`. Identifier mapping uses `realpath` only for a specifically constructed `/dev/disk/by-*` path after rejecting slash-containing identifiers. Races and disappearing links become diagnostics.

The classifier is deliberately finite and non-authoritative. A nonzero major/minor is useful block-backed evidence, while type/source heuristics identify common network, FUSE, pseudo, RAM, and overlay families. Unknown remains unknown.
