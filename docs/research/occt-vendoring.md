# Open CASCADE as a submodule: what it costs (measured 2026-10-21)

For M3 (AGENTS.md section 6). Measured on this machine (16 threads) with OCCT `V7_9_3`, the
newest stable release (8.0 is at release candidates), built with only the modules NG needs:
FoundationClasses, ModelingData, ModelingAlgorithms and DataExchange (STEP). Draw, Visualization,
ApplicationFramework, DETools, Tk, FreeType and OpenGL off; static libraries.

| What | Size / time |
|---|---|
| Upstream repository, full history (GitHub's figure) | 220 MB packed |
| Shallow clone of the tag (`--depth 1`) | 59 MB `.git` + 294 MB sources = 352 MB |
| Release build tree | 755 MB, of which 217 MB static libraries |
| Release build time | 9 min 52 s wall (114 min CPU) |
| Debug build | not measured; expect several GB (debug info for ~1.5 M lines) |

Recommendation:
- Add it as a **shallow submodule pinned to a release tag** (`.gitmodules`: `shallow = true`,
  checked out at `V7_9_3`), so a clone costs about 350 MB, not the full history.
- Build OCCT **Release (or RelWithDebInfo) even in Debug builds** of NG, through
  `add_subdirectory` with only the four modules above; it is never stepped through, and a
  Debug OCCT build is several times larger and slower.
- Desktop only (`ENABLE_OCCT`, default on except Emscripten), so the web build is unchanged.
- Licence: LGPL 2.1 with the OCCT exception (about using its headers), allowed by AGENTS.md
  section 2. Static linking under the LGPL needs users to be able to relink; shipping NG's
  complete GPLv3 source with OCCT's satisfies that. OCCT's notices ship with the binaries.
