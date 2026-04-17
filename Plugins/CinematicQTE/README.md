# Cinematic QTE System

一个基于 Unreal Engine 5.5 的**过场动画 QTE（Quick Time Event）系统插件**，支持在 Level Sequence 过场动画的任意时间点插入交互式 QTE，实现"子弹时间"慢动作体验。

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

## 🧪 运行测试

编辑器菜单 → `Tools > Session Frontend > Automation`，勾选 `CinematicQTE.*` 分类执行：
- `CinematicQTE.Mash.*` — 连点 QTE 状态机
- `CinematicQTE.Tap.*` — 单点 QTE 状态机
- `CinematicQTE.PlayRate.*` — 速率控制器插值

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
