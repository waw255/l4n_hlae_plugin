# l4n_hlae_plugin

`l4n_hlae_plugin` v0.1 是面向 Left 4 Dead 2 的 L4N 原生插件。它将 HLAE Source 1 的 `AfxHookSource` 改造成符合 L4N v1 ABI 的 x86 DLL，由 L4N 在游戏进程内直接加载。

本项目复用 HLAE 已有的模块处理和导入钩子实现，不调用 `injector.exe`，不使用 `AfxHook.dat`、远程线程、手动映射或额外注入器。OpenEXR 默认静态链接到插件中，安装包不需要另行复制 OpenEXR DLL。

> [!WARNING]
> 本项目由 AI 开发和整理。代码、构建结果以及运行时行为请自行审查和验证，注意甄别，不应只依据本项目说明判断安全性或兼容性。HLAE、L4N、游戏更新和其他 Mod 都可能改变实际行为。
> 同时注意：L4N 2.34.0版本才加入了插件系统，要求你L4N的版本达到2.34.0才能正常使用插件

## 功能和限制

- 目标平台是 32 位 Left 4 Dead 2、x86 L4N 和 x86 HLAE Source 1。
- 正式插件文件名是 `l4n_hlae_plugin.dll`，插件版本是 `v0.1`。
- 只有游戏启动参数包含 `-l4n_hlae` 时才初始化 HLAE。
- 未包含该参数时，插件不会初始化，并尝试在游戏控制台输出：

  ```text
  [WARNING] L4N_HLAE plugin is not started. Add -l4n_hlae to the game launch options to start the plugin.
  ```

- 当前插件不要求 `-insecure` 参数。仍建议只在本地、离线或确认安全的环境中使用，不要带着实验性 DLL 进入 VAC 保护的在线环境。
- 这是实验性的兼容层。游戏、L4N 或 HLAE 更新后，应重新检查插件加载、进入地图、渲染和 HLAE 命令。

## 安装

### 1. 克隆并编译

克隆项目后，按本文的“编译环境”和“编译”章节执行命令。成功编译后，项目根目录会生成可直接复制的 `Release` 文件夹：

```text
Release/
  bin/neko/plugins/
    l4n_hlae_plugin.dll
    l4n_hlae_plugin.ini
  l4n_hlae_core/
    resources/
      hexfont.tga
      shaders/*.acs
      ...
    LICENSE
    CREDITS.md
```

### 2. 复制到游戏根目录

关闭游戏，将 `Release` 文件夹里面的 `bin` 和 `l4n_hlae_core` 文件夹连同目录结构一起复制到 Left 4 Dead 2 游戏根目录，并合并覆盖。不要把 `Release` 文件夹本身复制成游戏目录下的 `Release` 文件夹。

安装完成后的关键路径应为：

```text
<Left 4 Dead 2 游戏根目录>/bin/neko/plugins/l4n_hlae_plugin.dll
<Left 4 Dead 2 游戏根目录>/bin/neko/plugins/l4n_hlae_plugin.ini
<Left 4 Dead 2 游戏根目录>/l4n_hlae_core/resources/hexfont.tga
```

配置文件中的 `HlaeRoot` 必须保持为相对路径：

```ini
HlaeRoot=..\..\..\l4n_hlae_core
```

该路径相对于 `bin/neko/plugins/` 解析，最终指向游戏根目录下的 `l4n_hlae_core/`。安装时不要把它改成构建机上的绝对路径。

### 3. 使用 L4N 启动脚本

一般安装 L4N Mod 后会附带 `left4dead2 no-insecure.bat` 启动脚本。打开这个脚本，在原有启动参数末尾增加 `-l4n_hlae`，例如：

```bat
start left4dead2.exe -steam -novid -l4n_hlae
```

请保留脚本中已有的 L4N 参数，只追加 `-l4n_hlae`。这样以后直接运行该脚本即可启动插件。L4N 更新可能覆盖脚本，更新后需要重新确认这个参数仍然存在。

## 验证安装

启动游戏后依次确认：

1. L4N 能发现并加载 `l4n_hlae_plugin.dll`。
2. 启动参数中包含 `-l4n_hlae`。
3. `%TEMP%\l4n_hlae_plugin.log` 中出现插件初始化和 HLAE 模块处理记录。
4. 进入本地地图后，在游戏控制台检查源码中存在的命令：

   ```text
   mirv_streams
   mirv_time
   __mirv_info
   ```

如果没有使用 `-l4n_hlae`，控制台应出现上文的英文警告；这时 HLAE 不会初始化。如果日志提示 DLL 或资源找不到，先确认 `bin/neko/plugins/` 与 `l4n_hlae_core/` 的相对位置没有改变。

## 编译环境

需要在 Windows 上准备以下组件：

- Git。
- 支持 `Visual Studio 18 2026` 生成器的 CMake。当前项目已用 CMake 4.2.1 验证，建议使用 4.2.1 或更新版本。
- Visual Studio 2026 的 `Desktop development with C++` 工作负载。
- x86 MSVC 编译工具和 Windows SDK。
- `.NET Framework 4.6.2 Targeting Pack` 或包含 reference assemblies 的等效组件，供 HLAE 的 ShaderBuilder 使用。

首次配置时，CMake 会通过 HLAE 的 `FetchContent` 下载 protobuf、OpenEXR、Imath、Abseil 等依赖，需要网络连接和 GitHub 访问权限。依赖下载完成后会缓存在构建目录中，不会复制到 Git 仓库。

## 编译

在项目根目录打开 PowerShell。大多数机器可以让 HLAE 通过 `vswhere` 自动发现 Visual Studio：

```powershell
cmake -S . -B build-release -G "Visual Studio 18 2026" -A Win32 `
  -DL4N_BUILD_HLAE_PLUGIN=ON `
  -DL4N_BUNDLE_OPENEXR=ON `
  -DL4N_PACKAGE_RELEASE=ON `
  "-DL4N_SHADERBUILDER_FRAMEWORK_VERSION=v4.6.2"

cmake --build build-release --config Release --target l4n_release_package --parallel 1
```

如果自动发现失败，只需要在配置命令中增加一个项目变量，不需要手动填写 HLAE 内部使用的 `AFX_VS_INSTALLPATH` 和 `AFX_VS_MSBUILD`：

```powershell
cmake -S . -B build-release -G "Visual Studio 18 2026" -A Win32 `
  -DL4N_BUILD_HLAE_PLUGIN=ON `
  -DL4N_BUNDLE_OPENEXR=ON `
  -DL4N_PACKAGE_RELEASE=ON `
  "-DL4N_SHADERBUILDER_FRAMEWORK_VERSION=v4.6.2" `
  "-DL4N_VS_INSTALL_PATH=<Visual-Studio-installation-directory>"

cmake --build build-release --config Release --target l4n_release_package --parallel 1
```

`L4N_VS_INSTALL_PATH` 是顶层 CMake 变量。设置后，项目会检查该目录，并自动查找其中的 `MSBuild/Current/Bin/MSBuild.exe`，再传给 HLAE 的 ShaderBuilder。路径可使用正斜杠；尖括号只是占位符，执行前请替换成实际安装目录。

`L4N_SHADERBUILDER_FRAMEWORK_VERSION` 默认是 `v4.6.2`。如果安装的 Targeting Pack 版本不同，可以在配置时改成实际存在的版本，例如 `v4.6.1`；配置输出会显示最终选用的 reference assemblies。

本项目必须使用 `Win32` 配置。不要使用 `x64` 生成结果部署到 L4N。

### protobuf-populate / MSB8066 故障排查

如果看到类似下面的错误：

```text
MSB8066: protobuf-populate ... custom build exited with code 1
CMake Error ... FetchContent.cmake ... Build step for protobuf failed
```

这通常只是 protobuf 外部项目的包装错误，真正原因应在更早的输出中查找。常见原因包括依赖下载或 Git 更新失败、缓存没有完成，以及多个 MSBuild/`cl.exe` 子任务同时访问同一构建目录。

建议按以下顺序处理：

1. 关闭其他 CMake、Visual Studio 和 MSBuild 构建进程，并确认没有另一个构建仍在使用 `build-release`。
2. 删除本项目的 `build-release/` 生成目录后重新配置。该目录只包含可重新生成的缓存和依赖，不是源码。
3. 确认配置阶段能够访问 GitHub，并等待 protobuf 等依赖完成下载。
4. 使用 `--parallel 1` 串行编译，不要在同一目录同时运行多个构建命令。

必要时可以单独重试 protobuf 子构建：

```powershell
cmake --build build-release/_deps/protobuf-subbuild `
  --config Release `
  --target protobuf-populate `
  --parallel 1 `
  --verbose
```

当前项目验证过的结果是：protobuf 子构建串行重试成功，完整的 x86 Release 构建也能完成；原始失败发生在并行/未完成缓存的外部依赖构建阶段，而不是 Visual Studio 路径无效。

### 静态检查

成功打包后，可以检查 PE 架构、导出和依赖：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\verify_plugin.ps1 `
  -PluginPath .\Release\bin\neko\plugins\l4n_hlae_plugin.dll
```

检查结果应包括：

- PE Machine 为 `0x014C`。
- 导出表包含 `GetL4NPluginInstance`。
- 不存在对原始 `AfxHookSource.dll` 的静态依赖。
- 不存在外部 `Iex-3_3.dll`、`IlmThread-3_3.dll`、`Imath-3_1.dll`、`OpenEXR-3_3.dll` 或 `OpenEXRCore-3_3.dll` 依赖。

## 可选：升级 HLAE 源码

仓库内的 `advancedfx/` 是已经加入 L4N 改动的 HLAE 源码快照，不是独立 Git 仓库。当前快照版本记录在 `advancedfx-source-revision.txt`。如果要升级 HLAE，建议先在临时目录获取上游源码：

```powershell
git clone https://github.com/advancedfx/advancedfx.git advancedfx-upstream
git -C advancedfx-upstream checkout <commit-or-tag>
git -C advancedfx-upstream submodule update --init --recursive
git -C advancedfx-upstream rev-parse HEAD
```

确认上游版本可以独立构建后，再将上游源码与仓库内的 `advancedfx/` 做目录比较并合并。不要直接覆盖后跳过 L4N 改动。升级时必须检查并保留或重新移植这些文件和构建改动：

```text
advancedfx/AfxHookSource/l4n_plugin.cpp
advancedfx/AfxHookSource/l4n_plugin.h
advancedfx/AfxHookSource/hlaeFolder.cpp
advancedfx/AfxHookSource/CMakeLists.txt
advancedfx/CMakeLists.txt
```

然后将新的 `git rev-parse HEAD` 写入 `advancedfx-source-revision.txt`，重新执行 Win32 Release 编译、静态检查和游戏内验证。不要把上游的 `.git`、构建目录或预编译二进制复制进本项目。HLAE 的许可证、版权和归属文件必须继续保留。

## 使用注意事项

- 只部署 `Release/` 中的 `bin` 和 `l4n_hlae_core` 内容，不要把 `src/` 中的旧版 `LoadLibraryExW` 适配器与原生 HLAE 插件同时安装。
- 不要把 `injector.exe`、`AfxHook.dat`、原始 `AfxHookSource.dll` 或 OpenEXR DLL 混入本项目安装目录。
- 更新插件前应备份现有的 `bin/neko/plugins/l4n_hlae_plugin.dll` 和配置文件，并确认旧适配器没有以其他文件名残留。
- L4N 的其他插件属于独立组件。若 L4N 启动时报告某个无关 DLL 加载失败，应先检查该 DLL 是否是旧版本或残留文件，不要用本项目 DLL 替换它。
- `l4n_hlae_core/` 只包含运行所需的 HLAE 资源；它不是完整的 HLAE 独立安装包，也不能与 x64 HLAE 混用。
- HLAE 命令可用并不等于所有录制、渲染和编码功能都已经验证。出现崩溃、卡死、渲染异常或命令缺失时，应停止使用并保留日志。

## 项目结构

```text
src/                         旧版 LoadLibraryExW 适配器失败实验基线
include/                     L4N v1 ABI 头文件
advancedfx/                  带 L4N 改动的 HLAE 源码副本
config/                      配置模板
cmake/package_release.cmake  生成可复制的 Release 安装包
tools/                       PE 静态检查脚本
```

`build-release/` 和 `Release/` 都是生成目录，已加入 Git 忽略规则，不应提交。仓库不包含 HLAE 预编译 DLL、`injector.exe`、`AfxHook.dat` 或 OpenEXR DLL。

## 许可证和第三方项目

本项目新增的 L4N 集成代码、构建脚本和项目配置使用 MIT License，完整文本见 [LICENSE](LICENSE)。

本项目包含并改造了 [advancedfx/advancedfx](https://github.com/advancedfx/advancedfx) 的 HLAE 源码。HLAE 源码副本位于 `advancedfx/`，其原始许可证、版权和归属信息以 `advancedfx/LICENSE`、`advancedfx/CREDITS.md` 以及源码中的相关说明为准。HLAE 第三方源码不因本项目的 MIT License 而改变其原有授权条件。
