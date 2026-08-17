# Data

This directory is the index for project-owned measurement data.

Do not copy existing Governor captures here. Link to their authoritative location
from the evidence ledger. New cross-process traces may be indexed here when they
belong to this project rather than an adjacent repository.

## Storage policy

- Commit small CSV/JSON summaries and normalized configuration manifests.
- Preserve raw sample identities and timestamps.
- Keep large ETL, GPU capture, RenderDoc, video, and image artifacts outside Git
  unless an explicit large-file policy is adopted.
- Record absolute artifact path, file size, SHA-256, capture tool/version, and the
  experiment ID in an index entry.
- Derived data must name the raw inputs and analysis script revision.

Suggested index fields:

```text
experiment_id
capture_id
artifact_type
path_or_url
sha256
size_bytes
tool_and_version
created_utc
configuration_signature_hash
notes
```

