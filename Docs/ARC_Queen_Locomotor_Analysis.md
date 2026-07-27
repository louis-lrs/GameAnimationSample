# ARC Raiders「Queen」移动行为分析 与 UE Locomotor 复刻方案

> 生成日期:2026-07-24
> 目标:拆解 Queen(六足巨型 ARC)的步态分组、落脚节奏、Body 延迟/惯性/倾斜等表现,并给出用 UE **Locomotor 插件**(Experimental,UE 5.6+,本文基于 **UE 5.8 引擎源码** `Engine/Plugins/Experimental/Animation/Locomotor/`)在自研项目中复刻的完整配置方案。

---

## 目录

1. [Queen 原作技术背景:它为什么"不像动画"](#1-queen-原作技术背景)
2. [Queen 运动表现拆解(观察层)](#2-queen-运动表现拆解)
3. [UE Locomotor 插件源码级解析](#3-ue-locomotor-插件源码级解析)
4. [复刻映射:Queen 表现 → Locomotor 参数](#4-复刻映射queen-表现--locomotor-参数)
5. [Control Rig / 工程落地步骤](#5-control-rig--工程落地步骤)
6. [Queen 预设参数速查表](#6-queen-预设参数速查表)
7. [Locomotor 做不到的部分与补偿手段](#7-locomotor-做不到的部分与补偿手段)
8. [参考资料](#8-参考资料)

---

## 1. Queen 原作技术背景

先明确一个关键事实:**ARC Raiders 里 Queen 的腿部运动不是关键帧动画,也不是传统程序化 IK,而是"物理身体 + 强化学习(RL)locomotion 控制器"**:

- Embark 将 ARC 机器人当作**物理实体**(关节 + 虚拟马达),用**深度强化学习**在物理模拟中离线训练出行走控制器;ML 的职责范围**严格限定在 locomotion**(落脚位置、如何迈步),高层决策(去哪、打谁)仍由行为树 / Utility AI 完成。
- 动画质量靠 **Adversarial Motion Priors(对抗性运动先验)** 挽救——这项技术让学出来的步态"像动物/机械该有的样子",项目一度差点被砍。
- 因为身体是物理的,Queen 被掀翻时会像螃蟹一样蹬腿挣扎、被撞会真实位移、断腿后步态会自适应——这些是 RL+物理的**涌现行为**,不是预制动画。
- 引擎侧:游戏基于 Unreal Engine,结合物理系统、强化学习 locomotion 与程序化动画。

**对复刻的启示**:我们用 Locomotor(运动学模拟 + 弹簧阻尼)复刻的是它的**外观特征**(步态节奏、身体惯性),而不是它的生成机制。只要把"重量感"的几个来源(落脚急停、身体滞后、加速前倾、转身缓慢)都做对,视觉上可以非常接近;真正的受击物理反应需要额外用 Physics/RBAN 补(见第 7 节)。

---

## 2. Queen 运动表现拆解

基于游戏内观察与公开影像资料整理(Queen 为六足巨型蜘蛛形 ARC,腿部关节可被逐条打断):

### 2.1 六条腿的步态分组(Gait Grouping)

六足生物/机械有三种经典步态,Queen 的表现介于后两者之间:

| 步态 | 分组 | 占空比(触地时间比) | 适用 |
|---|---|---|---|
| **Tripod(三角步态)** | {L1,R2,L3} ↔ {R1,L2,R3} 两组交替,始终 3 点支撑 | ~50% | 快速移动,昆虫感 |
| **Tetrapod(四足波)** | 每次 2 条腿摆动,4 条支撑 | ~66% | 中速 |
| **Wave / Metachronal(波浪步态)** | 一次只抬 1 条腿,5 条支撑,落脚在时间上均匀错开(1/6 相位间隔) | >80% | 缓慢、沉重、巨型体 |

**Queen 的观察特征:**

- 巡逻/慢速移动时接近 **Wave/Tetrapod**:同一时刻绝大多数腿在地面,落脚一次一响、在时间轴上均匀错开,几乎不出现同侧相邻两腿同时离地——这是"巨物感"的核心来源(真实巨型机械不敢牺牲支撑多边形)。
- 追击/警戒加速时向 **Tripod 方向压缩**:摆动期占比增大,对侧成组的趋势更明显,但受体型限制从不达到昆虫那种干脆的 50/50 交替。
- **左右交替铁律**:同一体节(同一对)的左右腿相位差恒定约 0.5,绝不同时离地。
- **断腿自适应**:腿被打断后剩余腿重新分配相位、步频下降、身体向缺腿侧下沉——RL 涌现行为,Locomotor 需要用"重建 FootSet + 身体偏移"近似。

### 2.2 落脚节奏(Footfall Rhythm)

- **步频极低**:全周期(全部六脚各落一次)约 2–4 秒量级,单腿摆动期约 0.4–0.8 秒。
- **抬脚慢、落脚急**:抬离地面有明显的"拔起"缓入;落地几乎无缓出——脚掌快速砸下并**瞬间锁死**,配合音效/震屏形成 stomp 冲击感。落脚急停正是"重量"的最强视觉信号。
- **踏步承诺(step commitment)**:一旦迈出,落点基本不再大幅修正;转向时宁可多走几个小碎步(重新落脚)也不在空中改方向。
- **原地转向**:身体几乎不平移,六条腿轮流小步重新落位,呈放射状"倒脚"。
- **落点稳定**:脚落下后完全钉在世界空间,身体在其上摆动;不存在脚底打滑(foot sliding)。

### 2.3 Body 延迟 / 惯性 / 倾斜(Body Lag / Inertia / Tilt)

- **位置延迟**:身体(壳体重心)不直接跟随移动目标,而是**跟随支撑腿的"平均位置"**——腿先走,身体被"拖"着走,启动时身体明显滞后于第一批落脚,停止时身体向前多滑一小段再被拉回(过冲回摆)。
- **加速度倾斜(lean)**:加速时壳体前倾/前移(pitch down + lead forward),减速时后仰;量不大但相位明显滞后于速度变化,像被弹簧拽着。
- **转向 roll**:大幅转向时壳体向弯内侧轻微 roll,且转动角速度远低于腿的重新落位速度——身体旋转是**低刚度弹簧**行为。
- **垂直 bob**:随步态周期轻微起伏;移动时整体高度略降(重心下压),静止时缓慢恢复。频率 = 步频的整数倍,幅度相对体型很小(巨物 bob 过大会显得轻飘)。
- **地形贴合**:上下坡时壳体 pitch 跟随支撑面平均法线(部分跟随,不 100% 贴合);侧坡时 roll 跟随,腿长差由各腿 IK 吸收。
- **受击/落脚残余振动**:每次重落脚后壳体有 1–2 个周期的微小衰减振荡(高频低幅),强化"悬挂系统"的感觉。

---

## 3. UE Locomotor 插件源码级解析

插件位置:`Engine/Plugins/Experimental/Animation/Locomotor/`(UE 5.6+ Experimental)。核心是一个 Control Rig 节点 **`Locomotor`**(`FRigUnit_Locomotor`),内部跑一个纯运动学模拟器 `FLocomotor`。

### 3.1 数据流与模拟循环

```
RootControl(世界空间目标 Control,你来驱动)
        │
        ▼
FRigUnit_Locomotor::Execute()
        │  固定 1/120s 子步进(大 DeltaTime 自动切片,支持慢动作)
        ▼
FLocomotor::Simulate()  每子步依次执行:
  1. UpdateWorldSpeedAndPhase()  // 加/减速积分 → CurrentSpeed → 全局相位推进
  2. UpdateFeetTargets()         // 每脚:目标点、想不想迈步、摆动期判定、落点球体扫描贴地
  3. AnimateFeet()               // 摆动插值 + 抬脚高度(正弦)+ ease in/out + 脚跟剥离 + 脚旋转弹簧
  4. UpdateBody()                // Body = 所有脚平位置的平均 → 地形倾斜 → lead 外推 → 旋转弹簧
  5. AnimatePelvis()             // Pelvis 挂到 Body 上 + bob 弹簧(垂直)
        │
        ▼
输出: FeetTransforms[](每脚世界目标) + Pelvis 骨骼被直接写入
        │
        ▼
你自己接: 每条腿一个 IK(FBIK / Basic IK / CCDIK), effector = 对应 FeetTransforms[i]
```

关键实现细节(直接影响调参思路):

- **相位系统**:全局相位 `CurrentGlobalPhase`(0–1 循环)按 `CurrentPhaseSpeed`(cycles/s,由速度在 PhaseSpeedMin/Max 间插值)推进。每只脚的相位 = `全局相位 + FootSet.PhaseOffset + 脚在组内的 StaticPhaseOffset`。
- **组内交替是硬编码的**:`FLocomotorFootSet::AddFoot()` 中,组内第 0/2/4… 只脚 StaticPhaseOffset=0,第 1/3/5… 只 =0.5(左/右/左/右交替)。**这决定了 FootSet 的正确用法:每组放一对左右腿**(见第 4 节)。
- **摆动期判定**:`bInSwingPhase = CurrentPhase < FootPhaseWhenSwingEnds`;`FootPhaseWhenSwingEnds` 在抬脚瞬间锁定 = `PercentOfStrideInAir`(随速度加上 `AirExtensionAtMaxSpeed`)。即**相位前段是摆动、后段是支撑**。
- **落脚锁定**:支撑期脚位置完全冻结(`PlantedWorld`),零滑步。
- **踏步承诺**:摆动中落点目标只允许"更接近最终目标"的更新,且经过一个重阻尼弹簧(stiffness 10, damping 2)缓慢修正——天然模拟"迈出后不能急改方向"。
- **Body 计算**(这就是"延迟/惯性"的来源):
  - `Body 目标位置 = 所有脚 CurrentWorldFlatPositionNoEase 的平均值` → 身体天生滞后于目标、被脚"抬着走";
  - **Lead(惯性前倾)**:取 Body 目标速度 × `LeadAmount` 作为外推量,加速时为正(前倾),**减速时取负**(后仰),再经 `LeadDampingHalfLife` 的半衰期阻尼器平滑 —— 源码 `UpdateBody()`:`LeadAmountToUse = bAccelerating ? LeadAmount : -LeadAmount`;
  - **旋转**:Body 朝向 = RootControl 朝向 + 地面法线 pitch/roll(按 `OrientToGroundPitch/Roll` 比例),经 QuatSpring(`RotationStiffness/RotationDamping`)—— 低刚度 = 沉重的转身延迟;
  - **Bob**:目标 bob = 所有脚当前抬高的平均值 + 移动时的静态下压 `BobOffset`,经 FloatSpring(`BobStiffness/BobDamping`)输出——弹簧欠阻尼时自动产生"落脚后残余振荡"。
- **阻尼器/弹簧实现**:位置用 Daniel Holden 的 Exact Damper(半衰期参数化),旋转/bob 用 UE 的 SpringInterp(stiffness/damping)。
- **地面贴合**:脚与身体都用 SphereCast(`TraceChannel`,`MaxCollisionHeight` 限幅),脚的 pitch/roll 按 `OrientFootToGroundPitch/Roll` 贴合坡面。
- **脚间避让**:`bEnableFootCollision` 把每只脚当作不重叠圆盘(`CollisionRadius`),防止交叉踩踏——六足密集腿必开。
- **已知空缺(5.8 源码确认)**:
  - `Movement.Styles`(Walk/Trot/Gallop)枚举存在但 `GetPhaseOffsetForSetFromMovementStyle()` 是 **TODO,恒返回 0** —— 步态切换目前无效,需自己处理;
  - `FSpineSettings` / `FHeadSettings` 已定义但在节点上被注释(TODO),脊柱前倾/头部朝向要自己在节点后追加;
  - `FootSets` 引脚带 `Constant` 元数据:**PhaseOffset 不能运行时动画化**,改组结构会触发重初始化。

### 3.2 完整参数表(UE 5.8 源码默认值)

**FMovementSettings(Movement)**

| 参数 | 默认 | 含义 |
|---|---|---|
| MinimumStepLength | 10 cm | 小于此距离不迈步(死区) |
| SpeedMax / SpeedMin | 80 / 50 cm/s | 移动速度上/下限 |
| PhaseSpeedMax / PhaseSpeedMin | 4 / 1 cycles/s | 步频上/下限(随速度插值) |
| Acceleration / Deceleration | 100 / 30 cm/s² | 加/减速度(带停车距离预测,提前减速) |
| GlobalTimeScale | 1 | 全局时间缩放(慢动作) |
| bTeleport | false | 置 true 一帧 = 重置模拟(瞬移用) |
| Styles | [] | Walk/Trot/Gallop —— **当前未实现,忽略** |

**FStepSettings(Stepping)**

| 参数 | 默认 | 含义 |
|---|---|---|
| PercentOfStrideInAir | 0.35 | 相位中摆动期占比(0.1–0.9)。**越小触地越久 = 越重** |
| AirExtensionAtMaxSpeed | 0.2 | 满速时额外增加的空中占比(内部总和限 95%) |
| StepHeight | 6 cm | 最大步幅时抬脚峰值高度(实际高度随速度在 20%–100% 之间缩放,正弦曲线) |
| StepEaseIn | 0.5 | 抬脚加速度缓入(0 瞬动,1 满缓) |
| StepEaseOut | 0.2 | 落脚减速缓出(**0 = 瞬停急砸**) |
| bEnableFootCollision / FootCollisionGlobalScale | true / 1.0 | 脚-脚圆盘避让 |
| bEnableGroundCollision / MaxCollisionHeight / TraceChannel | true / 30 / TraceTypeQuery1 | 落点球体扫描贴地 |
| OrientFootToGroundPitch / Roll | 0.8 / 0.5 | 脚掌贴合坡面比例 |

**FPelvisSettings(Pelvis / Body)**

| 参数 | 默认 | 含义 |
|---|---|---|
| PelvisBone | — | 被节点直接驱动的骨骼(整体平移根) |
| PositionDampingHalfLife | 0.1 s | 骨盆到位半衰期(**越大身体越拖沓**) |
| RotationStiffness / RotationDamping | 40 / 0.9 | Body 旋转弹簧(**低刚度 = 转身沉重**) |
| LeadAmount | 2.0 | 惯性前倾量(加速 +,减速自动取 −,即后仰) |
| LeadDampingHalfLife | 0.1 s | 前倾量的平滑半衰期 |
| BobOffset | −8 | 移动时的静态重心下压 |
| BobStiffness / BobDamping | 40 / 0.9 | 垂直 bob 弹簧(**欠阻尼 → 落脚残余振荡**) |
| OrientToGroundPitch / Roll | −0.3 / −0.3 | 身体贴合坡面比例(**负=双足,正=四足/多足**) |

**FFootSet / FFootSettings**

| 参数 | 默认 | 含义 |
|---|---|---|
| FootSet.PhaseOffset | 0 | 该组相对全局相位的偏移(0–1)——**步态分组的核心旋钮** |
| FootSet.Feet[] | — | 组内脚列表,**索引偶/奇自动 +0 / +0.5 相位** |
| Foot.AnkleBone | — | 腿末端骨骼(输出 goal 起点) |
| Foot.CollisionRadius | 10 cm | 脚圆盘半径 |
| Foot.MaxHeelPeel | (0,0,50) | 离地前脚跟剥离最大旋转(机械蹄可设 0) |
| Foot.StaticLocalOffset | (0,0,0) | 参考姿势下的脚局部偏移 |

---

## 4. 复刻映射:Queen 表现 → Locomotor 参数

### 4.1 六腿分组:用 3 个 FootSet,每组一对左右腿

腿编号(从前往后):`L1 R1 / L2 R2 / L3 R3`。利用"组内偶数索引 +0、奇数索引 +0.5"的硬编码规则:

**方案 A —— Wave 波浪步态(推荐,慢速巡逻的"巨物感")**

```
FootSet[0] = { L1, R1 }, PhaseOffset = 0.000
FootSet[1] = { L2, R2 }, PhaseOffset = 0.333   (≈1/3)
FootSet[2] = { L3, R3 }, PhaseOffset = 0.667   (≈2/3)
```

实际每脚相位:`L1=0, R1=0.5, L2=0.333, R2=0.833, L3=0.667, R3=0.167`
→ 六次落脚在时间轴上**均匀间隔 1/6 周期**,一次一响,永远 5 腿支撑(配合低 PercentOfStrideInAir)。这正是 2.2 节描述的落脚节奏。

**方案 B —— Tripod 三角步态(战斗/追击的快速档)**

```
FootSet[0] = { L1, R1 }, PhaseOffset = 0.0
FootSet[1] = { L2, R2 }, PhaseOffset = 0.5
FootSet[2] = { L3, R3 }, PhaseOffset = 0.0
```

实际每脚相位:`L1=0, R1=0.5, L2=0.5, R2=0, L3=0, R3=0.5`
→ {L1,R2,L3} 与 {R1,L2,R3} 两组严格交替 —— 教科书三角步态。

**步态切换**:因 `FootSets` 是 Constant 引脚且 Styles 未实现,运行时无法直接滑变 PhaseOffset。可行做法:
1. **双节点方案**:两个 Locomotor 节点(A/B 两套 FootSet),输出脚 goal 用 alpha 插值切换(切换点选在全体脚触地的瞬间,插值 0.3–0.5s);
2. **只用方案 A + 提高 AirExtensionAtMaxSpeed**:速度快时空中占比自动加大,波浪步态会自然"挤"向三角步态的观感——**最省事,推荐先试这个**;
3. 接管源码:插件很小(3 个 cpp),拷进项目改 `GetPhaseOffsetForSetFromMovementStyle()` 把 Styles TODO 补上,即可按速度自动切组偏移。

### 4.2 落脚节奏 → Stepping/Movement 参数

| Queen 表现 | 参数手段 |
|---|---|
| 步频极低(单腿摆动 0.4–0.8s) | `PhaseSpeedMin=0.25, PhaseSpeedMax=0.6`(cycles/s)。周期 1.6–4s,配合 1/6 错相 = 每 0.3–0.7s 一次落脚 |
| 触地时间长、始终多点支撑 | `PercentOfStrideInAir = 0.15~0.22`(wave);tripod 档用 0.45 |
| 抬脚慢 | `StepEaseIn = 0.7~0.9` |
| **落脚急砸(重量感核心)** | `StepEaseOut = 0.0~0.05` —— 脚以全速命中地面瞬间锁死 |
| 步幅大但不夸张 | 步幅=速度/步频自动得出;`MinimumStepLength` 设大(如 60–100cm)避免碎步抖动 |
| 抬脚高度 | `StepHeight = 80~150`(按 Queen 体型 10m 级估;先取腿长的 8–12%) |
| 移动缓慢、提前减速 | `SpeedMax≈300, SpeedMin≈120, Acceleration≈80, Deceleration≈60`(cm/s;按你项目中 Queen 实际体型缩放) |
| 机械蹄无脚跟剥离 | `MaxHeelPeel = (0,0,0)`(或极小值,金属蹄不弯曲) |
| 六腿不互踩 | `bEnableFootCollision = true`,`CollisionRadius` 按脚掌实际半径(如 60–100cm),必要时调 `FootCollisionGlobalScale` |
| 贴地与坡面 | `bEnableGroundCollision=true`,`MaxCollisionHeight` 提高到 200+(巨型腿跨度大),专用 TraceChannel 忽略自身与小型动态物 |

> **缩放提醒**:插件默认值是人形尺度(cm)。Queen 体型放大 5–10 倍时,所有长度类参数(StepHeight、MinimumStepLength、MaxCollisionHeight、CollisionRadius)等比放大;时间/刚度类参数反而要**调小**(更慢=更重)。

### 4.3 Body 延迟/惯性/倾斜 → Pelvis 参数

| Queen 表现 | 参数手段 |
|---|---|
| 身体滞后于腿、被"抬着走" | 免费获得——Body=六脚平均位置,天然滞后平滑。想更拖沓:`PositionDampingHalfLife = 0.3~0.5` |
| 加速前倾 / 减速后仰(惯性) | `LeadAmount = 3~6`,`LeadDampingHalfLife = 0.3~0.6`(半衰期越长,倾摆越"油腻"、回位越慢)。注意源码只做**位置 lead**,姿态前倾见下方补充 |
| 转身沉重(壳体旋转慢于腿) | `RotationStiffness = 5~12`(默认 40 太灵),`RotationDamping = 0.8~1.0`(临界略欠,允许一点点过冲) |
| 移动时重心下压 | `BobOffset = -(体高的 1~2%)`,如 −15 ~ −30 |
| 落脚后残余振荡(悬挂感) | `BobStiffness = 15~25`、`BobDamping = 0.5~0.7`(**故意欠阻尼**,每次落脚 bob 目标突变 → 1–2 个周期衰减振荡) |
| 坡面贴合(多足用正值) | `OrientToGroundPitch = +0.4, OrientToGroundRoll = +0.3` |

**补充:姿态级前倾/侧倾(源码未覆盖的部分)**
Lead 只平移不旋转。要"加速时壳体 pitch down、转弯时向内 roll",在 Locomotor 节点**之后**追加少量 Control Rig 逻辑:

```
// 伪代码(Control Rig 图中用 Accumulate/Spring Interp 节点实现)
vel        = (BodyPos - PrevBodyPos) / dt
accel      = (vel - PrevVel) / dt
pitchTgt   = clamp(dot(accel, fwd) * kPitch, -6°, +6°)   // 加速低头,减速抬头
rollTgt    = clamp(yawRate * speed * kRoll,  -4°, +4°)   // 向弯内侧 roll
pelvisRot  = SpringInterp(pelvisRot, pitchTgt+rollTgt, stiffness≈8, damping≈0.9)
```

角度别超过 ±6°:巨物的倾斜靠"慢"而不是靠"大"。

### 4.4 落脚冲击反馈(节奏的"可听化/可感化")

Locomotor 不输出落脚事件,自己检测:在 Control Rig 或 AnimBP 里缓存每脚上一帧位置,当**该脚从移动转为静止**(或 `CurrentPhase` 跨过 `FootPhaseWhenSwingEnds`)时广播事件 →
- 触发地面 Decal / 尘土 Niagara / 音效;
- 按脚与相机距离触发 CameraShake(Queen 的压迫感一半来自这里);
- 可选:给壳体 bob 弹簧目标叠加一个 −2~−5cm 的瞬时脉冲,模拟"落脚砸得身体一沉"。

简单实现:把每脚的 `FeetTransforms[i].Z` 与速度写进 Anim Curve,在 AnimBP 事件图中检测由动转静的过零点。

---

## 5. Control Rig / 工程落地步骤

### 5.1 前置

1. UE 5.6+(建议 5.8),`Edit > Plugins` 启用 **Locomotor**(Experimental)重启。
2. Queen 骨架要求:一个 Pelvis/Body 根骨 + 6 条独立腿链(每链 2–4 节 + 末端 ankle/ball 骨),命名建议 `leg_l1..l3 / leg_r1..r3`。

### 5.2 Control Rig 装配

1. 新建 Control Rig(目标 Queen 骨架)。
2. 创建一个世界空间 Control:`ctrl_root_goal`(移动目标,initial 放在角色原点)。
3. Forward Solve 图:
   ```
   BeginExecution
     → Locomotor 节点
         RootControl  = ctrl_root_goal
         Pelvis.PelvisBone = body/pelvis 骨
         FootSets     = 按 4.1 方案 A 填 3 组×2 脚(AnkleBone = 各腿末端)
         Movement/Stepping/Pelvis = 第 6 节速查表
     → (可选)姿态倾斜追加逻辑(4.3 补充)
     → 每条腿: Basic IK / FBIK
         Effector[i] = FeetTransforms[i]   (Locomotor 输出数组,顺序=FootSets 填表顺序)
         注意 FeetTransforms 是世界空间 → 先用 To Rig Space / From World 转换
     → (可选)Aim 节点让小腿/踝对准落点法线
   ```
4. 打开节点 `Debug.bDrawDebug` 看相位圆盘与脚目标,先在 CR 编辑器里拖 `ctrl_root_goal` 验证步态分组正确(观察 6 脚落地顺序:L1→R3→L2→R1→L3→R2 均匀错开即为 wave)。

### 5.3 游戏侧集成(重要:两种策略)

Locomotor 是**目标驱动**的:它把角色"拉向" RootControl,自带加减速。社区已知它与 CharacterMovementComponent 直接叠加会**滞后/打架**。

- **策略 A(推荐,Boss 适用):Locomotor 主导移动。**
  Actor 本体不做平滑移动;AIController/行为树把导航路径点(或玩家位置的预测点)写入 `ctrl_root_goal` 的世界变换(通过 AnimBP → Control Rig 节点的 Control 输入,或 `SetControlTransform`)。胶囊/根组件用低频跟随 mesh 的 Body 位置(每帧把 Actor 位置慢速插向 Body 投影点,只为碰撞与网络同步)。Queen 是 AI Boss,不需要 CMC 的手感,这条路最干净,与原作"动画即移动"的思路一致。
- **策略 B:CMC 主导。**
  CMC 正常移动胶囊,`ctrl_root_goal` 每帧设为"胶囊前方 N 米的地面点";需保证 `SpeedMax ≥ CMC MaxWalkSpeed × 1.2`、`Acceleration` 足够大,否则腿追不上身体。适合已有移动管线不想改的项目,但要接受微小滞后。
- 传送/出生:瞬移当帧把 `Movement.bTeleport` 置 true 一帧。

### 5.4 断腿表现(近似 Queen 的 gameplay 联动)

Locomotor 不支持运行时增删脚,近似做法:
- 预建 4 套 FootSet 配置的 CR 变体(6 腿 wave / 5 腿 / 4 腿…),断腿时切换 AnimBP 中的 Control Rig 资产(切换瞬间 bTeleport 重置 + 全屏尘土遮掩);
- 或改源码:暴露 `FLocomotor::Reset/AddFootSet` 的运行时重建,并在重建时保留当前脚位;
- 断腿侧配合:该侧 `Pelvis` 追加固定 roll 偏移(壳体下沉)、`PhaseSpeed` 上限下调 30%(瘸腿更慢)。

---

## 6. Queen 预设参数速查表

以"体长约 8–12m、腿展 ~15m 的六足 Boss"为基准起点(数值单位 cm/s/deg,请按自己资产比例整体缩放长度类参数):

```ini
; ================= Movement =================
MinimumStepLength      = 80
SpeedMax               = 300        ; 巡逻 ~3 m/s
SpeedMin               = 100
PhaseSpeedMax          = 0.6        ; cycles/s → 最快 1.7s 一个全周期
PhaseSpeedMin          = 0.25       ; 最慢 4s 一个全周期
Acceleration           = 80         ; 起步拖沓
Deceleration           = 60         ; 提前缓停
GlobalTimeScale        = 1.0

; ================= Stepping =================
PercentOfStrideInAir   = 0.18       ; wave 步态,5 腿常驻支撑
AirExtensionAtMaxSpeed = 0.25       ; 加速时自动向 tripod 观感靠拢
StepHeight             = 120
StepEaseIn             = 0.8        ; 拔脚缓
StepEaseOut            = 0.02      ; 落脚砸(重量感核心)
bEnableFootCollision   = true
FootCollisionRadius    = 80         ; 每脚 (FFootSettings)
bEnableGroundCollision = true
MaxCollisionHeight     = 250
MaxHeelPeel            = (0,0,0)    ; 金属蹄不剥离

; ================= FootSets(wave 步态)=================
Set0 { L1, R1 }  PhaseOffset = 0.000
Set1 { L2, R2 }  PhaseOffset = 0.333
Set2 { L3, R3 }  PhaseOffset = 0.667
; 落脚顺序: L1 → R3 → L2 → R1 → L3 → R2(1/6 均匀间隔)

; ================= Pelvis / Body =================
PositionDampingHalfLife = 0.35      ; 身体拖沓滞后
RotationStiffness       = 8         ; 转身沉重
RotationDamping         = 0.9
LeadAmount              = 4.5       ; 加速前移/减速后坐
LeadDampingHalfLife     = 0.45
BobOffset               = -20       ; 移动时重心下压
BobStiffness            = 18        ; 欠阻尼 →
BobDamping              = 0.6       ;   落脚残余振荡(悬挂感)
OrientToGroundPitch     = 0.4       ; 多足用正值
OrientToGroundRoll      = 0.3
```

**调参顺序建议**:① 先关 Debug 以外全部附加效果,只调 FootSet/相位,确认落脚顺序;② 调 PhaseSpeed/PercentOfStrideInAir 定节奏;③ 调 StepEaseOut→0 找"砸地感";④ 最后开 Pelvis 各弹簧,从过阻尼往欠阻尼退,直到落脚后刚好看得见 1 次回弹。

---

## 7. Locomotor 做不到的部分与补偿手段

| 缺口 | 原作机制 | 补偿方案 |
|---|---|---|
| 受击物理反应/失衡/被掀翻 | RL+全物理身体 | AnimBP 末端加 **RigidBody(RBAN)** 节点做壳体/天线/挂件的次级物理;重击时 Physical Animation Component 局部混合;掀翻属剧情级,单做一段受击 montage |
| 断腿后步态自适应 | RL 涌现 | 第 5.4 节的 CR 变体切换 |
| 步态随速度真正切换 | 连续控制器 | AirExtension 近似 / 双节点混合 / 改源码补 Styles(第 4.1 节) |
| 脊柱/头部程序化 | 整机学习 | 源码中 Spine/Head 是 TODO——自己在节点后加 Aim/Spring 链(炮塔、传感器天线跟随目标,滞后 0.2–0.4s 更有机械感) |
| 腿部中间关节姿态 | 物理 | 由你的 IK 解算器负责;多节机械腿建议 FBIK + 每节角度限制,避免"果冻腿" |
| 落脚冲击事件 | 物理接触 | 第 4.4 节的相位过零检测 |
| 性能 | — | 单节点每子步 6 次以上 SphereCast;Boss 单体无压力,若复用到小怪群注意 LOD:远处关 GroundCollision/FootCollision |

另外注意:插件标记 **Experimental**,升级引擎版本时参数/行为可能变动(5.7 与 5.8 的默认值已有差异,如 BobOffset 由 10 改为 −8)。

---

## 附录:CR_Salt 实际调参记录(2026-07-24)

已通过编辑器 MCP bridge + Remote Control API(`RigVMModel_Controller.SetPinDefaultValue`)将 Queen 预设写入 `Content/Temp/Arc/Salt/CR_Salt.uasset` 并保存。

**发现的核心问题:原配置把三角步态的两组腿直接填进 2 个 FootSet({L1,R2,L3} / {R1,L2,R3}),叠加组内 0/0.5 硬编码交替后,实际相位变成左三腿全 0、右三腿全 0.5 —— 意外的同侧 pace 步态(划船摇摆)。**

| Pin | 旧值 | 新值(Queen 预设) |
|---|---|---|
| FootSets | 2 组×3 脚 {L1,R2,L3}+0 / {R1,L2,R3}+0.5(实效=同侧步态) | **3 组×2 脚** {L1,R1}+0 / {L2,R2}+0.333 / {L3,R3}+0.667 → wave 波浪步态,落脚均匀间隔 1/6 周期 |
| Movement.Speed | Min=Max=60000(动态全灭) | Min=150 / Max=600,Accel 60000→**180**,Decel 1000→**120**,MinStep 220→250 |
| Movement.PhaseSpeed | 0.85/0.85 恒定 | **0.30–0.70**(步频随速度变化,周期 1.4–3.3s) |
| Stepping | AirPct=0.42, EaseIn=0.45, **EaseOut=0.28**, StepHeight=180, FootColScale=5.0, AirExt=0.03 | AirPct=**0.20**, EaseIn=**0.80**, EaseOut=**0.05**(砸地急停), StepHeight=150, FootColScale=**1.25**, AirExt=**0.25** |
| Pelvis | HalfLife=0.1, RotStiff=30, Lead=2.5/0.2, Bob=-40/50/0.75 | HalfLife=**0.30**(身体拖沓), RotStiff=**9**(转身沉重), Lead=**4.5/0.45**(惯性前倾/后坐), Bob=**-30/18/0.6**(欠阻尼残余振荡) |
| BP_Salt.CharMoveComp | MaxWalkSpeed=1000 | **500**(与 Locomotor 速度域匹配;原来 60000 的 hack 就是为了追 1000 的 Pawn) |

---

## 8. 参考资料

- [Embark Studios × Unreal Engine 开发者访谈(物理系统 + 强化学习 locomotion + 程序化动画)](https://www.unrealengine.com/developer-interviews/embark-studios-build-the-award-winning-arc-raiders-with-unreal-engine)
- [Embark:Transforming animation with machine learning(Tom Solberg,RL 教 AI 走路的原始博文)](https://medium.com/embarkstudios/transforming-animation-with-machine-learning-27ac694590c)
- [80 Level:ARC Raiders 敌人 ML locomotion 技术访谈(Martin Singh-Blom;RL 仅用于落脚/移动、Adversarial Motion Priors、行为树分工)](https://80.lv/articles/inside-the-magic-of-machine-learning-that-powers-enemy-ai-in-arc-raiders)
- [GamesRadar:ARC 的多"脑"与实时 reward locomotion(GDC 演讲综述)](https://www.gamesradar.com/games/third-person-shooter/the-arc-in-arc-raiders-have-multiple-brains-and-they-all-love-pursuing-you-because-embark-gives-them-rewards-in-real-time-via-machine-learning-i-learned-to-walk-on-my-own/)
- [PC Gamer:Queen 被掀翻蹬腿——物理+ML 平衡的实证](https://www.pcgamer.com/games/third-person-shooter/arc-raiders-physics-dont-get-better-than-this-one-in-a-million-shot-of-a-queen-getting-suplexed-by-a-probe/)
- [PC Gamer:Embark 澄清 ML 范围(仅 locomotion,非实时学习)](https://www.pcgamer.com/games/third-person-shooter/congrats-you-played-yourself-arc-raiders-machines-arent-actually-learning-thats-just-the-way-we-author-them/)
- [Epic 官方教程:Procedural Animation with a Locomotor](https://dev.epicgames.com/community/learning/tutorials/EkxO/unreal-engine-procedural-animation-with-a-locomotor)
- [Epic 公开路线图:Rig Locomotor(Experimental)](https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/2010-rig-locomotor-experimental-)
- [unreal.RigUnit_Locomotor Python API(参数结构)](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/RigUnit_Locomotor?application_version=5.7)
- [UE 论坛:Locomotor 与 CharacterMovement 的滞后问题与 alpha 混合 workaround](https://forums.unrealengine.com/t/locomotor-plugin-with-character-movement/2599597)
- [Locomotor 蜘蛛/四足视频教程(spider)](https://www.youtube.com/watch?v=uhjN4jf3q6k) · [双足/四足 setup](https://www.youtube.com/watch?v=tamtEO47v-w) · [Wolf setup](https://www.youtube.com/watch?v=hXPBeY4_YxE)
- [Daniel Holden:Spring Roll Call(Exact Damper——插件 Body 阻尼的数学来源)](https://theorangeduck.com/page/spring-roll-call)
- 引擎源码(本机):`C:/Program Files/Epic Games/UE_5.8/Engine/Plugins/Experimental/Animation/Locomotor/Source/Locomotor/`(`RigUnit_Locomotor.h`、`LocomotorCore.h/.cpp`、`RigUnit_Locomotor.cpp`)
