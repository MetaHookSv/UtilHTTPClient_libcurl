# UtilHTTPClient_libcurl

从 [MetaHookSv](https://github.com/hzqst/MetaHookSv) 提取的独立 libcurl HTTP 客户端，
支持同步请求对象、异步请求、流式响应及可选的共享 Cookie 容器。网络传输由 `RunFrame()` 推进。

保留 `UtilHTTPClient_libcurl.dll`、`CreateInterface` 导出、
`UtilHTTPClient_libcurl_007` 与 `UtilHTTPClientFactory_libcurl_007` ABI。
源码基线为 `hzqst/MetaHookSv@fe80b6d60bfb487b52aed7ea7ec0492e7b27a5d2` 的
`PluginLibs/UtilHTTPClient_libcurl` 和 `include/Interface/IUtilHTTPClient.h`。
请求实现保持原行为，仅补充显式标准头文件。[English](README.md)

## 构建与测试

需要 Windows、Visual Studio 2022 的 C++ x86 工具与 Windows SDK、CMake 3.21+、Git、PowerShell。
仅支持 Windows MSVC x86。Debug/Release 均采用 C++20、静态 CRT（`/MTd`、`/MT`）与 VC-LTL 5.3.1。

```bat
scripts\build-UtilHTTPClient_libcurl-x86-Debug.bat
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat
```

脚本依次配置、构建、运行 CTest、安装，并将测试宿主暂时放到安装根目录验证运行库搜索路径。
任何步骤失败都会停止。输出位于 `build/x86/<Configuration>` 与 `install/x86/<Configuration>`。

等效 CMake 命令（不包含安装后的运行验证）：

```bat
cmake -S . -B build/x86/Release -G "Visual Studio 17 2022" -A Win32 -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

`BUILD_TESTING` 默认开启，可用 `-DBUILD_TESTING=OFF` 仅构建库。
其他生成器也必须使用 MSVC x86；单配置生成器必须指定 `CMAKE_BUILD_TYPE=Debug` 或 `Release`。

## 固定依赖

首次配置自动获取以下依赖，不初始化其子模块，也不构建 MetaHook 启动器：

| 依赖 | 来源 | 提交或版本 |
| --- | --- | --- |
| curl | `curl/curl` | `4f95f327093bef29a4f8fe188edc6c95f49980a5` |
| MetaHook 接口代码 | `MetaHookSv/MetaHook` | `4d23b6fecd79dc949aabc2e145480cd1328d4a35` |
| ScopeExit | `SergiusTheBest/ScopeExit` | `bd345da594a4675d04de663d93d00cb81b6678b2` |
| VC-LTL 二进制包 | `Chuyu-Team/VC-LTL5` | `5.3.1` |

curl 提交与原仓库 `thirdparty/curl` 的 gitlink 一致。
VC-LTL 下载到 `thirdparty/cache`，SHA-256 必须为
`7a18799ed3aa84a225610a5447a56bc534c5c98ccb8dec05caba0e3f633431ad`。

两种配置均构建共享 libcurl，启用 Windows Schannel 和 HTTPS，修正原 Release 构建关闭 SSL 的配置。
沿用原依赖配置，关闭可选压缩、HTTP/2、SSH、PSL 和 IDN2 外部依赖，保留 Windows IDN 支持。

本地或离线构建可提供现有源码和已解压的 VC-LTL 二进制包：

```bat
scripts\build-UtilHTTPClient_libcurl-x86-Release.bat -DMETAHOOK_SOURCE_PATH=D:\MetaHookSv -DCURL_SOURCE_PATH=D:\MetaHookSv\thirdparty\curl -DSCOPEEXIT_SOURCE_PATH=D:\MetaHookSv\thirdparty\ScopeExit -DVC_LTL_Root=D:\VC-LTL-5.3.1
```

这些变量也接受环境变量作为 CMake 缓存初始值，显式缓存参数优先。外部目录保持只读。
`CURL_SOURCE_PATH` 必须指向固定提交的干净 Git 仓库根目录；不同提交或已修改的受跟踪源码会被拒绝，
不接受没有 Git 元数据的 curl 压缩包目录。可用 `UTILHTTPCLIENT_DEPENDENCY_CACHE_DIR` 指定 VC-LTL 缓存位置。

## 安装与打包

将 `install/x86/Release` 内容复制到 Sven Co-op 游戏目录：

- `libcurl.dll` 放在游戏可执行文件旁；Debug 对应 `libcurl-d.dll`。
- 客户端 DLL/PDB 放在 `svencoop/metahook/dlls`。
- 头文件位于 `include/Interface` 和 `include/HLSDK/common`，使用者需将两者加入包含路径。

客户端由使用它的模块加载，无须加入 `plugins.lst`。
Release 构建后执行 `powershell -NoProfile -File scripts/Package-Windows.ps1`（需 `7z` 在 PATH 中），
生成并校验 `build/artifacts/UtilHTTPClient_libcurl-windows-x86.7z`，包含运行库、客户端 PDB、
公共头文件、中英文文档及所有安装的许可证声明。

## 接口与生命周期

加载 DLL，取得 `CreateInterfaceFn`，请求 `UTIL_HTTPCLIENT_FACTORY_LIBCURL_INTERFACE_VERSION`，
调用 `CreateUtilHTTPClient()`，随后用有效的 `CUtilHTTPClientCreationContext` 调用 `Init()`。
也可通过 `UTIL_HTTPCLIENT_LIBCURL_INTERFACE_VERSION` 直接创建客户端。

- 请求配置后调用 `Send()`，持续调用 `RunFrame()` 推进传输。同步请求同样需要推进，等待函数不会驱动网络。
- 请求持有回调对象，并在释放时调用回调的 `Destroy()`；响应只在请求存活期间有效。
- 异步请求加入池后，由 `RunFrame()` 自动释放已完成且开启自动销毁的请求。跨帧保存 ID，避免保存原始指针。
  关闭自动销毁的请求应使用 `DestroyRequestById()` 释放。
- 流式响应通过 `OnReceiveData()` 交付，响应体不累计到 Payload；流式回调中可以查询响应头。
- 手动管理的请求应先释放，再调用客户端 `Shutdown()` 和 `Destroy()`。卸载 DLL 前结束全部活动并销毁全部客户端。
  不要在请求回调中销毁当前请求或关闭客户端。

### 保留的源码行为

URL 正则匹配忽略大小写，但默认端口和安全标志使用区分大小写的协议比较；安全标志仅在未指定端口时设置。
因此，大写协议或显式带端口的 HTTPS URL 可能得到错误默认值或转为 HTTP。请使用小写、未显式指定端口的 HTTPS URL。
解析器不支持 IPv6 字面量；`ParseUrl()` 和创建请求都使用该解析器。

收到响应头之前发生传输错误时，会交付响应完成回调并唤醒同步等待，但 `IsFinished()` 可能仍为 false：
原实现的完成状态转换要求先进入 Responding。此类异步请求应在错误回调结束后手动清理池。
这些既有行为在本次迁移中仅记录，不修改。

## 验证与 CI

九个 CTest 场景通过实际 DLL 的公共接口检查工厂、URL、GET、POST/自定义头、异步请求池、流式响应、
HTTP 错误、连接拒绝及 SSL/HTTPS/Schannel 能力。测试使用 C++ 回环服务，不依赖公网或游戏。
TLS 能力检查不等于 HTTPS 握手或证书验证测试。

GitHub Actions 在 `windows-2022` 上构建并验证 Debug/Release 与安装布局。
main push、PR、手动运行上传 Release 压缩包；`v*` 标签通过相同门禁后创建 GitHub Release。

## 许可证

MIT，参见 [LICENSE](LICENSE) 与 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)。
