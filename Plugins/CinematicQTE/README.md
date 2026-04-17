# Cinematic QTE System

一个基于 Unreal Engine 5.5 的**过场动画 QTE（Quick Time Event）系统插件**，支持在 Level Sequence 过场动画的任意时间点插入交互式 QTE，实现"子弹时间"慢动作体验。

> 📖 **新手入门**：建议先跟着 [Docs/GettingStarted.md](Docs/GettingStarted.md) 走一遍手把手教程，20 分钟内完成从零到跑通一段 LevelSequence QTE 的全流程。

## ✨ 核心特性

- **数据驱动**：`UQTEDataAsset` 可视化配置，策划无需写代码即可接入
- **Sequencer 原生集成**：在 Sequencer 时间轴上直接添加 QTE Track / QTE Section
- **平滑动画速率过渡**：默认 0.01 倍慢速，可配置 `SlowMotionRate` / `SlowDownBlendTime` / `SpeedUpBlendTime` / `BlendCurve`
- **两种内置 QTE 类型**：
  - **Mash（连点累积）**：快速连按按键累积进度达成目标
  - **Tap（时机单点）**：在完美窗口内单次精准按键
- **Enhanced Input 无缝集成**：支持键鼠 / 手柄 / 触屏
- **事件驱动**：`OnGlobalQTEFinished` / `OnQTEFinished` 委托，方便接入剧情分支
- **可扩展**：继承 `UQTETaskBase` 可自定义 QTE 类型
- **调试友好**：`qte.Debug.Show`、`qte.Debug.ForceResult` 控制台变量

## 📁 目录结构

```
CinematicQTE/
├── CinematicQTE.uplugin
└── Source/
    ├── CinematicQTE/              # Runtime 核心
    │   ├── Public/
    │   │   ├── CinematicQTETypes.h        # 枚举 / 委托 / FQTEResultMeta
    │   │   ├── CinematicQTESubsystem.h    # WorldSubsystem 主入口
    │   │   ├── QTEDataAsset.h             # 配置基类
    │   │   ├── MashQTEDataAsset.h / TapQTEDataAsset.h
    │   │   ├── QTETaskBase.h              # 任务基类
    │   │   ├── MashQTETask.h / TapQTETask.h
    │   │   ├── QTEWidgetBase.h            # UI 抽象
    │   │   ├── SequencePlayRateController.h   # 速率平滑控制器
    │   │   ├── CinematicQTEDebugLibrary.h
    │   │   └── Sequencer/
    │   │       ├── MovieSceneQTETrack.h
    │   │       ├── MovieSceneQTESection.h
    │   │       └── MovieSceneQTESectionTemplate.h
    │   └── Private/ (同名 .cpp)
    ├── CinematicQTEEditor/         # Editor 支持（Sequencer 菜单）
    │   ├── Public/
    │   │   ├── CinematicQTEEditorModule.h
    │   │   └── MovieSceneQTETrackEditor.h
    │   └── Private/
    └── CinematicQTETests/          # 自动化测试
        └── Private/
            ├── QTETask.spec.cpp
            └── PlayRateController.spec.cpp
```

## 🚀 10 分钟接入指南（策划）

### 1. 启用插件
将 `CinematicQTE/` 放入项目 `Plugins/` 目录，重启编辑器。

### 2. 创建 QTE 数据资产
在 Content Browser 右键 → `Miscellaneous > Data Asset`：
- 选择 `MashQTEDataAsset` 或 `TapQTEDataAsset`
- 填写：
  - `Duration`（持续时间）
  - `Input Action`（玩家按键对应的 `UInputAction`）
  - `Widget Class`（派生自 `QTEWidgetBase` 的 UMG Widget 蓝图）
  - Mash 专有：`RequiredPressCount`、`ProgressDecayRate`
  - Tap 专有：`bUsePerfectWindow`、`PerfectWindowStart`、`PerfectWindowEnd`
  - 可选：`SlowMotionRate`（默认 0.01）、`SlowDownBlendTime`（0.2s）、`SpeedUpBlendTime`（0.3s）

### 3. 在 Sequencer 中插入 QTE
1. 打开 Level Sequence，点击 `+ Track` 下拉菜单 → `QTE Track`
2. 在 Track 上右键 → `Add Section`，将 Section 拖到目标时间点
3. 选中 Section，在 Details 面板指定 `QTEDataAsset`

### 4. 监听 QTE 结果（程序员）
在 Level Blueprint / GameMode / 任意蓝图中：
```cpp
// C++
UCinematicQTESubsystem* Sub = UCinematicQTESubsystem::Get(this);
Sub->OnGlobalQTEFinished.AddDynamic(this, &AMyActor::HandleQTEFinished);

void AMyActor::HandleQTEFinished(EQTEResult Result, UQTEDataAsset* Asset, FQTEResultMeta Meta)
{
    if (Result == EQTEResult::Success) { /* 成功逻辑 */ }
}
```

或在蓝图中调用 `Get Cinematic QTE Subsystem → Bind Event to On Global QTE Finished`。

### 5. UI 示例
派生 `QTEWidgetBase` 的 UMG 蓝图，实现事件：
- `BP_On QTE Started(Data Asset)` — 初始化显示按键图标/提示文本
- `BP_On Progress Changed(Progress)` — 更新进度条（Mash）
- `BP_On Remaining Time Changed(Remaining Ratio)` — 更新收缩光圈（Tap）
- `BP_On QTE Finished(Result)` — 播放成功 / 失败动画

## 🎬 CinematicQTEEditor 模块使用详解

`CinematicQTEEditor` 是**纯编辑器模块**（打包时不会被包含），它的职责是：
- 向 Sequencer 的 `+ Track` 下拉菜单注册 **"QTE Track"** 菜单项
- 提供 `MovieSceneQTETrack` / `MovieSceneQTESection` 的编辑器交互（拖拽、右键菜单、属性面板）

### 1. 确认模块已加载
编辑器启动后，打开 `Window → Developer Tools → Modules`，搜索：
- `CinematicQTE` — 状态应为 **Loaded**
- `CinematicQTEEditor` — 状态应为 **Loaded**

若 `CinematicQTEEditor` 未加载，通常是 `.uplugin` 中 `"LoadingPhase": "PostEngineInit"` 未生效或编辑器启动时报错，查看 `Output Log` 中 `LogCinematicQTEEditor` 分类。

### 2. 在 Level Sequence 中添加 QTE Track

完整交互流程：

```
┌─ Content Browser ──────────────────┐
│ 右键 → Cinematics → Level Sequence │
│ 命名 LS_CutsceneDemo，双击打开     │
└────────────────────────────────────┘
               │
               ▼
┌─ Sequencer 面板 ─────────────────────────────┐
│ 点击左上角 [+ Track ▾]                       │
│   ├─ Folder                                  │
│   ├─ Actor To Sequencer                      │
│   ├─ Camera Cut Track                        │
│   ├─ ...（引擎内置）                         │
│   └─ QTE Track              ← CinematicQTEEditor 注入的入口 │
└──────────────────────────────────────────────┘
               │ 点击
               ▼
   时间轴新增一条红棕色 "QTE Track"
               │
               ▼
┌─ 在 Track 行上右键 ─────────────────┐
│   → Add Section                      │
│   （或长按拖动指定时间范围）         │
└──────────────────────────────────────┘
               │
               ▼
   生成 QTE Section（默认长度 = DataAsset.Duration）
               │ 选中 Section
               ▼
┌─ Details 面板 ──────────────────────────┐
│ ▼ QTE                                  │
│   QTE Data Asset     [DA_MashQTE_Demo]│ ← 必填
│   Conflict Policy    [Ignore ▼]        │
└────────────────────────────────────────┘
```

### 3. Section 属性说明

| 属性 | 类型 | 说明 |
|---|---|---|
| `QTE Data Asset` | `UQTEDataAsset*` | 必填。指向 `MashQTEDataAsset` / `TapQTEDataAsset` 或自定义派生资产 |
| `Conflict Policy` | `EQTEConflictPolicy` | `Ignore` 丢弃、`Queue` 排队、`Replace` 抢占当前 QTE |
| Section 起始帧 | Sequencer 原生 | 触发 QTE 的时间点；Section 长度无实际作用（QTE 时长由 DataAsset.Duration 决定） |

### 4. 多个 QTE 编排建议

- **串行**：多个 Section 首尾相接，每个 Section 绑定不同 DataAsset
- **并行**（不推荐）：时间轴上重叠的 Section 会按 Conflict Policy 处理，实测容易出现 UI 抖动，请尽量避免
- **分支预留**：对于"成功 → A 剧情、失败 → B 剧情"的分支，不要在 Sequencer 里写分支；在 Level Blueprint 监听 `OnGlobalQTEFinished`，用 `PlayTo` / `JumpTo` 跳转不同 Sequence

### 5. PIE 调试
1. 场景中拖入 `Level Sequence Actor` 并指定你的 Sequence
2. 勾选 `Auto Play` 或者在 BeginPlay 中手动 `Play()`
3. PIE 运行，到达 Section 起始时间点：
   - 看到 `PlayRate` 从 1.0 插值到 0.01
   - UI Widget 创建在 Viewport 上
   - `InputMappingContext` 被压入 `EnhancedInputLocalPlayerSubsystem`
4. QTE 结束后 PlayRate 插值回 1.0，UI 销毁，IMC 弹出

## 🎮 运行时 API

```cpp
// 手动启动 QTE（不依赖 Sequencer）
UCinematicQTESubsystem::Get(World)->StartQTE(DataAsset, LevelSequencePlayer, EQTEConflictPolicy::Ignore);

// 取消当前 QTE
Sub->CancelCurrentQTE(EQTEResult::Cancelled);

// 查询状态
Sub->IsQTEActive();
Sub->GetCurrentTask();
Sub->GetCurrentPlayRate();
```

## 🛠 调试控制台

| 控制台变量 / 命令          | 说明                                     |
|---------------------------|------------------------------------------|
| `qte.Debug.Show 1`        | 屏幕显示当前 QTE 类型 / 剩余时间 / 进度 / PlayRate |
| `qte.Debug.ForceResult Success` | 下一次 QTE 强制以成功结束（调试剧情分支）|
| `qte.Debug.ForceResult Failure` | 下一次 QTE 强制以失败结束                |

## 🔌 扩展自定义 QTE 类型

1. 派生 `UQTEDataAsset` 与 `UQTETaskBase`
2. 在 DataAsset 构造函数中设置 `TaskClass = UMyCustomTask::StaticClass()`
3. 重写 `UQTETaskBase::OnStartQTE / OnTickQTE / OnHandleInput / OnFinishQTE`
4. 在派生任务中调用 `BroadcastProgress / BroadcastRemaining / FinishQTE` 管理状态

## 🧪 CinematicQTETests 模块使用详解

`CinematicQTETests` 是 **DeveloperTool** 类型模块，只在 `Development Editor` / `DebugGame Editor` 下加载，打包版本不包含。它提供 9 个自动化测试用例，覆盖连点 / 单点 / 速率控制三类核心状态机。

### 1. 测试用例清单

| 命令空间 / 测试路径 | 对应源文件 | 断言目标 |
|---|---|---|
| `CinematicQTE.Mash.SuccessByReachingTarget` | `QTETask.spec.cpp` | 连按达到 `RequiredPressCount` 即判定 Success |
| `CinematicQTE.Mash.RejectPressWithinMinInterval` | 同上 | 小于 `MinPressInterval` 的抖动按键被忽略 |
| `CinematicQTE.Mash.TimeoutWhenProgressNotReached` | 同上 | `Duration` 到期但进度不足 → Timeout |
| `CinematicQTE.Tap.SuccessInPerfectWindow` | `QTETask.spec.cpp` | 在 `[PerfectWindowStart, PerfectWindowEnd]` 内按键 → Success |
| `CinematicQTE.Tap.FailOutsideWindow` | 同上 | 窗口外按键 → Failure，并立即结束 |
| `CinematicQTE.Tap.TimeoutWhenNoInput` | 同上 | 整段无输入 → Timeout |
| `CinematicQTE.PlayRate.ImmediateSnapWhenBlendTimeZero` | `PlayRateController.spec.cpp` | `BlendTime=0` 时当帧直接切换到目标速率 |
| `CinematicQTE.PlayRate.BlendProgressesOverTime` | 同上 | 每帧 `Tick` 速率按曲线单调逼近目标 |
| `CinematicQTE.PlayRate.InterruptedBlendStartsFromCurrent` | 同上 | 混合中途发起新 blend，起点 = 当前插值中的速率 |

### 2. 运行方式 A：编辑器 UI

1. 菜单 `Tools → Test Automation`（UE 5.5 入口）或 `Tools → Session Frontend → Automation` 标签页
2. 左侧测试树展开 `CinematicQTE` 节点，勾选想跑的子节点
3. 点击右上角 **Start Tests**
4. 实时查看每条用例的 PASS / FAIL，失败时下方日志会打印断言位置

![automation-panel 示意]
```
☑ CinematicQTE
  ☑ Mash
    ☑ SuccessByReachingTarget        ✅ 12ms
    ☑ RejectPressWithinMinInterval   ✅ 8ms
    ☑ TimeoutWhenProgressNotReached  ✅ 16ms
  ☑ Tap
    ☑ SuccessInPerfectWindow         ✅ 9ms
    ☑ FailOutsideWindow              ✅ 7ms
    ☑ TimeoutWhenNoInput             ✅ 15ms
  ☑ PlayRate
    ☑ ImmediateSnapWhenBlendTimeZero ✅ 2ms
    ☑ BlendProgressesOverTime        ✅ 11ms
    ☑ InterruptedBlendStartsFromCurrent ✅ 6ms
```

### 3. 运行方式 B：编辑器控制台命令

在主视口按 `` ` `` 打开控制台：

```
Automation RunTests CinematicQTE                              # 跑全部 9 条
Automation RunTests CinematicQTE.Mash                         # 仅连点组
Automation RunTests CinematicQTE.Tap.SuccessInPerfectWindow   # 单条用例
Automation List CinematicQTE                                  # 列出匹配的用例
```

### 4. 运行方式 C：命令行（CI / 无头）

```bat
"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "D:\UnrealProject\GameAnimationSample\GameAnimationSample.uproject" ^
  -ExecCmds="Automation RunTests CinematicQTE; Quit" ^
  -TestExit="Automation Test Queue Empty" ^
  -Unattended -NoSound -NullRHI ^
  -ReportOutputPath="D:\UnrealProject\GameAnimationSample\Saved\AutomationReports"
```

- 退出码 `0` = 全部通过；非 0 = 至少一条失败
- 报告位于 `Saved/AutomationReports/index.html`
- 日志位于 `Saved/Logs/GameAnimationSample.log`，搜索 `LogAutomationController` 查看每条结果

### 5. 编写新测试用例

在 `Plugins/CinematicQTE/Source/CinematicQTETests/Private/` 下新建 `MyCase.spec.cpp`：

```cpp
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "QTEDataAsset.h"
#include "MashQTEDataAsset.h"
#include "MashQTETask.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMyMashCustomTest,
    "CinematicQTE.Mash.MyCustomCase",     // ← UI 中显示的路径
    EAutomationTestFlags::EditorContext
  | EAutomationTestFlags::ClientContext
  | EAutomationTestFlags::EngineFilter)

bool FMyMashCustomTest::RunTest(const FString& Parameters)
{
    UMashQTEDataAsset* Data = NewObject<UMashQTEDataAsset>();
    Data->Duration = 2.0f;
    Data->RequiredPressCount = 5;

    UMashQTETask* Task = NewObject<UMashQTETask>();
    Task->Initialize(Data, /*Subsystem=*/nullptr);
    Task->StartTask();

    for (int32 i = 0; i < 5; ++i)
    {
        Task->HandlePress();
    }

    TestEqual(TEXT("达到目标后应判定为 Success"),
              (int32)Task->GetResult(), (int32)EQTEResult::Success);
    return true;
}
#endif
```

重新编译后，新测试会自动出现在 Automation 面板的 `CinematicQTE.Mash.MyCustomCase` 路径下。

### 6. 测试调试技巧

- **只想看失败** ：UI 面板右上角 `Filter Results → Failed Only`
- **断点调试** ：以 `Development Editor` 配置启动 VS，主视口 `Automation RunTests CinematicQTE.Mash`，可在 `.spec.cpp` 中打断点
- **Flaky 排查** ：`Automation SetMinimumPriority Medium` 后批量跑 N 次，UE 会统计通过率

## 📐 架构概览

```
         ┌─────────────────────────────┐
         │   Level Sequence (Sequencer)│
         │   └─ MovieSceneQTETrack     │
         │      └─ MovieSceneQTESection│
         └─────────────┬───────────────┘
                       │ Evaluate()
                       ▼
         ┌─────────────────────────────┐
         │  UCinematicQTESubsystem     │
         │  (WorldSubsystem, Tickable) │
         │                             │
         │  ├─ FSequencePlayRateCtrl   │──► ULevelSequencePlayer::SetPlayRate
         │  ├─ UQTETaskBase (运行中)   │──► 广播 OnProgressChanged 等
         │  ├─ UQTEWidgetBase (UI)     │──► UMG 显示
         │  └─ UInputMappingContext    │──► EnhancedInput 路由
         └─────────────────────────────┘
```

## ⚠️ 已知限制

- Dedicated Server 环境下自动跳过 UI 与输入绑定，但 QTE 事件仍会触发并广播 `OnGlobalQTEFinished`
- 冲突策略 `Queue` 在高并发下可能导致视觉延迟，建议策划在 Sequencer 中避免重叠 Section
- `BlendCurve` 的横轴应为 `[0, 1]`，纵轴取值决定插值 Alpha，不建议超出 `[0, 1]` 范围

## 📜 License

本插件遵循所属项目的许可证条款。
