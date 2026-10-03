# UtilHTTPClient_libcurl

A standalone libcurl HTTP client extracted from [MetaHookSv](https://github.com/hzqst/MetaHookSv).
It supports synchronous request objects, asynchronous requests, streamed responses,
and an optional shared cookie container. Transfers are driven by `RunFrame()`.

The port preserves `UtilHTTPClient_libcurl.dll`, the `CreateInterface` export,
`UtilHTTPClient_libcurl_007`, and `UtilHTTPClientFactory_libcurl_007`.
Source baseline: `hzqst/MetaHookSv@fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2`,
`PluginLibs/UtilHTTPClient_libcurl` and `include/Interface/IUtilHTTPClient.h`.
The original request implementation is retained; explicit standard headers were added.

[简体中文](README.zh-CN.md)

## Build and test

Requirements: Windows, Visual Studio 2022 with the C++ x86 tools and Windows SDK,
CMake 3.21 or newer, Git, and PowerShell. The library supports Windows MSVC x86 only.
Debug and Release use C++20, static MSVC CRT (`/MTd` or `/MT`), and VC-LTL 5.3.1.

```bat
scripts\build-UtilHTTPClient_libcurl-x86-Debug.bat
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat
```

Each script configures, builds, runs CTest, installs, and tests the installed DLL
from a host executable placed temporarily in the game root. Every step stops on failure.
Outputs are under `build/x86/<Configuration>` and `install/x86/<Configuration>`.

Equivalent CMake commands (these omit the installed runtime test):

```bat
cmake -S . -B build/x86/Release -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

`BUILD_TESTING` defaults to `ON`; use `-DBUILD_TESTING=OFF` for a library-only build.
Other generators must also select MSVC x86; single-configuration generators require
`-DCMAKE_BUILD_TYPE=Debug` or `Release`.

## Pinned dependencies

The first configure fetches source dependencies without their submodules:

| Dependency | Source | Commit/version |
| --- | --- | --- |
| curl | `curl/curl` | `4f95f327093bef29a4f8fe188edc6c95f49980a5` |
| MetaHook interface code | `MetaHookSv/MetaHook` | `4d23b6fecd79dc949aabc2e145480cd1328d4a35` |
| ScopeExit | `SergiusTheBest/ScopeExit` | `bd345da594a4675d04de663d93d00cb81b6678b2` |
| VC-LTL binary package | `Chuyu-Team/VC-LTL5` | `5.3.1` |

The curl commit matches the original MetaHookSv `thirdparty/curl` gitlink.
VC-LTL is downloaded to `thirdparty/cache` and verified against SHA-256
`7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`.
MetaHook is used only for `interface.h` and `interface.cpp`; its launcher is not built.

Both configurations build shared libcurl with Windows Schannel and HTTPS enabled.
This corrects the original Release script's disabled SSL setting. Optional external
compression, HTTP/2, SSH, PSL, and IDN2 dependencies are disabled, matching the original
dependency profile. Windows IDN support is retained.

For local/offline builds, pass existing source trees and an extracted VC-LTL package:

```bat
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHookSv -DCURL_SOURCE_PATH=D:\MetaHookSv\thirdparty\curl -DSCOPEEXIT_SOURCE_PATH=D:\MetaHookSv\thirdparty\ScopeExit -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

These names are also accepted as environment variables when initializing the CMake cache;
explicit cache values take precedence. All external directories remain read-only.
`CURL_SOURCE_PATH` must be the root of a clean Git checkout at the exact pinned commit;
different commits or tracked modifications fail configuration. Archives without Git
metadata are not accepted as local curl overrides. `UTILHTTPCLIENT_DEPENDENCY_CACHE_DIR`
overrides the VC-LTL package cache.

## Installation and packaging

Copy the contents of `install/x86/Release` into the Sven Co-op game directory:

- `libcurl.dll` goes next to the game executable.
- `UtilHTTPClient_libcurl.dll` and its PDB go under `svencoop/metahook/dlls`.
- Public headers are under `include/Interface` and `include/HLSDK/common`; add both
  directories to a consumer's include paths.

Debug installs `libcurl-d.dll` instead. The library is loaded by its consumers and
does not need an entry in `plugins.lst`.

After building Release, run `powershell -NoProfile -File scripts/Package-Windows.ps1`
(requires `7z` on PATH). The verified archive is
`build/artifacts/UtilHTTPClient_libcurl-windows-x86.7z`; it includes both runtime DLLs,
the client PDB, public headers, documentation, and all installed license notices.

## Public interface and lifecycle

Load the DLL, resolve `CreateInterface` as `CreateInterfaceFn`, and request
`UTIL_HTTPCLIENT_FACTORY_LIBCURL_INTERFACE_VERSION`. Call `CreateUtilHTTPClient()`
and then `Init()` with a valid `CUtilHTTPClientCreationContext`.
The direct `UTIL_HTTPCLIENT_LIBCURL_INTERFACE_VERSION` interface is also retained.

- Create a request, configure it, and call `Send()`. Pump `RunFrame()` until completion.
  Synchronous request objects also need this pump; `WaitForComplete()` does not drive I/O.
- A request owns its callback object and calls `Destroy()` when the request is destroyed.
  Completion callbacks receive a response valid for the request's lifetime.
- Async requests auto-destroy only when placed in the pool and observed as finished by
  `RunFrame()`. Store pool IDs across frames instead of raw pointers. Disable auto-destroy
  when retaining a request, then release it with `DestroyRequestById()`.
- Streaming responses arrive through `OnReceiveData()` and are not accumulated in the
  response payload. Headers are available during stream callbacks.
- Destroy manually managed requests before `Shutdown()`; then call client `Destroy()`.
  Finish all activity and destroy all clients before unloading the DLL. Do not destroy a
  request or shut down the client from inside its callbacks.

### Existing behavior retained by the port

The custom URL parser is case-insensitive when matching, but determines defaults using
case-sensitive scheme comparisons. It sets the secure flag only when no explicit port
was supplied. Consequently uppercase schemes and HTTPS URLs with an explicit port can
select incorrect defaults or HTTP; use lowercase HTTPS URLs without an explicit port.
The parser does not support IPv6 literals. Both `ParseUrl()` and request creation use it.

For transport failures before any response header, the response completion callback and
sync wait signal are delivered, but `IsFinished()` may remain false: the original finish
transition is guarded by the responding state. Such async requests require manual pool
cleanup after the error callback. These source behaviors are documented rather than changed.

## Verification and CI

Nine CTest scenarios load the actual DLL and cover factory lookup, URL parsing, GET,
POST/custom headers, async pool ownership, streamed responses, HTTP errors, connection
refusal, and libcurl SSL/HTTPS/Schannel capabilities. A C++ loopback HTTP server avoids
public network endpoints and game dependencies. The TLS capability check is not an HTTPS
handshake or certificate-validation test.

GitHub Actions builds, tests, and checks installation for Debug and Release on
`windows-2022`. Main pushes, pull requests, and manual runs upload the Release archive.
Pushing a `v*` tag runs the same gates and publishes the archive as a GitHub Release.

## License

MIT; see [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
