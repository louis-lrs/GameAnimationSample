# CinematicQTE 开发记录 · 2026-04-17

> 本文件用于跨会话的上下文交接。下次继续开发前请先通读本篇，再结合最新代码确认现状。

---

## 1. 项目背景

- **插件名**：`CinematicQTE`（位于 `Plugins/CinematicQTE/`）
- **宿主项目**：`GameAnimationSample`（UE 5.5）
- **目标**：在 `Sequencer` 时间轴中以 Section 的形式编辑 QTE 事件，运行时由 `UCinematicQTESubsystem` 驱动，UI 由 `UQTEWidgetBase` 派生实现
- **当前阶段**：QTE Section 的编辑器交互设计打磨，尚未正式接入业务，**无历史兼容包袱**，以"最佳实践"为准

相关模块：
- Runtime：`Source/CinematicQTE/`（Subsystem、Task、Widget、DataAsset、MovieSceneQTESection/Template）
- Editor：`Source/CinematicQTEEditor/`（`MovieSceneQTETrackEditor` + 新增的 `QTESectionInterface`）

---

## 2. 当前开发状态

### ✅ 已完成（本轮最终方案）

统一 QTE Section 的创建入口 —— 只有一个右键菜单项 **"Add QTE Section"**，通过 Section 上的布尔字段 `bUseSectionRangeAsDuration` 显式切换两种运行模式。

- **Commit**：`d45906b`
- **Title**：`refactor(qte): unify QTE Section creation into a single entry with explicit duration flag`
- **Diff 规模**：13 files changed, 353 insertions(+), 55 deletions(-)
- **已 push 到** `origin/main`（range：`4dc38c5..d45906b`）

### 🧭 方案演化脉络（避免再走回头路）

| 阶段 | 方案 | 结论 |
| --- | --- | --- |
| 初版 | 单 Section + `bUseSectionRangeAsDuration` 勾选框 | 功能 OK，但 UI 看不出区别，用户体验差 |
| 中间 | 拆成两个菜单项：**Add QTE Key**（菱形、AnimNotify 风格） / **Add QTE Range**（色块、AnimNotifyState 风格），用 `UMovieSceneSection::IsLocked` 标记 Key | **被否决**：`IsLocked` 是 UE 原生"全锁"，Key Section 既不能拖位置、也不能改 Property，策划无法使用 |
| **最终（本次）** | **回到单入口 + 勾选框**，彻底不使用 `IsLocked`；Key 的菱形视觉通过自定义 `FQTESection::IsKeyMode()` 读取 flag 来保留 | **采纳**：交互不受限，视觉仍可区分 |

---

## 3. 技术实现细节

### 3.1 核心字段

`Plugins/CinematicQTE/Source/CinematicQTE/Public/Sequencer/MovieSceneQTESection.h`

```cpp
UCLASS(MinimalAPI)
class UMovieSceneQTESection : public UMovieSceneSection
{
    GENERATED_BODY()
public:
    UMovieSceneQTESection(const FObjectInitializer&);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    TObjectPtr<UQTEDataAsset> QTEDataAsset = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    EQTEConflictPolicy ConflictPolicy = EQTEConflictPolicy::Ignore;

    /** false=用 DataAsset.Duration；true=用 Section 长度，超时按 Timeout 结束 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QTE")
    bool bUseSectionRangeAsDuration = false;
};
```

**关键约束**：绝对不使用 `IsLocked()` 来区分模式——那是上一轮被否决的方案。

### 3.2 模式对照

| 模式 | `bUseSectionRangeAsDuration` | 运行时长来源 | 时间轴可视 | 视觉风格（Sequencer） |
| --- | --- | --- | --- | --- |
| Key 模式（默认） | `false` | `QTEDataAsset->Duration` | Section 宽度仅为占位，不影响运行时 | **菱形**（AnimNotify 风格） |
| Range 模式 | `true` | Section 的长度 | Section 宽度即运行时长；播放头越过终点时若 QTE 仍在运行按 **Timeout** 结束 | 常规矩形色块（AnimNotifyState 风格） |

### 3.3 运行时读取（Template）

`Source/CinematicQTE/Private/Sequencer/MovieSceneQTESectionTemplate.cpp`
- `FMovieSceneQTESectionTemplate::bUseRange` 现在从 `Section.bUseSectionRangeAsDuration` 取值，**不再**从 `IsLocked()` 反推。
- `CinematicQTESubsystem` 在 `bUseRange=true` 时以 Section 长度覆盖 `DataAsset.Duration`；在 `bUseRange=false` 时走原逻辑。

### 3.4 编辑器交互

`Source/CinematicQTEEditor/Private/MovieSceneQTETrackEditor.cpp`
- `BuildTrackContextMenu` 只添加一个条目：**Add QTE Section**
- 默认创建的 Section：**长度 0.25s**，`bUseSectionRangeAsDuration = false`（即默认 Key 模式），**永不 Lock**
- 用户如需 Range 模式：选中 Section → Details 面板勾选 `Use Section Range As Duration`，然后按常规方式拖拽两端调整长度

### 3.5 自定义 Section 可视（菱形保留）

新增文件：
- `Source/CinematicQTEEditor/Private/QTESectionInterface.h`
- `Source/CinematicQTEEditor/Private/QTESectionInterface.cpp`

职责：
- `FQTESection::IsKeyMode()` 读取 `bUseSectionRangeAsDuration`，为 `false` 时绘制菱形图标；为 `true` 时走默认矩形绘制
- **不**改变 Section 的可拖动/可编辑属性（这些都由 UE 默认行为保证，不要再动 `IsLocked`）

---

## 4. 关键文件列表（本 commit 涉及）

### 修改（11）

**Runtime（CinematicQTE）**
- `Source/CinematicQTE/Private/CinematicQTESubsystem.cpp`
- `Source/CinematicQTE/Public/CinematicQTESubsystem.h`
- `Source/CinematicQTE/Private/QTETaskBase.cpp`
- `Source/CinematicQTE/Public/QTETaskBase.h`
- `Source/CinematicQTE/Public/QTEWidgetBase.h`
- `Source/CinematicQTE/Private/Sequencer/MovieSceneQTESectionTemplate.cpp`
- `Source/CinematicQTE/Public/Sequencer/MovieSceneQTESection.h` ← **新增 flag 字段**
- `Source/CinematicQTE/Public/Sequencer/MovieSceneQTESectionTemplate.h`

**Editor（CinematicQTEEditor）**
- `Source/CinematicQTEEditor/Private/MovieSceneQTETrackEditor.cpp` ← **合并菜单入口**
- `Source/CinematicQTEEditor/Public/MovieSceneQTETrackEditor.h`

**Docs**
- `Plugins/CinematicQTE/Docs/GettingStarted.md`（Step 6.3/6.4/6.5、预期行为表、排错表全部同步）

### 新增（2）
- `Source/CinematicQTEEditor/Private/QTESectionInterface.h`
- `Source/CinematicQTEEditor/Private/QTESectionInterface.cpp`

---

## 5. 待办 / 下一步

### P0 · 功能验证
- [ ] 在编辑器中新建一条 QTE Track，右键 → Add QTE Section，默认得到 0.25s 的菱形 Section
- [ ] 选中 Section → 可拖位置、可拖两端改长度、Details 面板可改 `QTEDataAsset` / `ConflictPolicy` / `bUseSectionRangeAsDuration`
- [ ] 勾选 `Use Section Range As Duration` → 图标切换为矩形（或保留矩形），播放后运行时长 = Section 长度
- [ ] 不勾选时，运行时长 = `DataAsset.Duration`，与 Section 长度无关
- [ ] Range 模式下播放头越过终点时，QTE 若未完成按 Timeout 正确结束

### P1 · UX 打磨（可选）
- [ ] 考虑在 Section 缩略图/Tooltip 上直接标注当前模式（例如 `[Key]` / `[Range]` 角标），减少需要打开 Details 面板确认的场景
- [ ] 考虑在 Details 面板上，勾选 `bUseSectionRangeAsDuration` 时把 `QTEDataAsset.Duration` 字段做灰化/提示说明
- [ ] 考虑右键菜单增加"Toggle Section Duration Mode"的快捷切换项

### P2 · 文档
- [ ] 对 `README.md` 做一次总览更新，把"单入口 + 勾选框"的结论体现到顶部示例/截图说明里（当前仅 `Docs/GettingStarted.md` 同步了）
- [ ] 录一段短 gif 放 `Docs/`，展示两种模式的播放差异

---

## 6. 注意事项（给下一次的自己）

1. **不要再用 `IsLocked` 区分模式**——已被用户明确否决，会破坏拖动/编辑交互。
2. **不要回到"两个菜单项"方案**——视觉区分价值低于交互一致性损失。
3. 本轮 commit **只包含 `Plugins/CinematicQTE/` 下改动**，以下文件/目录**被刻意排除**、不要误提交：
   - `Audio2Face-3D-SDK/`、`.codebuddy/`、`.cursor/`、`.shared/`
   - `Docs/plan1.md`、`complete_setup.ps1`、`setup_audio2face_sdk.ps1`
   - 若干根目录的中文 md/txt（安装报告、进度总结等）
   - `.gitignore` 的 CRLF 变动
4. `Docs/GettingStarted.md` 已与代码同步，**改代码时请一并维护该文档**（Step 6.3/6.4/6.5、预期行为表、排错表）。
5. 默认 Section 长度在 `MovieSceneQTETrackEditor.cpp` 里硬编码为 **0.25s**，后续若要可配置，统一走 ProjectSettings 或 DeveloperSettings，不要散落常量。
6. 本文件仅为会话交接说明，**不要作为对外设计文档**；面向用户的规范入口仍是 `README.md` + `Docs/GettingStarted.md`。

---

## 7. 快速恢复指令（给下一次的自己）

```bash
# 当前分支 & 最新 commit
git log -n 3 --oneline
# 应该能看到 d45906b 在顶部或附近

# 检查工作区是否干净
git status
```

如果需要基于本方案继续迭代，建议起点：
1. 打开 `Source/CinematicQTEEditor/Private/MovieSceneQTETrackEditor.cpp` 复核 "Add QTE Section" 菜单的创建路径
2. 打开 `Source/CinematicQTE/Public/Sequencer/MovieSceneQTESection.h` 复核字段与注释
3. 打开 `Docs/GettingStarted.md` 的 Step 6 小节，对齐用户视角

— EOF —
