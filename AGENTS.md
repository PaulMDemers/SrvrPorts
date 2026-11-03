# SrvrPorts integration

This repository mounts at `ports/` in SrvrOS. Read the parent AGENTS.md and
coordination guide before shared changes. Upstream gitlinks stay pinned; use
`git submodule update --init --recursive`, never `--remote` for normal builds.
Put SrvrOS adaptations under `srvros/` and document supported behavior in the
port manifest. Preserve licenses and source provenance. Build and run selected
port checks from a matched SrvrOS checkout; keep OS ABI/runtime fixes there.
No concurrent image builds or VM harnesses in one checkout. Commit and push
port changes before updating the SrvrOS gitlink; record exact tested revisions.
