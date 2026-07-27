# GameAnimationSample

基于 Epic **Game Animation Sample** 的 Unreal Engine **5.8** 工程分支，额外包含 Cinematic QTE、Property History、Electronic Nodes 等插件与 C++ 实验代码。

## 重要：本仓库不包含 Content

`Content/` 下的二进制资源（`.uasset` / `.umap` 等）已被 **gitignore**，体积过大，不适合进 Git。  
仅克隆本仓库只能得到源码、配置与项目插件，**无法直接运行完整示例关卡**。

必须先从 Epic 官方 Sample 获取 Content，再放到本工程中使用。

### 从 Fab / Epic 获取 Content

1. 打开 [Epic Games Launcher](https://store.epicgames.com/zh-CN/download) → **虚幻引擎** → **Fab** / 库（或在浏览器打开 [Fab](https://www.fab.com/)）。
2. 搜索 **Game Animation Sample**，添加并下载适用于 **Unreal Engine 5.8** 的资源包（或你准备改绑的相近 5.x 版本）。
3. 至少打开一次官方 Sample，确保资源下载完成（常见路径类似：`Documents/Unreal Projects/GameAnimationSample 5.8`）。

官方 Sample（Fab，兼容 UE 5.4–5.8）：

- [Game Animation Sample（Fab）](https://www.fab.com/listings/880e319a-a59e-4ed2-b268-b32dac7fa016)
- 或在 Epic Games Launcher → Fab / 库中搜索：`Game Animation Sample`

### 将 Content 合并进本工程

把官方 Sample 目录下的整个 `Content` 文件夹复制到本仓库根目录：

```text
<官方Sample>/Content  →  <本仓库>/Content
```

推荐命令（Windows PowerShell / cmd）：

```powershell
robocopy "C:\Users\<你的用户名>\Documents\Unreal Projects\GameAnimationSample 5.8\Content" ".\Content" /E
```

可选：若地图或插件加载异常，可从官方 Sample 同步部分 `Config/`（尤其是 `DefaultEngine.ini`、设备配置、Network Prediction 相关）。  
同步时请 **不要盲目覆盖** 本仓库 `GameAnimationSample.uproject` 中已启用的自定义插件配置，合并前先看 diff。

复制完成后目录应类似：

```text
GameAnimationSample/
  Content/          # 来自 Fab / 官方 Sample（仅本地）
  Config/
  Source/
  Plugins/
  GameAnimationSample.uproject
```

## 环境要求

- Unreal Engine **5.8**（Epic Games Launcher 二进制版本）
- Visual Studio 2022 或更新版本，并安装 C++ 桌面开发工作负载（VS 2026 亦可）
- Windows 10/11
- Git

## 快速开始

1. 克隆本仓库。
2. 通过 Fab / Epic Launcher 下载官方 **Game Animation Sample** Content（见上文）。
3. 将官方 `Content/` 复制到本工程根目录。
4. 右键 `GameAnimationSample.uproject` → **Generate Visual Studio project files**，或用 UE 5.8 直接打开并允许编译模块。
5. 如有需要，编译目标 `GameAnimationSampleEditor`（Development | Win64）。
6. 打开编辑器后 Play（有 Content 时默认关卡为 `DefaultLevel`）。

命令行编译示例：

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  GameAnimationSampleEditor Win64 Development `
  -Project="$PWD\GameAnimationSample.uproject" -WaitMutex
```

## 工程结构

| 路径 | 说明 |
|------|------|
| `Source/` | 游戏模块 C++（角色移动相关扩展等） |
| `Content/` | **不在 Git 中** — 来自 Epic Sample 的地图、动画、蓝图等 |
| `Config/` | 工程 `.ini` 配置 |
| `Plugins/CinematicQTE/` | 过场 QTE 系统（Level Sequence 集成） |
| `Plugins/PropertyHistory/` | Details 面板属性历史（编辑器） |
| `Plugins/ElectronicNodes/` | 蓝图 / 材质连线风格（编辑器） |

## 插件

### 引擎 / Sample 相关（`.uproject` 中已启用）

AnimationWarping、PoseSearch、AnimationLocomotionLibrary、MotionWarping、HairStrands、Chooser、RigLogic、LiveLink、LiveLinkControlRig、Mover、NetworkPrediction、SmartObjects、Locomotor 等。

### 本仓库自带插件

- **CinematicQTE** — Level Sequence 上的 QTE Track / Section
- **PropertyHistory** — 属性右键 → **See history**（需编辑器启用源码管理 / Git）
- **ElectronicNodes** — 蓝图 / 材质编辑器电路板风格连线

各插件细节见 `Plugins/*/README.md`。

## 功能（安装 Content 后）

- 角色移动 / Motion Matching 示例关卡
- Animation Warping、Pose Search
- MetaHuman 相关示例资源（来自 Epic 包）
- 上述项目插件提供的额外工具能力

## 常见问题

| 现象 | 排查 |
|------|------|
| 没有关卡 / 资源缺失 | 未从官方 Sample 复制 `Content/` |
| 引擎版本不匹配提示 | 工程绑定 **5.8**，请安装 UE 5.8 或谨慎改绑 |
| 拉取后编译失败 | 重新生成工程文件并编译 Editor 目标 |
| Property History 无记录 | 编辑器中启用 Git / 源码管理 |
| PoseSearch / PoseHistory 日志刷屏 | 多为 Content / AnimBP 版本不匹配，请使用对应 5.8 的官方 Content |

## 许可

- Epic Game Animation Sample 的 Content 仍遵循 Epic / Fab 的示例许可条款。
- 本仓库中的代码与插件以学习 / 开发用途为主；各插件以 `Plugins/*/LICENSE` 为准（如有）。
