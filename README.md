# UtilHTTPClient_libcurl

[简体中文](README.zh-CN.md)

UtilHTTPClient_libcurl is a standalone libcurl HTTP client. It implements the
`IUtilHTTPClient` API on libcurl and provides synchronous request objects, asynchronous
requests, streamed responses and an optional shared cookie container. Transfers are
driven by `RunFrame()`.

* Windows MSVC x86 only, built with Visual Studio 2022, C++20 and VC-LTL 5.3.1.
* Exports `CreateInterface`, plus the `UtilHTTPClient_libcurl_007` and
  `UtilHTTPClientFactory_libcurl_007` interfaces.
* Shared libcurl with Windows Schannel and HTTPS; no external compression, HTTP/2, SSH,
  PSL or IDN2 dependencies.

## Quick start

Download `UtilHTTPClient_libcurl-windows-x86.7z` from
[GitHub Releases](https://github.com/MetaHookSv/UtilHTTPClient_libcurl/releases) (built on `v*` tag pushes)
and copy its contents into the game directory:

* `libcurl.dll` (`libcurl-d.dll` for Debug), `UtilHTTPClient_libcurl.dll` and its PDB under
  `svencoop/metahook/dlls`.

The archive ships the runtime payload only. The public headers live in the repository's
`include/Interface` (and the MetaHook SDK's `include/HLSDK/common`); add both to a
consumer's include paths.

Load the DLL through `CreateInterface`, request
`UTIL_HTTPCLIENT_FACTORY_LIBCURL_INTERFACE_VERSION`, then call `CreateUtilHTTPClient()` and
`Init()` with a valid `CUtilHTTPClientCreationContext`. This is a consumer-loaded utility
DLL; it needs no `plugins.lst` entry. Keep pumping `RunFrame()` to drive transfers and to
release finished pooled requests — `WaitForComplete()` does not drive I/O.

## Build

Windows with Visual Studio 2022 C++ x86 tools and the Windows SDK, CMake 3.21 or newer, Git
and PowerShell. Debug and Release both use C++20, the static MSVC CRT and VC-LTL 5.3.1.

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_libcurl
cd UtilHTTPClient_libcurl
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat
```

Each script configures, builds, runs CTest, installs and verifies the installed DLL,
stopping on failure. `BUILD_TESTING` defaults to `ON`; use `-DBUILD_TESTING=OFF` for a
library-only build. The first configure fetches curl, the MetaHook interface code and
ScopeExit at pinned commits; pass `-DCURL_SOURCE_PATH=... -DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
to reuse local checkouts for offline builds. `scripts/Package-Windows.ps1` produces the
distributable archive.

## Known limitations

* The URL parser matches case-insensitively but derives defaults from case-sensitive
  scheme comparisons, sets the secure flag only when no explicit port was supplied, and
  rejects IPv6 literals. Use lowercase HTTPS URLs without an explicit port.
* When a transfer fails before any response header, the completion callback still fires
  while `IsFinished()` may stay false; such async requests need manual pool cleanup.

## License

MIT; see [LICENSE](LICENSE). Third-party licenses are under `licenses/` in the repository;
the release archive ships the runtime payload only.
