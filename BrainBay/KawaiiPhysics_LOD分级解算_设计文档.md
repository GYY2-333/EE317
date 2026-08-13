# LOD 分级解算体系 · 开发设计文档

> 目标插件：UE5.5 KawaiiPhysics（骨骼物理软骨模拟）
> 适用场景：多角色同屏场景下软骨模拟的规模化性能优化

---

## 1. 背景与问题定义

当前披风解算对远近物体"一视同仁"：8 模型约 1.11ms/帧（Worker 累加）尚属温和，但 30+ 同屏角色时 Worker 累加约 4.2ms/帧，叠加其他并行系统后易触发 Oversubscription，沿签收点 / EOF 同步点传导至 GameThread 造成掉帧。而远景披风的穿插不可见，全量防穿是纯浪费。

**目标**：按视觉重要性分级分配算力——降级的是防穿，不是动感——实现产品级规模能力。

---

## 2. 关键技术点（含实现逻辑）

本节把每个技术点拆成「是什么 → 解决什么 → 实现逻辑（代码级）→ 为什么这样更好」四层。

### 技术点 1：屏幕占比判定（Screen-size Metric）

**是什么 / 解决什么**：视觉重要性正比于**物体在屏幕上占多大**，而非绝对距离——大披风 30m 外仍显眼，小披风 5m 外也不起眼，裸距离会误判。

**实现逻辑**：
```text
ScreenSize ≈ Bounds.SphereRadius / DistanceToView
// Bounds 取 SkelComp->GetCachedComponentSpaceBounds()（GameThread 安全）
// DistanceToView = |组件位置 - 本地视图位置|
LOD 等级 = 分段函数(ScreenSize)：>=0.05→0；[0.01,0.05)→1；<0.01→2；不可见→3
```

**为什么更好**：优先复用引擎现成的 `VisibilityBasedAnimTickOption` / Significance Manager / URO 的同类度量，让**物理 LOD 与动画 LOD 档位对齐**，行为一致、不重复造轮子。

### 技术点 2：线程分界（GameThread 判定，Worker 只读）

**是什么 / 解决什么**：UE 动画分两阶段——`Update`（GameThread）→ `Evaluate`（Worker 并行）。**Worker 不能碰 UObject / 场景查询 / 摄像机**（非线程安全，会崩或数据竞争）。

**实现逻辑**：
```text
GameThread 侧（组件 Tick 或 Update_AnyThread，需 IsInGameThread() 断言）：
    算 ScreenSize → 滞回 → CurrentLODLevel.Store(level)   // TAtomic<int32>
Worker 侧（Evaluate_AnyThread 帧首）：
    const int32 LOD = CurrentLODLevel.Load();             // 读一次到局部常量，防帧内撕裂
    // 之后全程只用局部 LOD，绝不触碰任何 UObject
```

**为什么更好**：`TAtomic` 保证 Worker 读到完整值；这与插件已有的 `CachedSharedCollisionSubsystem`（GameThread 解析指针、Worker 只读，AnimNode_KawaiiPhysics.h:760）是同一套架构语言，扩展像"长"出来的。

### 技术点 3：滞回状态机（Hysteresis）

**是什么 / 解决什么**：物体停在档位边界（正好 10m）时，每帧微动会**反复跨阈值** → 反复切档 → 防穿约束反复上下线 → 披风抽搐、性能颠簸。

**实现逻辑（进/出用不同阈值）**：
```text
int32 ResolveLOD(float screenSize, int32 prevLOD):
    for 每个候选档位 L:
        enterTh = LODScreenSize[L]                       // 升档阈值
        exitTh  = LODScreenSize[L] / HysteresisRatio     // 降档阈值（1.15 倍死区）
        if 升档方向: 需 screenSize < enterTh 才升到更远档
        if 降档方向: 需 screenSize > exitTh  才降回更近档
    return 落在死区内则维持 prevLOD                       // 关键：死区内不切
```

**为什么更好**：这就是电子学的**施密特触发器（Schmitt Trigger）** / 温控滞环——用死区吸收边界抖动，跨领域同源思想。

### 技术点 4：使用时缩放（不可变缓存 + 派生视图）

**是什么 / 解决什么**：`DirectionalCollisionCount`、`Pair.Count` 是初始化固化的"真值缓存"。若 LOD 降级时直接把 Count 从 4 改成 2，切回近景时原值 4 已丢失、无法还原。

**实现逻辑（读取处乘系数，原值永不动）**：
```text
// 错误做法：Pair.Count = Pair.Count / 2;             ← 污染真值
// 正确做法（使用时算有效值）：
int32 effectiveCount = (LOD == 1)
        ? FMath::Max(1, FMath::RoundToInt(Pair.Count * LOD1ProbeScale))
        : Pair.Count;
for (i = 1; i <= effectiveCount; ++i) { ... 布探针 ... }
```

**为什么更好**：这是"**不可变数据 + 派生视图**"模式——原始配置保持权威，运行时状态是它的临时函数，杜绝"改了状态回不去"的一类 bug。

### 技术点 5：平滑切换（WarmUp + Alpha 渐变）

**是什么 / 解决什么**：切档有两类视觉跳变，来源不同、对策不同。

**实现逻辑**：
```text
① 速度尖峰（LOD3 休眠 → LOD2 恢复解算）：
   休眠期 PrevLocation 过期 N 帧，Verlet 重建速度 (now-old)/dt 爆炸
   → 切出休眠帧调用 WarmUp()（Simulation.cpp:780）：PrevLocation=Location、Pose 重置
   → 速度从零干净起步（同 teleport 处理）

② 约束突现（LOD2 → LOD1，防穿约束突然上线把穿插的链一把推开）：
   → 切档后 5~10 帧内让节点 Alpha 从 0 渐变到 1（复用 AnimNotifyState_KawaiiPhysicsSetAlpha 机制）
   → 物理结果与动画姿势按 α 插值，推挤被摊到多帧，肉眼不可察
```

**为什么更好**：任何"状态突变"视觉上都表现为跳变，通用平滑只有两招——**重置到干净初值（WarmUp）** 或 **把突变摊到时间上（Alpha 渐变）**——这里两招各治一类。

---

## 3. 算法与机制详解

### 3.1 分档策略

| 档位 | 判定（屏幕占比） | 保留 | 降级 / 关闭 |
|------|------|------|----------|
| LOD0 近景 | >= 0.05 | 全量 | 无 |
| LOD1 中景 | 0.01 ~ 0.05 | 积分 + 骨球碰撞 | 探针 Count×0.5、XPBD 迭代 = 1 |
| LOD2 远景 | < 0.01 | 积分 + 骨长恢复（披风照飘） | 碰撞与双探针全停 |
| LOD3 休眠 | 不可见 / 极小 | 骨骼运动学跟随姿势 | 全部（开销约 0） |

**设计灵魂**：降级的是防穿，不是动感——远处披风照样飘（积分与骨长恢复很便宜），只是不再花大钱防穿。

### 3.2 滞回状态机

```text
进入更高档位：Dist > Enter阈值 才升档
退出回低档位：Dist < Exit阈值（= Enter × 1.15）才降档
=> 进出不同阈值，消除边界来回切换
```

### 3.3 闸控映射（运行时缩放，不改原值）

| 步骤 | 位置 | 逻辑 |
|------|------|------|
| 3. 默认球碰撞 | Simulation.cpp:400 | LOD >= 2 整步跳过 |
| 4. 方向探针 | Simulation.cpp:430 | LOD1：有效 Count = max(1, 原值×0.5)；LOD >= 2 跳过 |
| 6. 相邻链探针 | Collision.cpp:771 | 同上；LOD3 可隔帧（GFrameCounter % 2） |
| 8. XPBD 迭代 | Simulation.cpp:494 | LOD1 clamp = 1 |
| 9. 骨长恢复 | Simulation.cpp:508 | **永不跳过**（形态正确性兜底，防远景披风被拉成面条） |

**为什么缩放发生在使用时**：`DirectionalCollisionCount`、`Pair.Count` 是初始化固化的缓存，直接改会导致 LOD 切回时无法还原。运行时读原值×系数，原值永不改动。

---

## 4. 在插件中的运行流程

```text
GameThread（每帧，组件 Tick / AnimUpdate 阶段）
  - ScreenSize = Bounds半径 / 视距（一次减法一次除法，成本可忽略）
  - 滞回状态机 -> CurrentLODLevel（TAtomic 写入）

Worker：Evaluate_AnyThread（每帧）
  - 帧首 Load 一次到局部常量（防帧内撕裂）
  - LOD3 -> Location = PoseLocation 运动学跟随 -> 回写 -> 收工
  - 否则 SimulateModifyBones：
      1-2. 照常（积分，动感保留）
      3/4/6. 按档闸控（跳过 / 半密度）
      8. 迭代降级
      9. 永远执行（骨长恢复）
  - 切档瞬间：平滑器介入

线程安全铁律：Worker 不碰 UObject；Bounds / 视距一律 GameThread 预取
（与 CachedSharedCollisionSubsystem 同款模式，AnimNode_KawaiiPhysics.h:760）
```

---

## 5. 实现路径

```text
Day 1  判定器：ScreenSize 计算 + TAtomic 成员 + IsInGameThread 断言验证
Day 2  闸控器：4 个跳过点 + 使用时缩放（原值不动）
Day 3  滞回状态机 + Category="KawaiiPhysics|LOD" 配置全套 UPROPERTY
Day 4  平滑器：切档 WarmUp 接入 + Alpha 渐变过渡（复用 SetAlpha 机制）
Day 5  30 角色压测 + Insights 验证 + 切档逐帧检查 + 单元测试
```

**配置参数**：`bUseLOD` / `LOD1ScreenSize`(0.05) / `LOD2ScreenSize`(0.01) / `LODHysteresisRatio`(1.15) / `LOD1ProbeScale`(0.5) / `LOD1ConstraintIterations`(1)。

---

## 6. 难点与解决方案

| 难点 | 解决方案 |
|------|---------|
| Worker 线程不能访问 UObject / 摄像机 | 判定全部前置 GameThread，Worker 只读原子 int；优先复用引擎 `VisibilityBasedAnimTickOption` / Significance 信号，不造平行轮子 |
| 档位边界抖动 | 滞回：进出阈值分离（×1.15） |
| 休眠恢复时 Verlet 速度爆炸 | 切出 LOD3 时调用 WarmUp（Simulation.cpp:780 现成函数），Prev / Pose 重置，速度从零起步 |
| 防穿约束上线瞬间披风"弹开" | Alpha 渐变把推挤稀释到 5~10 帧，肉眼不可察 |
| 服务器无摄像机 / 多视角 | 服务器取最近玩家距离或强制封顶 LOD1；过场动画强制 LOD0 |
| 误判导致近景降质 | 判定按各客户端本地视图，LOD0 为默认安全档 |

---

## 7. 验证方案

| 验证项 | 方法 | 标准 |
|--------|------|------|
| 规模提升 | 30 / 50 角色压测 + Insights | Worker 累加耗时降幅 >= 40% |
| 近景无回归 | 近景 8 模型 40 帧 trace | 与现状曲线重合 |
| 切档无跳变 | 匀速远离 / 靠近镜头过档 | 逐帧无位置突变 |
| 远景可接受 | 远景录像目测 | 飘动自然、该距离下无可见穿插 |

**预期效果量化**：30 模型（5 近 / 10 中 / 15 远）从约 4.2ms/帧 降至约 2.4ms/帧（-45%）；50 模型大场面省 50%+；8 模型现状规模约 -10%。核心价值是帧率"保险丝"：保护 Worker 池余量，低配机与大场面不掉帧。

---

## 8. 简历缩略版

> 为 UE5.5 KawaiiPhysics 骨骼物理插件设计并实现分级 LOD 解算体系：以屏幕占比驱动、滞回状态机防抖的档位判定（GameThread 判定、Worker 线程原子量零成本读取），按档位对碰撞检测、探针密度与约束迭代做降级 / 休眠闸控，并以 WarmUp 重置与 Alpha 渐变消除切档跳变；30+ 同屏角色规模下解算开销降低约 45%，同屏容量提升 3~5 倍，使软骨模拟方案具备产品级规模化落地能力。

---

## 9. 深度机制拆解（附录）

### 9.0 先厘清它的"作用形态"：横向闸控，不是纵向新增

自碰撞是往管线里**插一段新逻辑**（纵向新增一步）；LOD 恰相反——它**不改任何解算步骤的内部逻辑**，只决定"某一步是否执行、执行多粗"（横向闸控）。

```text
纵向新增（自碰撞）：pipeline 里多一个 AdjustByChainSelfCollision 节点
横向闸控（LOD）  ：在已有节点前加 if(LOD ...) 门，控制跳过 / 半量 / 休眠
```

这个区别决定了 LOD 的实现是"**在既有步骤外围加门**"，改动是外科式的、不侵入算法本体。

### 9.1 判定数学（ScreenSize 推导）

视觉重要性正比于物体投影到屏幕的大小，而非绝对距离：

```text
近似式（够用、便宜）：
    ScreenSize ≈ Bounds.SphereRadius / DistanceToView
    // 一次向量减法求距离 + 一次除法

精确式（需要投影矩阵时）：
    投影半径 = Bounds.SphereRadius * (0.5 * ProjMatrix.M[1][1]) / DistanceToView
    屏幕像素占比 = 投影半径 / 视口高度
```

初版用近似式即可；若要和引擎 Significance / URO 的档位精确对齐，再切精确式。

### 9.2 一帧的完整数据流（一个角色由近到远走一遍）

```text
GameThread（组件 Tick / Update 阶段）:
    dist = |CompLocation - ViewLocation|
    screen = BoundsRadius / dist
    newLOD = 分段(screen)                       // 0.08→L0  0.03→L1  0.006→L2  不可见→L3
    lod = 滞回(newLOD, prevLOD, screen)          // 死区内维持 prevLOD
    CurrentLODLevel.Store(lod)                   // 原子写

Worker（Evaluate_AnyThread 帧首）:
    const int32 LOD = CurrentLODLevel.Load();    // 读一次，之后只用局部量

  例：角色从 5m -> 12m -> 30m -> 走出屏幕
    5m  (screen 0.09) LOD0 全量：碰撞+双探针+自碰撞+迭代N 全开
    12m (screen 0.03) LOD1 中景：探针 Count×0.5、迭代=1
    30m (screen 0.005) LOD2 远景：碰撞/探针/自碰撞全停，只积分+骨长恢复（照飘）
    离屏  LOD3 休眠：Location = PoseLocation，跟随动画，开销≈0

帧尾：若发生降档->升档（如 30m 回到 12m），触发平滑器（见 9.5）
```

### 9.3 滞回状态机的精确实现

```text
int32 ResolveLOD(float screen, int32 prevLOD):
    // 阈值表（屏幕占比，越大越近）：LOD0/1/2 边界
    Enter = { L0:0.05, L1:0.01, ... }            // 升清晰档（screen 变大）用 Enter
    Exit  = { L0:0.05/1.15, L1:0.01/1.15, ... }  // 降模糊档（screen 变小）用 Exit
    候选 = 分段(screen, Enter)
    if 候选 比 prevLOD 更清晰: 需 screen > Enter[候选] 才升
    if 候选 比 prevLOD 更模糊: 需 screen < Exit[prevLOD] 才降
    else: return prevLOD                          // 落在死区 -> 维持，关键防抖
```

原理同电子学**施密特触发器**：Enter 与 Exit 之间的 1.15 倍死区吸收边界抖动。

### 9.4 使用时缩放（不可变缓存 + 派生视图）

```text
// 错误：污染真值，LOD 切回近景时原值已丢
Pair.Count = Pair.Count / 2;

// 正确：读取处算有效值，原值永不动
int32 effectiveCount = (LOD == 1)
    ? FMath::Max(1, FMath::RoundToInt(Pair.Count * LOD1ProbeScale))
    : Pair.Count;
for (i = 1; i <= effectiveCount; ++i) { ...布探针... }
```

对方向探针的 `Bone.DirectionalCollisionCount`、约束迭代 `BoneConstraintIterationCountAfterCollision` 同理——一律"读时乘系数"。这是"不可变数据 + 派生视图"模式，杜绝"改了状态回不去"。

### 9.5 平滑切换：两类跳变的分治

```text
① 速度尖峰（LOD3 休眠 -> LOD2 恢复解算）：
   休眠期 PrevLocation 停在 N 帧前，Verlet 速度 (now-old)/dt 爆炸
   -> 切出休眠帧调 WarmUp()（Simulation.cpp:780）：PrevLocation=Location、Pose 重置
   -> 速度从零干净起步（同 teleport 处理）

② 约束突现（LOD2 -> LOD1，防穿约束突然上线把穿插的链一把推开）：
   -> 切档后 5~10 帧内节点 Alpha 从 0 渐变到 1（复用 AnimNotifyState_KawaiiPhysicsSetAlpha）
   -> 物理结果与动画姿势按 α 插值，推挤摊到多帧，肉眼不可察
   Alpha 曲线建议用 SmoothStep 而非线性，起止更柔
```

任何"状态突变"视觉上都是跳变，通用平滑只有两招——**重置到干净初值（WarmUp）** 或 **把突变摊到时间上（Alpha）**——这里两招各治一类。

### 9.6 线程安全的完整论证

```text
问题：Evaluate 在 Worker 线程，碰 UObject/摄像机会崩或数据竞争
论证：
  - Bounds / 视距 / Subsystem 指针 -> 全部 GameThread 侧预取（IsInGameThread() 断言）
  - 跨线程传递唯一通道 = TAtomic<int32> CurrentLODLevel
  - Worker 帧首 Load 一次到 const 局部量 -> 帧内不再读原子（防撕裂 + 帧内一致）
  - Worker 全程不解引用任何 UObject
结论：与插件既有 CachedSharedCollisionSubsystem（AnimNode_KawaiiPhysics.h:760）
      同款"GameThread 解析、Worker 只读"模式，架构一致
```

### 9.7 边界情形清单

| 情形 | 行为 | 依据 |
|------|------|------|
| 服务器无摄像机 | 取最近玩家距离，或强制封顶 LOD1 | 无本地视图 |
| 多视角 / 分屏 | 取各视图中最清晰的一档（宁清晰不降质） | 保守安全 |
| 过场动画 / 特写 | 强制 LOD0（可加 bForceLOD0 标记） | 品质优先 |
| AnimInstance 池化复用 | LOD 状态随实例，切换 Mesh 时重置 prevLOD | 防状态串味 |
| 骨长恢复（第 9 步） | **永不降级** | 形态正确性兜底，防远景披风被拉成面条 |

### 9.8 性能预算核算（规模化数据）

```text
判定成本（GameThread）：每角色 1 减法 + 1 除法 ≈ 纳秒级，可忽略
收益（Worker 累加，估算，单模型 139µs 中碰撞+探针约占一半）：
   LOD0 全量   : 139µs
   LOD1 半量   : ≈ 100µs   (-28%)
   LOD2 停碰撞 : ≈ 55µs    (-60%)
   LOD3 休眠   : ≈ 5µs     (-96%)

30 模型（5 近 + 10 中 + 15 远）：
   无 LOD ≈ 4.2ms/帧
   有 LOD ≈ 5×139 + 10×100 + 15×55 = 0.70 + 1.0 + 0.83 ≈ 2.4ms/帧  (-45%)
50 模型大场面：省 50%+，LOD3 越多越省

核心价值不只省 CPU，而是帧率"保险丝"：保护 Worker 池余量，
大场面 / 低配机不因披风解算饱和 Worker 而掉帧。
```
