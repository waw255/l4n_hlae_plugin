# l4n_hlae_plugin

> [!IMPORTANT]
> 需要 **L4N v2.34.0 或更高版本**。低于此版本的 L4N 可能无法正确加载本插件。

`l4n_hlae_plugin` v0.1 是面向 32 位 Left 4 Dead 2 的 L4N 原生插件，将 HLAE Source 1 作为 L4N 插件加载。

> [!WARNING]
> 本项目由 AI 开发和整理。请自行审查源码、构建结果和运行行为，注意甄别。建议只在本地或离线环境使用，不要带实验性 DLL 连接 VAC 保护的服务器。

## 玩家安装

### 编译项目

本项目不提交预编译 DLL。克隆仓库后，按“开发者编译”章节编译。成功后，根目录会生成可直接安装的 `Release` 文件夹。

### 复制文件

关闭游戏，把 `Release` 文件夹**里面的内容**复制到 Left 4 Dead 2 游戏根目录并合并覆盖，不要把 `Release` 文件夹本身复制进去：

```text
Release/bin/neko/plugins/l4n_hlae_plugin.dll
Release/bin/neko/plugins/l4n_hlae_plugin.ini
Release/l4n_hlae_core/...
```

安装后应为：

```text
<Left 4 Dead 2>/bin/neko/plugins/l4n_hlae_plugin.dll
<Left 4 Dead 2>/bin/neko/plugins/l4n_hlae_plugin.ini
<Left 4 Dead 2>/l4n_hlae_core/...
```

`l4n_hlae_plugin.ini` 中的路径必须保持为相对路径：

```ini
[HLAE]
HlaeRoot=..\..\..\l4n_hlae_core
```

该路径相对于 `bin/neko/plugins/` 解析。不要将 `HlaeRoot` 改成开发机上的绝对路径，也不要把 `l4n_hlae_core` 改名或移动。

### 启动游戏

在游戏启动项中添加：

```text
-l4n_hlae
```

L4N Mod 通常会附带 `left4dead2 no-insecure.bat`。打开该脚本，在原有启动参数末尾追加 `-l4n_hlae`，例如：

```bat
start left4dead2.exe -steam -novid -l4n_hlae
```

保留脚本原有参数；本插件不要求 `-insecure`。未添加 `-l4n_hlae` 时不会初始化 HLAE，并会在控制台输出：

```text
[WARNING] L4N_HLAE plugin is not started. Add -l4n_hlae to the game launch options to start the plugin.
```

### 验证

进入本地地图后，在控制台检查 HLAE 命令，例如：

```text
mirv_streams
mirv_time
__mirv_info
```

日志位于 `%TEMP%\l4n_hlae_plugin.log`。如果插件或资源找不到，先检查 `bin/neko/plugins/` 和 `l4n_hlae_core/` 的目录结构。

使用 `mirv_input camera` 时，Demo Playback 界面显示鼠标后，鼠标可以自由移动和点击；按住右键可暂时旋转视角，松开右键后恢复鼠标操作。

## 开发者编译

### 环境

- Windows、Git。
- CMake 4.2.1 或更新版本；使用 `Visual Studio 18 2026` 生成器。
- Visual Studio 2026 的 `Desktop development with C++`、x86 MSVC 和 Windows SDK。
- `.NET Framework 4.6.2 Targeting Pack`，供 HLAE ShaderBuilder 使用。
- 首次配置需要访问 GitHub，以下载 protobuf、OpenEXR、Imath 和 Abseil 等依赖。

### 配置和编译

在项目根目录打开 PowerShell：

```powershell
cmake -S . -B build-release -G "Visual Studio 18 2026" -A Win32 `
  -DL4N_BUILD_HLAE_PLUGIN=ON `
  -DL4N_BUNDLE_OPENEXR=ON `
  -DL4N_PACKAGE_RELEASE=ON `
  "-DL4N_SHADERBUILDER_FRAMEWORK_VERSION=v4.6.2"

cmake --build build-release --config Release --target l4n_release_package --parallel 1
```

`L4N_BUNDLE_OPENEXR=ON` 会将 OpenEXR 静态链接进插件，安装时不需要额外复制 OpenEXR DLL。`--parallel 1` 用于避免 protobuf 外部项目在同一构建目录发生并发访问。

如果 CMake 找不到 Visual Studio，在配置命令中加入项目变量：

```powershell
"-DL4N_VS_INSTALL_PATH=C:/Path/To/Visual/Studio/18/Community"
```

该变量会自动查找 Visual Studio 下的 `MSBuild.exe`，不需要手动设置 HLAE 内部变量。路径中的反斜杠建议改为正斜杠。

如果出现 `MSB8066`、`protobuf-populate` 或 `FetchContent` 错误，关闭其他 CMake/MSBuild 进程，删除未完成的 `build-release`，重新配置后使用上面的串行编译命令。

### 静态检查

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_plugin.ps1 `
  -PluginPath .\Release\bin\neko\plugins\l4n_hlae_plugin.dll
```

检查应通过以下项目：

- PE Machine 为 `0x014C`（x86）。
- 导出表包含 `GetL4NPluginInstance`。
- 不依赖原始 `AfxHookSource.dll`。
- 不依赖外部 OpenEXR DLL。

不要将 `x64` 产物部署到 L4N，也不要把 `build-*` 或 `Release` 目录提交到 Git。

## 更新 HLAE 源码

仓库中的 `advancedfx/` 是已经加入 L4N 改动的 HLAE 源码快照，版本记录在 `advancedfx-source-revision.txt`。升级时先在仓库外获取上游版本：

```powershell
git clone https://github.com/advancedfx/advancedfx.git ..\advancedfx-upstream
git -C ..\advancedfx-upstream checkout <commit-or-tag>
git -C ..\advancedfx-upstream submodule update --init --recursive
git -C ..\advancedfx-upstream rev-parse HEAD
```

比较上游源码后再合并，不能直接覆盖 `advancedfx/`。必须重新检查并移植 L4N 集成涉及的 CMake、`l4n_plugin.*`、`RenderView.cpp` 和 `MirvInput.*` 改动，然后更新 `advancedfx-source-revision.txt`，重新执行 Win32 Release 编译、静态检查和游戏内测试。不要复制上游 `.git`、构建目录或预编译二进制。

## 运行注意事项

- 正式插件文件名为 `l4n_hlae_plugin.dll`，版本为 `v0.1`。
- 不要同时安装旧版 `LoadLibraryExW` 适配器、原始 `AfxHookSource.dll`、`injector.exe` 或 `AfxHook.dat`。
- 更新前备份 `bin/neko/plugins/l4n_hlae_plugin.dll` 和配置文件。
- 如果游戏崩溃、卡死、渲染异常或 HLAE 命令缺失，停止使用并保留 `%TEMP%\l4n_hlae_plugin.log`。
- `l4n_hlae_core` 只包含本插件需要的 HLAE 资源，不是完整的 HLAE 独立安装包。

## 项目结构

```text
advancedfx/                  改造后的 HLAE 源码
config/                      配置模板
include/                     L4N ABI 头文件
src/                         旧适配器实验基线
cmake/package_release.cmake  Release 安装包生成脚本
tools/                       构建辅助和 PE 检查脚本
```

构建输出、日志、预编译 DLL 和本地配置均已加入 `.gitignore`。

## 许可证

本项目新增的 L4N 集成代码、构建脚本和项目配置使用 [MIT License](LICENSE)。

本项目包含并改造了 [advancedfx/advancedfx](https://github.com/advancedfx/advancedfx)。`advancedfx/` 内的 HLAE 源码继续遵循其原始许可证、版权和归属文件；HLAE 的授权条件不因本项目的 MIT License 改变。
