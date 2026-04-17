# 🎬 手把手教程：在 LevelSequence 中使用 CinematicQTE

> 适用版本：UE 5.5
> 目标读者：第一次接触 CinematicQTE 插件的策划 / 客户端程序
> 预计用时：20 分钟

本教程带你从零构建一段带有 **Mash 连点 QTE** 与 **Tap 时机 QTE** 的过场动画，完整验证插件的全部核心功能。

---

## 🎯 本次测试目标

做两段串行测试：

1. **Mash 连点进度型** — 摄像机拉近目标，触发 QTE，0.01× 慢速下连按 N 次 `E` 键
2. **Tap 单点时机型** — 继续播放，在完美窗口内按一次 `Space`，成功则通关

把两个 QTE 串到同一条 LevelSequence 里测试串行触发。

---

## Step 0 · 前置检查（30 秒）

编辑器菜单 **Window → Developer Tools → Modules**，搜 `QTE`，确认下面两个模块状态都是 **Loaded**：

| 模块名 | 期望状态 |
|---|---|
| `CinematicQTE` | **Loaded** |
| `CinematicQTEEditor` | **Loaded** |

如果任何一个不是 Loaded，去 `Edit → Plugins` 搜 `CinematicQTE`，勾选启用后重启编辑器。

---

## Step 1 · 建立规范的资源目录（1 分钟）

Content Browser 中右键 → `New Folder`，依次建立：

```
Content/
└── QTEDemo/
    ├── Input/          ← InputAction / InputMappingContext
    ├── Data/           ← QTEDataAsset
    ├── UI/             ← QTE Widget 蓝图
    ├── Maps/           ← 测试用关卡
    └── Sequences/      ← LevelSequence
```

---

## Step 2 · 创建输入（Enhanced Input）（2 分钟）

CinematicQTE 走 Enhanced Input，每个 QTE 都需要一个 `UInputAction`。

### 2.1 新建 `IA_QTE_Mash`

1. 进入 `Content/QTEDemo/Input`
2. 右键 → **Input → Input Action**，命名 `IA_QTE_Mash`
3. 双击打开，保持默认 `Value Type = Digital (bool)`，保存

### 2.2 新建 `IA_QTE_Tap`

同上流程，命名 `IA_QTE_Tap`，`Value Type = Digital (bool)`。

### 2.3 新建 InputMappingContext

> 📌 **为什么要有 IMC？** CinematicQTE 在 QTE 触发时会自动创建内部动态 IMC 并 push 到玩家 Subsystem（无需手动管）。但 Enhanced Input 要求 Action 至少在某个 IMC 中被"映射过"才会正常响应硬件按键，所以我们需要一个 IMC 把 Action 关联到键盘键。

1. `Input` 文件夹右键 → **Input → Input Mapping Context**，命名 `IMC_QTEDemo`
2. 双击打开，`+ Mappings` 两次：
   - `IA_QTE_Mash` → **E** 键
   - `IA_QTE_Tap`  → **Space Bar**
3. 保存

如果项目已有玩家默认 IMC（如 `IMC_Default`），也可以直接把这两个 Action 加进去。`GameAnimationSample` 默认 Pawn 是 `BP_SandboxCharacter`，我们会在 Step 5 把 IMC 压入玩家。

---

## Step 3 · 创建 QTE DataAsset（2 分钟）

### 3.1 Mash 配置

1. 进入 `Content/QTEDemo/Data`
2. 右键 → **Miscellaneous → Data Asset**
3. 弹框搜 `MashQTEDataAsset`，选它 → OK
4. 命名 `DA_QTE_Mash_Demo`
5. 双击打开，按下表填值：

| 分类 | 字段 | 值 |
|---|---|---|
| QTE \| Basic | Duration | `5.0` |
| QTE \| Basic | Input Action | `IA_QTE_Mash` |
| QTE \| SlowMotion | Slow Motion Rate | `0.01`（默认） |
| QTE \| SlowMotion | Slow Down Blend Time | `0.2` |
| QTE \| SlowMotion | Speed Up Blend Time | `0.3` |
| QTE \| UI | Widget Class | 先留空（Step 4 填） |
| QTE \| Mash | Required Press Count | `10` |
| QTE \| Mash | Progress Per Press | `0`（=0 时自动取 `1/Count`） |
| QTE \| Mash | Progress Decay Rate | `0.05` |
| QTE \| Mash | Min Press Interval | `0.05` |

保存。

### 3.2 Tap 配置

同路径右键 → **Data Asset**，这次选 `TapQTEDataAsset`，命名 `DA_QTE_Tap_Demo`：

| 分类 | 字段 | 值 |
|---|---|---|
| QTE \| Basic | Duration | `2.0` |
| QTE \| Basic | Input Action | `IA_QTE_Tap` |
| QTE \| UI | Widget Class | 先留空 |
| QTE \| Tap | bUsePerfectWindow | ✅ |
| QTE \| Tap | Perfect Window Start | `0.4` |
| QTE \| Tap | Perfect Window End | `0.7` |

> 💡 **窗口含义**：Duration = 2s + 窗口 `0.4~0.7` = 玩家必须在第 **0.8s ~ 1.4s** 之间按一次 Space 才算成功。

---

## Step 4 · 创建 QTE UMG Widget（5 分钟，可选但推荐）

没 Widget 也能跑——只是屏幕上看不到进度条。做一个最简的通用 HUD。

### 4.1 `WBP_QTEHud`

1. 进入 `Content/QTEDemo/UI`
2. 右键 → **User Interface → Widget Blueprint** → **All Classes** 搜 `QTEWidgetBase` → 选它作为父类
3. 命名 `WBP_QTEHud`，双击打开
4. Designer 面板按下图摆一个最简 HUD：

```
Canvas Panel（根）
└─ Vertical Box  [锚点 居中, 位置 (0, 300)]
   ├─ TextBlock       "TipText"       (IsVariable ✅)
   ├─ ProgressBar     "ProgressBar"   (IsVariable ✅, Size 400x30)
   └─ TextBlock       "ResultText"    (IsVariable ✅)
```

### 4.2 绑定父类事件

切到 Graph 面板，右键搜以下 **父类事件**（都是 `UQTEWidgetBase` 提供的 BlueprintImplementableEvent）并 Override：

| 父类事件 | 节点实现 |
|---|---|
| `BP On QTE Started (DataAsset)` | `TipText → SetText(DataAsset.DisplayText)` |
| `BP On Progress Changed (Progress)` | `ProgressBar → SetPercent(Progress)` |
| `BP On Remaining Time Changed (RemainingRatio)` | `ProgressBar → SetPercent(RemainingRatio)`（Tap 用） |
| `BP On Perfect Window Info (bHasPerfectWindow, WindowStart, WindowEnd)` | Tap 类型下用来绘制"完美窗口高亮区"，见下方说明 |
| `BP On QTE Finished (Result)` | Branch on Result，给 `ResultText` 设 `Success!` / `Failed` / `Timeout` |

最简版本只绑 `Progress Changed` + `QTE Finished` 也可以。编译 + 保存。

> 💡 **完美窗口可视化（推荐给 Tap 类 QTE）**
> `BP_OnPerfectWindowInfo` 在 QTE 启动时触发一次，参数含义：
> - `bHasPerfectWindow`：当前 DataAsset 是否是启用了完美窗口的 Tap 类型
> - `WindowStart / WindowEnd`：窗口相对 Duration 的比例，0~1
>
> 典型做法：在进度条上叠一个 `Image`，锚定父 ProgressBar，X 偏移 = `Width * WindowStart`，Width = `Width * (WindowEnd - WindowStart)`，让玩家一眼看到"绿色安全区"在哪。非 Tap 类型时把该 Image `SetVisibility(Collapsed)`。
>
> 这三个值同时也以 `BlueprintReadOnly` 属性形式挂在基类（`bHasPerfectWindow / PerfectWindowStart / PerfectWindowEnd`），可在任意节点里直接 Get，不必依赖事件时序。

### 4.3 回头填入 DataAsset

- `DA_QTE_Mash_Demo.WidgetClass` → `WBP_QTEHud`
- `DA_QTE_Tap_Demo.WidgetClass`  → `WBP_QTEHud`

保存两个 DataAsset。

---

## Step 5 · 准备测试关卡（1 分钟）

为不污染现有关卡，复制一份空关卡：

1. `File → New Level → Basic`，保存到 `Content/QTEDemo/Maps/Map_QTETest.umap`
2. 关卡中放一个 Cube 和一个 Cine Camera Actor（用于后面做镜头）
3. 将默认 Pawn 改为 `BP_SandboxCharacter`（或让 `Project Settings → Maps & Modes` 保持当前默认）
4. 把 `IMC_QTEDemo` 压入玩家（二选一）：

   **方案 A：修改 Character**
   - 打开项目实际使用的 `BP_SandboxCharacter`
   - 在 `BeginPlay` 已有的 `AddMappingContext(IMC_Default)` 旁并联一个 `AddMappingContext(IMC_QTEDemo, Priority=0)`
   - 编译保存

   **方案 B：Level Blueprint（不改 Character）**
   - 打开 Level Blueprint，`BeginPlay` 接：
     ```
     GetPlayerController(0)
       → GetLocalPlayer
       → GetSubsystem<EnhancedInputLocalPlayerSubsystem>
       → AddMappingContext(IMC_QTEDemo, 0)
     ```

---

## Step 6 · 新建 LevelSequence 并插入 QTE（⭐ 核心）

### 6.1 新建 Sequence

1. 进入 `Content/QTEDemo/Sequences`
2. 右键 → **Cinematics → Level Sequence**，命名 `LS_QTEDemo`
3. 双击打开

### 6.2 加摄像机轨道（让你肉眼能看到过场在播）

1. Sequencer 左上 `+ Track ▾` → **Camera Cut Track**
2. Camera Cut Track 上右键 → `Add New Camera` → 选场景中的 CineCamera
3. 把时间轴末尾拖到 `00:00:10`（10 秒）
4. （可选）给 CineCamera 的 Transform 打两个 keyframe 做个轻微位移

### 6.3 QTE Section 的两种时长模式

添加的 Section 都是同一种（无创建入口差异），运行时长由 Section 上的 `bUseSectionRangeAsDuration` 勾选框切换：

| `bUseSectionRangeAsDuration` | 语义 | 运行时长 | Section 长度作用 | 适用 |
|---|---|---|---|---|
| **false（默认）** | AnimNotify 风格 | `DataAsset.Duration` | 仅视觉占位 | 固定时长的 Mash/Tap，策划只关心“哪一刻触发” |
| **true**         | AnimNotifyState 风格 | Section 长度 | 决定运行时长 | 需要用时间轴宽度反映节奏，Section 末端未通关则 `Timeout` |

> 📌 两种模式下 Section 都可以自由拖动位置 / 拉伸长度 / 在 Details 面板编辑属性，两者解耦。即使在 Key 模式下拉长了 Section，运行时仍然以 `DataAsset.Duration` 为准；Section 长度仅影响时间轴上的视觉占位。

### 6.4 ⭐ 添加 Mash（Key 模式，触发点 = 2s）

1. Sequencer `+ Track ▾` → **QTE Track**（CinematicQTEEditor 注入的入口）
2. 时间轴光标拖到 `2s`
3. 在 QTE Track 行上右键 → `QTE → **Add QTE Section**`
4. 选中生成的 Section（菱形 icon，默认长度 0.25s，视觉占位），在右侧 Details：
   - `QTE → QTE Data Asset` = `DA_QTE_Mash_Demo`
   - `QTE → Conflict Policy` = `Ignore`
   - `QTE → Use Section Range As Duration` = **❌ 保持未勾选**

运行时长 = `DA_QTE_Mash_Demo.Duration = 5.0s`，即 2s~7s（Section 视觉长度不影响运行）。

### 6.5 ⭐ 添加 Tap（Range 模式，窗口 = 7s ~ 8.2s）

> 💡 同一条 QTE Track 可以承载多个 Section。

1. 时间轴光标拖到 `7s`
2. 在同一条 QTE Track 上右键 → `QTE → **Add QTE Section**`
3. 新生成的 Section 默认长度 0.25s；**抓住 Section 右端手柄拖到 `8.2s`**，让窗口长度 = 1.2s
4. 选中 Section，在 Details：
   - `QTE Data Asset` = `DA_QTE_Tap_Demo`
   - `Conflict Policy` = `Ignore`
   - `Use Section Range As Duration` = **✅ 勾选**

勾选后，`DataAsset.Duration` 被忽略，**实际运行时长 = Section 长度 = 1.2s**，期间未通关则 `Timeout`。

完成后 Sequencer 大致是：

```
|Camera Cut|===[ CineCamera ]==========================|
|QTE       |   ◆Mash                     ▇▇Tap▇▇          |
0s         2s  (DA.Duration=5s, 结束于 7s)  7s      8.2s
                                                         10s
```

◆ = Key 模式（菱形 icon + 短占位条）；▇ = Range 模式（矩形条长度 = 运行时长）。

保存。

---

## Step 7 · 把 Sequence 放入关卡，启动自动播放（1 分钟）

1. 从 Content Browser 拖 `LS_QTEDemo` 到关卡视口 → 会生成 `Level Sequence Actor`
2. 选中这个 Actor，在 Details：
   - `Playback → Auto Play` ✅
   - `Playback → Loop` = `No Looping`
3. 保存关卡

---

## Step 8 · PIE 实测（5 分钟）

### 8.1 开调试 HUD

主视口控制台（`` ` `` 键）输入：
```
qte.Debug.Show 1
```
这会在屏幕上叠加显示当前 QTE 类型 / 剩余时间 / 进度 / PlayRate。

按 **Alt+P** 或点 Play。

### 8.2 预期现象

| 时间 | 现象 |
|---|---|
| 0~2s | 摄像机过场正常播，PlayRate = 1.0 |
| **2s** | 调试 HUD 显示 `QTE: Mash`（Key 模式），PlayRate 在 0.2s 内从 1.0 插到 0.01，Widget 弹出 |
| 2s~7s | **持续按 E 键** ≥10 次（间隔 > 50ms），进度满 → `Result: Success`；或啥都不按到 7s → `Result: Timeout`。Key 模式 QTE 时长 = `DA.Duration = 5s`，结束点固定在 7s（与 Section 视觉长度无关） |
| QTE 结束 | PlayRate 在 0.3s 内从 0.01 插回 1.0，Widget 消失 |
| 7s | 过场继续，PlayRate 再次慢速，`QTE: Tap`（Range 模式），窗口 `7s ~ 8.2s`（Section 长度 1.2s） |
| 窗口内按 Space（看 UI 绿色完美区高亮） | `Result: Success` |
| 非完美区按 Space | `Result: Failure`，立即结束 |
| 全程不按 | **8.2s** 时 Section 末端到达 → `Result: Timeout`（Range 模式由 Section 末端驱动） |
| ~10s | 过场结束 |

### 8.3 常用调试命令

```
qte.Debug.Show 1                  # 开调试HUD
qte.Debug.ForceResult Success     # 下一个 QTE 强制成功
qte.Debug.ForceResult Failure     # 下一个 QTE 强制失败
qte.Debug.ForceResult None        # 恢复正常判定
```

---

## Step 9 · 监听 QTE 结果做剧情分支（可选，5 分钟）

打开 `Level Blueprint`（菜单 `Blueprints → Open Level Blueprint`），`BeginPlay` 后这样连：

```
Event BeginPlay
   │
   ▼
Get Cinematic QTE Subsystem (World)
   │
   ▼
Bind Event to OnGlobalQTEFinished
   │
   └─ Event(Result, DataAsset, Meta)
         │
         ▼
      Switch on EQTEResult (Result)
         ├─ Success   → Print "✅ SUCCESS"
         ├─ Failure   → Print "❌ FAILURE"
         ├─ Timeout   → Print "⏱️ TIMEOUT"
         └─ Cancelled → Print "🚫 CANCELLED"
```

> 委托签名：`(EQTEResult Result, UQTEDataAsset* Asset, FQTEResultMeta Meta)`。Meta 里带 `ElapsedTime / PressCount / FinalProgress / PressTimingRatio`，可用于区分是哪个 QTE（通过比较 `Asset` 指针）。

---

## Step 10 · 跑一遍自动化测试做健康检查（1 分钟）

编辑器控制台敲：
```
Automation RunTests CinematicQTE
```

1 秒内可在 Output Log 看到：
```
LogAutomationController: Test Completed. Result={Passed}  Name={CinematicQTE.Mash.SuccessByReachingTarget}
... (共 9 条)
```

9 条全绿，说明插件运行时状态机 OK。

---

## 🧯 常见问题排查

| 现象 | 可能原因 | 解决 |
|---|---|---|
| `+ Track` 里找不到 `QTE Track` | `CinematicQTEEditor` 未加载 | Plugins 面板启用；Output Log 搜 `LogCinematicQTEEditor` 查报错 |
| QTE Track 行右键没菜单 | 插件旧版本缺 `BuildTrackContextMenu` | 拉取最新代码重新编译 |
| 拉伸了 Key 模式 Section 但运行时长没变 | 设计如此：Key 模式下 Section 长度仅视觉占位，运行时以 `DA.Duration` 为准 | 改时长请编辑 DataAsset；或勾上 `Use Section Range As Duration` |
| Range 模式 Section 到头没 Timeout | 忘了勾 `Use Section Range As Duration` | 勾上该选项 |
| QTE 触发了但按键不响应 | IMC 没 push 到玩家 | Step 5 的 `AddMappingContext(IMC_QTEDemo)` 没做 |
| QTE 触发但过场没变慢 | Sequence 的 `Auto Play` 没开，或不是由 `ULevelSequencePlayer` 驱动 | Details 启用 Auto Play；或用蓝图的 `Create Level Sequence Player` 节点手动播 |
| UI 没出现 | DataAsset 的 `WidgetClass` 为空；Widget 父类不是 `QTEWidgetBase` | 检查字段与父类 |
| Mash 按得很快但进度不涨 | `MinPressInterval=0.05` 过滤 | 正常的防宏行为；>20 次/秒会被忽略 |
| `qte.Debug.Show 1` 无效 | 在 PIE 外输入不生效 | 进 PIE 后再输 |
| 触发 QTE 时 ensure `HandleTaskFinished` | 旧版本缺 `UFUNCTION()` 标记 | 拉取最新代码重新编译 |

---

## ✅ 完整资产清单

教程走完后，你的 Content 目录应该是这样：

```
Content/QTEDemo/
├── Input/
│   ├── IA_QTE_Mash
│   ├── IA_QTE_Tap
│   └── IMC_QTEDemo
├── Data/
│   ├── DA_QTE_Mash_Demo
│   └── DA_QTE_Tap_Demo
├── UI/
│   └── WBP_QTEHud
├── Maps/
│   └── Map_QTETest
└── Sequences/
    └── LS_QTEDemo      ← 2 条 Track：Camera Cut + QTE（两个 Section：默认 Key 模式 + 勾选 Range 模式）
```

按这份教程走一遍，就完成了 **Mash 连点** 和 **Tap 时机** 两种 QTE 在 LevelSequence 过场中的插入、慢速、输入、UI、回调的完整闭环测试。

---

## 🔗 相关文档

- 插件总览与 API 参考：[../README.md](../README.md)
- 自动化测试使用与新增：见 README 中的 "CinematicQTETests 模块使用详解" 章节
- Sequencer 编辑器集成细节：见 README 中的 "CinematicQTEEditor 模块使用详解" 章节
