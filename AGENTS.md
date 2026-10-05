# AGENTS.md

This file provides guidance and important rules working with code in this repository.

## When coding / building plan

- Use a progressive disclosure approach for agent coding in this repository: start from high-level
  information in the Basic Memory knowledge base first, and only locate/read specific files or
  symbols when necessary, instead of expanding a large amount of context at once.

#### Basic Memory knowledge base (project-scoped, `memory/`)

- Notes live in `memory/` (markdown with YAML frontmatter: `title`/`type`/`permalink`), tracked in git.
- This repository contains the standalone UtilHTTPClient_libcurl library, extracted from MetaHookSv
  `PluginLibs/UtilHTTPClient_libcurl`. Its notes were migrated from MetaHookSv and adapted to the
  CMake workspace; see `memory/project_overview.md` for scope and provenance.
- Notes use the `utilhttpclient-libcurl/` permalink prefix to distinguish them from the source
  repository.

#### High-level information in this repository (read corresponding notes first)

- Project overview, provenance, request lifecycle, dependency pins and known limitations:
  `project_overview`

#### When notes are insufficient: source entry points (query and read on demand)

- Build: `CMakeLists.txt`, `cmake/Curl.cmake` (libcurl feature set and options),
  `cmake/Dependencies.cmake` (pins and source-path overrides), `cmake/VCLTL.cmake`,
  `scripts/build-UtilHTTPClient_libcurl-x86-{Debug,Release}.bat`, `scripts/Package-Windows.ps1`
- Library sources: `src/UtilHTTPClient_libcurl.cpp` (client, request family, response family, URL
  parsing and the exports at the end of the file), `src/dllmain.cpp`
- Public API / interface: `include/Interface/IUtilHTTPClient.h` — shared with the SteamAPI backend,
  so a change here affects both repositories
- Tests: `tests/RegressionTests.cpp`, run by CTest (`BUILD_TESTING` defaults to `ON` in the scripts)
- Docs: `README.md`, `README.zh-CN.md`, third-party terms under `licenses/`
- External sources, all read-only inputs: `CURL_SOURCE_PATH`, `METAHOOK_SOURCE_PATH`,
  `SCOPEEXIT_SOURCE_PATH` and `VC_LTL_Root`; pins are fetched only when the corresponding override is
  empty. The build uses C++20, a static CRT and VC-LTL 5.3.1, for MSVC x86 only
- Build output: `build/x86/<configuration>/`; install output: `install/x86/<configuration>/`. Neither
  is tracked, and nothing is deployed into a game

#### Progressive disclosure key points

- Read notes first, then locate a single file/symbol; do not read the whole repository at once.
- Prefer correctly scoped Basic Memory MCP tools for knowledge retrieval; otherwise use the local
  notes before reading source.
- Prefer Context7 for external dependency/library usage (query on demand).

## Repository rules

- Preserve the exported interface versions (`UtilHTTPClient_libcurl_007` and
  `UtilHTTPClientFactory_libcurl_007`), the `IUtilHTTPClient` vtable layout and the shared header's
  ABI unless an interface change is explicitly requested; the SteamAPI backend and its consumers must
  keep compiling against the same header.
- Keep the callback-ownership contract (the request destroys its `IUtilHTTPCallbacks`) and the
  "`RunFrame()` drives everything" model intact. Do not make `WaitForComplete()` drive I/O on its own
  without documenting the behaviour change in the note and READMEs.
- Keep the libcurl feature set deliberate: Schannel/HTTPS on, and external compression, HTTP/2, SSH,
  PSL and IDN2 off. Changing `cmake/Curl.cmake` changes the shipped `libcurl.dll`.
- Known limitations are contracts, not bugs to silently fix: the asymmetric URL parse, the
  failure-before-header async case that needs manual pool cleanup, and the stub async wait/response
  methods. If one is fixed, update `memory/project_overview.md` and both READMEs in the same change.
- Do not modify external dependency trees or fetched vendor sources. Explicit source trees are
  read-only inputs and must never be downloaded into or modified.
- Run both x86 Debug and Release build scripts for relevant changes; they configure, build, run
  CTest, install and verify the installed DLL. Keep tests focused on public behavior.
- Build, install and dependency-cache directories are ignored. Do not commit, push or publish
  without authorization.