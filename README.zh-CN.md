# UtilHTTPClient_libcurl

[English](README.md)

独立的 libcurl HTTP 客户端，在 libcurl 之上实现 `IUtilHTTPClient` API，支持同步请求对象、
异步请求、流式响应及可选的共享 Cookie 容器，网络传输由 `RunFrame()` 推进。

* 仅支持 Windows MSVC x86，使用 Visual Studio 2022、C++20 和 VC-LTL 5.3.1 构建。
* 导出 `CreateInterface`，以及 `UtilHTTPClient_libcurl_007` 和
  `UtilHTTPClientFactory_libcurl_007` 两个接口。
* 共享 libcurl 启用 Windows Schannel 和 HTTPS，不含压缩、HTTP/2、SSH、PSL、IDN2 等外部依赖。

## 快速开始

从 [GitHub Releases](https://github.com/MetaHookSv/UtilHTTPClient_libcurl/releases)
下载 `UtilHTTPClient_libcurl-windows-x86.7z`（由 `v*` 标签推送构建），将内容复制到游戏目录：

* `libcurl.dll`（Debug 为 `libcurl-d.dll`）、`UtilHTTPClient_libcurl.dll` 与 PDB 放在
  `svencoop/metahook/dlls`。

压缩包仅含运行时负载。公共头文件位于仓库的 `include/Interface`（以及 MetaHook SDK 的
`include/HLSDK/common`），使用者需将两者加入包含路径。

加载 DLL 的 `CreateInterface`，请求 `UTIL_HTTPCLIENT_FACTORY_LIBCURL_INTERFACE_VERSION`，
调用 `CreateUtilHTTPClient()`，再用有效的 `CUtilHTTPClientCreationContext` 调用 `Init()`。
此工具库由消费者加载，无需添加 `plugins.lst` 条目。需持续调用 `RunFrame()` 推进传输并回收
已完成的池内请求；等待完成不会驱动网络。

## 构建

需要 Windows、Visual Studio 2022 C++ x86 工具和 Windows SDK、CMake 3.21 以上、Git 及 PowerShell。
Debug、Release 均使用 C++20、静态 MSVC CRT 及 VC-LTL 5.3.1。

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/UtilHTTPClient_libcurl
cd UtilHTTPClient_libcurl
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat
```

每个构建脚本依次配置、构建、执行 CTest、安装并验证安装后的 DLL，失败立即退出。
`BUILD_TESTING` 默认开启，`-DBUILD_TESTING=OFF` 可只构建库。首次配置会按固定提交获取 curl、
MetaHook 接口代码和 ScopeExit；可通过
`-DCURL_SOURCE_PATH=... -DMETAHOOK_SOURCE_PATH=... -DSCOPEEXIT_SOURCE_PATH=... -DVC_LTL_Root=...`
复用本地检出目录进行离线构建。`scripts/Package-Windows.ps1` 用于生成发布压缩包。

## 已知限制

* URL 解析匹配忽略大小写，但默认端口和安全标志使用区分大小写的协议比较，且仅在未指定端口时设置
  安全标志，同时不支持 IPv6 字面量；请使用小写、未显式指定端口的 HTTPS URL。
* 收到响应头之前发生传输错误时，完成回调仍会触发，但 `IsFinished()` 可能保持 false；
  此类异步请求需手动清理请求池。

## 许可证

MIT，见 [LICENSE](LICENSE)。第三方许可证位于仓库的 `licenses/` 目录，发布压缩包仅含运行时负载。

## C/C++ 格式化

使用 [MetaHookSv/FormatValidation](https://github.com/MetaHookSv/FormatValidation)
共享工具及固定版本 **clang-format 23.1.3**，采用 DiligentCore 风格（4 空格，保留
include 顺序）。为 CMake 使用的 Python 解释器安装格式工具：

```sh
python -m pip install clang-format==23.1.3
cmake -S . -B build/format "-DFORMAT_VALIDATION_ONLY=ON"
cmake --build build/format --target format-check
cmake --build build/format --target format
```

格式专用配置需要 CMake 3.21+、Git、Python 3.9+（CI 使用 3.12）及构建生成器；
使用 `-G Ninja` 可无需 Visual Studio。它不准备原生 SDK 或游戏依赖。
格式目标需显式执行，不加入普通 DLL 构建。使用 Visual Studio 生成器时，执行目标
需追加 `--config Debug` 或 `--config Release`。

聚合仓库注入 `FORMAT_VALIDATION_SOURCE_PATH=thirdparty/FormatValidation`。
独立组件支持该 CMake 参数及同名环境变量；为空时通过 FetchContent 获取固定工具
提交。相对路径应加引号，例如
`"-DFORMAT_VALIDATION_SOURCE_PATH=../../thirdparty/FormatValidation"`。
配置时在仓库根目录生成被 gitignore 的 `.clang-format` 供编辑器使用；格式规则应
在共享仓库修改，不修改生成副本。可通过 `FORMAT_VALIDATION_CLANG_FORMAT_EXECUTABLE`
指定工具路径，但版本仍须与固定版本一致。

检查覆盖 `src/`、`include/`、`tests/` 中维护的 C/C++ 文件，包括未被 Git 忽略的新文件。
相对仓库根目录的排除规则位于 `.clang-format-ignore`；第三方源和构建产物不纳入检查。
`clang-format` workflow 在 push、pull request 和手动运行时执行全量检查。
