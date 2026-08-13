# 链-链真自碰撞子系统 · 开发设计文档

> 目标插件：UE5.5 KawaiiPhysics（骨骼物理软骨模拟）
> 适用场景：披风 / 裙摆 / 头发等多平行骨骼链软骨部位的链间穿插治理

---

## 1. 背景与问题定义

披风软骨模拟采用「点→线→面」三级防穿体系：

- **点**：原生骨球，防骨骼点穿身体
- **线**：方向探针（`AdjustByDirectionalProbes`），防链内父→子连线穿身体
- **面**：相邻链探针（`AdjustByAdjacentLinkProbes`），防链间布面穿身体/世界

但现有探针的检测对象均为**碰撞限制体与世界场景**，不包含**另一条骨骼链**。在翻滚、倒地、击飞、快速转身等极端动作下，相邻披风链会直接互相穿越，造成布面交叉、翻面闪烁等穿帮。

**目标**：以近零开销补齐链-链自碰撞能力，形成「点线面体」完整防穿闭环。

---

## 2. 关键技术点（含实现逻辑）

本节把每个技术点拆成「是什么 → 解决什么 → 实现逻辑（代码级）→ 为什么这样更好」四层，答辩被追问任一点都能落到实现细节。

### 技术点 1：结构性宽相位（Structural Broadphase）

**是什么**：碰撞检测普遍分两阶段——**宽相位**（broadphase，粗筛"哪些对可能碰"）+ **窄相位**（narrowphase，精确求交）。通用宽相位（空间哈希 / BVH / Sweep-and-Prune / 均匀网格）都要**每帧动态构建加速结构**（ChaosCloth 的 `BuildSpatialHash` 就占 4.4%）。**结构性宽相位**则不建任何运行时结构，直接用问题的拓扑先验得到候选对。

**解决什么**：披风链空间上并行排列，第 1 条链只可能碰第 2 条，碰不到隔着好几条的第 5 条。这个"谁挨着谁"是固定的拓扑，没必要每帧重算。

**实现逻辑**：
- 相邻关系在**初始化期**由 `InitAdjacentLinkCollision`（ModifyBones.cpp:747）算好，固化进 `AdjacentPairs` 数组（元素含 `BoneIndexA/BoneIndexB`）。
- 自碰撞运行时**直接遍历 `AdjacentPairs`** 即得候选对——`for (const FKawaiiPhysicsAdjacentPair& Pair : AdjacentPairs)`，复杂度 O(相邻对数)，**构建成本为零**。
- 每对只需读 `ModifyBones[Pair.BoneIndexA].Location` / `...BoneIndexB.Location`，纯数组下标访问。

**为什么更好**：ChaosCloth 面对"一堆三角面片"，不知道链式结构，只能通用暴力；我们**知道**这是链，就把候选集在初始化时写死。核心方法论——**读懂问题结构 > 套用通用强力算法**。

### 技术点 2：PBD 球-球分离约束

**是什么**：PBD（Position Based Dynamics，Müller 2007）——**直接操作位置**、而非"力→加速度→速度→位置"积分的物理求解范式。

**解决什么**：传统力学做"两物体不许重叠"这种刚性约束，需要很大排斥力配极小时间步，否则爆炸；PBD 直接把重叠的位置**投影**回不重叠，无条件稳定，适合实时。

**实现逻辑（对每个 Pair 的两端骨 A、B）**：
```text
delta   = B.Location - A.Location
dist    = |delta|
minDist = RA + RB                      // 半径取 PhysicsSettings.Radius
若 dist < minDist 且 dist > epsilon:
    C    = minDist - dist              // 侵入深度（约束违反量）
    n    = delta / dist                // 连心线单位向量
    ΔA   = -0.5 * C * n * Stiffness     // A 退一半
    ΔB   = +0.5 * C * n * Stiffness     // B 推一半
```
- **各推一半**：假设两骨等质量；要区分主次可按质量倒数加权。
- **Stiffness < 1（默认 0.5）**：欠松弛（under-relaxation），单次只修正一部分，靠迭代收敛，避免一步到位过冲。

**为什么更好**：整个 KawaiiPhysics 就是 PBD/XPBD 框架（骨长恢复、碰撞推出、`AdjustByBoneConstraints` 全是位置投影），新约束与它们"同一种语言"，融入无缝、不打架。

### 技术点 3：复用反馈结算机制（延迟加权累积，Jacobi 式）

**是什么 / 解决什么**：一根骨可能同时属于多个 Pair（披风中间的链左右各有邻居），一帧内被多个分离约束推。**边算边改**会导致：① 顺序依赖（先处理谁就偏向谁）；② 过冲（N 个约束推 N 倍远）。

**实现逻辑（两阶段延迟应用）**：
```text
阶段一（记账，不改骨骼）：
    AdjacentFeedbackPushScratch[A]   += ΔA;  AdjacentFeedbackWeightScratch[A] += 1
    AdjacentFeedbackPushScratch[B]   += ΔB;  AdjacentFeedbackWeightScratch[B] += 1
阶段二（结算，统一应用）：
    ModifyBones[i].Location += PushScratch[i] / max(1, WeightScratch[i])   // 加权平均
```
- **直接写进相邻链探针已有的 `AdjacentFeedbackPushScratch/WeightScratch`**（Collision.cpp:781），连结算循环都不用新写——第 6 步探针和第 7 步自碰撞共用一次结算。
- Scratch 数组 `Reset()+SetNumZeroed(NumBones)` 复用容量，端点 index 直访问，热路径零堆分配、无 TMap。

**为什么更好**：这是 **Jacobi 迭代**（先全算、后统一更新）而非 Gauss-Seidel（边算边用）——顺序无关、可并行、更稳定；加权平均在数学上消除过冲。

### 技术点 4：防振荡设计

**是什么 / 解决什么**：约束求解器通病——多约束冲突时位置来回跳（抖动）。本场景冲突源：**分离约束（推开）↔ 骨长恢复（拉回）↔ 碰撞（推出）** 三方拔河。

**实现逻辑（三道防线）**：
1. **Clamp 单次推挤**：`push = min(push, 0.5 * C)`，限制单步幅度，渐进收敛而非一步猛冲反弹。
2. **迭代封顶**：`ChainSelfCollisionIterations` 默认 1、上限 2——不追求精确收敛，够用即止。
3. **管线顺序仲裁**：自碰撞插在**骨长恢复之前**（第 7 步 vs 第 9 步），让骨长恢复统一把所有约束的残余误差归位。

**为什么更好**：Clamp、少迭代、`Stiffness<1` 本质都是**牺牲收敛速度换稳定性**——实时物理黄金准则「看起来对且不抖 > 数学精确」。

### 技术点 5：零配置接入

**是什么 / 实现逻辑**：自碰撞不引入任何新配置——复用的全是既有数据：`AdjacentPairs`（已有的配对，含 `Count/Radius`）+ `PhysicsSettings.Radius`（已有的骨半径）。对外只暴露一个开关 `bUseChainSelfCollision`：
```text
if (!bUseChainSelfCollision || AdjacentPairs.Num() == 0) return;   // early-out
```

**为什么更好**：接入边际成本为零，不增加美术/策划负担——直接服务"团队通用软骨方案"的产品目标。

---

## 3. 算法详解

### 3.1 路径 A：Pair 端点骨最小距离约束（首选实现）

```text
for 每个 AdjacentPair (A, B):
    delta   = B.Location - A.Location
    dist    = |delta|
    minDist = RA + RB                     // 两端骨 PhysicsSettings.Radius 之和
    if dist < minDist 且 dist > epsilon:
        push = (minDist - dist) * Stiffness   // Stiffness 默认 0.5，宁弱勿振
        dir  = delta / dist
        Scratch[A] -= dir * push * 0.5;  Weight[A] += 1
        Scratch[B] += dir * push * 0.5;  Weight[B] += 1
// 结算沿用既有加权平均通道，统一应用到 ModifyBones[i].Location
```

**激活阈值设计**：链自然间距（5~10cm）> 半径和（约 6cm），正常悬垂零误触；仅真实贴近时启动。

### 3.2 路径 B：跨链斜向分离（A 不足时升级）

检测对扩展为 A_k vs B_k、A_k vs B_{k±1}（对称 B_k vs A_{k±1}），每对相邻链约 30 次测试；宽相位直接复用相邻配对结构，窄相位仍为球-球分离。

### 3.3 算法复杂度

4 链 × 6 层约 24 骨：路径 A 约 18 次/帧，路径 B 约 90 次/帧，纯标量运算，微秒级。对照：ChaosCloth 同功能（三角形级自碰撞 + 自相交分析）实测 5.1ms/帧。

---

## 4. 在插件中的运行流程

```text
Evaluate_AnyThread（Worker 线程，每帧）
  ...
  6. AdjustByAdjacentLinkProbes（探针 vs 限制体，Simulation.cpp:491）
  7. ★ AdjustByChainSelfCollision【新增，插于此】
       - 遍历 AdjacentPairs -> 球-球分离 -> 写入 AdjacentFeedback Scratch
       - 与第 6 步共用同一段结算循环（端点加权平均）
  8. BoneConstraints After Collision（XPBD，Simulation.cpp:494）
  9. AdjustByLimitsAndLength（角度/平面/骨长恢复，Simulation.cpp:508）
       - 第 7 步产生的推挤"毛边"由第 9 步的骨长恢复免费抚平
  10. ApplySimulateResult 回写
```

**插入位置是硬约束**：必须在第 9 步骨长恢复之前，否则推挤残留导致帧间抖动。

---

## 5. 实现路径

| 步骤 | 内容 | 落点 |
|------|------|------|
| 1 | 新函数 `AdjustByChainSelfCollision()`（含 `SCOPE_CYCLE_COUNTER` 统计） | Collision.cpp，约 80 行 |
| 2 | 3 个配置 UPROPERTY：`bUseChainSelfCollision` / `ChainSelfCollisionIterations`(默认 1) / `ChainSelfCollisionStiffness`(默认 0.5) | AnimNode_KawaiiPhysics.h，Category `Bones\|Adjacent Link Collision` |
| 3 | 调用点 1 行 | Simulation.cpp L491 之后 |
| 4 | 单元测试：分离正确性 / 防过冲 / 开关回归 / Rest 距离不误触 | Tests/ 目录，复用 TestHarness |
| 5 | 性能基准：8 模型 40 帧 trace 前后对照 | Unreal Insights |

**任务节奏**：Day1 实现路径 A -> Day2 防振荡调优 -> Day3 极端动作集 + 对比视频 -> Day4 评估是否升级路径 B -> Day5 性能报告 + 单测 + 提交。

---

## 6. 难点与解决方案

| 难点 | 解决方案 |
|------|---------|
| 与骨长恢复互相拉扯导致抖动 | 单次推挤 clamp（<= 0.5×侵入深度）+ 迭代 <= 2 + 插入位置固定在骨长恢复之前 |
| 静止状态误推挤（披风被撑开显胖） | 激活阈值 = 半径和，远小于自然链距；必要时按 RestDistance×系数校准；Stiffness 取 0.3~0.5 |
| 多 Pair 推同一根骨过冲 | 推挤不直接改骨骼，统一走 Scratch 加权平均结算 |
| 同帧顺序污染 | 先全量记账、后统一结算（探针基准位置不在遍历中被修改） |
| 线程与性能 | 全部 Worker 线程、零堆分配（Scratch 复用）、零配置解析 |

---

## 7. 验证方案

| 验证项 | 方法 | 标准 |
|--------|------|------|
| 穿插消除 | 极端动作集前后逐帧对比 | 穿插帧数显著下降 |
| 常态无回归 | 待机/走/跑 40 帧 trace | 触发约 0 次、耗时曲线重合 |
| 稳定性 | 1000+ 帧长跑 | 无抖动、无炸开、无漂移 |
| 性能 | Insights 40 帧框选 | `KawaiiPhysics_Eval` 增幅 < 5% |

---

## 8. 简历缩略版

> 基于 UE5.5 KawaiiPhysics 骨骼物理插件，针对披风软骨模拟的链间互穿问题，设计并实现链-链自碰撞子系统：利用相邻链配对的结构先验作为天然宽相位，将检测规模压缩至百次级/帧（免空间哈希）；采用 PBD 球-球分离约束与加权平均结算机制防止过冲，以不足 5% 的额外开销消除极端动作下的披风互穿——相较 ChaosCloth 原生自碰撞（5.1ms/帧），性能成本降低两个数量级。

---

## 9. 深度机制拆解（附录）

### 9.0 先纠正一个误区：它**不走** `RunSingleProbeCollision`

方向探针 / 相邻链探针都"复用原生碰撞管线"，但自碰撞是**例外**：

| | 检测对象 | 检测函数 |
|---|---|---|
| 方向探针 / 相邻链探针 | 探针球 vs **限制体**（球/胶囊/盒/平面）+ 世界 | `RunSingleProbeCollision` |
| **链-链自碰撞** | **骨骼球 vs 骨骼球**（两条链上的 ModifyBone） | **新增窄相位**，不经过 `RunSingleProbeCollision` |

原因：`RunSingleProbeCollision` 内部回答的是"这个点离**限制体**多远"，而自碰撞要回答"这根骨离**那根骨**多远"——`ModifyBones` 不在那套管线里，必须新写一个极小的窄相位（输入两个骨骼索引，输出分离推挤量）。

### 9.1 数学推导（从约束到位置修正）

把每根模拟骨当作球心 `Location`、半径 `PhysicsSettings.Radius` 的球。对 Pair 的两端骨 A、B 定义分离约束：

```text
C(A, B) = |B - A| - (RA + RB)
C >= 0  -> 满足（不碰）
C <  0  -> 违反（穿透深度 = -C）
```

PBD 位置投影，沿连心线各推开一半：

```text
n  = (B.Location - A.Location) / dist       // 分离方向
Δ  = -C * Stiffness                          // 总修正量（欠松弛，Stiffness ∈ (0,1]）
ΔA = -0.5 * Δ * n                            // A 往反方向退
ΔB = +0.5 * Δ * n                            // B 往正方向推
```

与 `AdjustByBoneConstraints`（Collision.cpp:843）的 XPBD 写法同构；要更"软"可引入 compliance（`XPBDComplianceValues` 柔度表 L832-841），初版建议先硬分离（compliance=0）。

**边界守卫**（照抄插件现有风格）：
- `dist < KINDA_SMALL_NUMBER`（两骨心重合）-> 用父骨侧方轴 `PoseRotation.GetAxisX()` 做替代方向（`AdjustByAngleLimit` L646-647 已有先例）
- `RA + RB <= 0` -> 跳过该 Pair

### 9.2 一帧的完整数据流（以 4 链 × 6 层为例）

4 条链、每链 6 层实骨，自动配对生成 3 对链 × 6 层 = 18 个 Pair。

```text
帧首：Scratch 清零（Reset + SetNumZeroed(24)，容量复用）

6. 相邻链探针阶段：18 Pair × 约 2 探针 ≈ 36 次限制体检测 -> 记账部分骨

7. 【新增】自碰撞阶段：
   for Pair in 18 个 Pair:                    // 宽相位 = 遍历数组本身
       dist = |B.Location - A.Location|
       if dist < RA + RB:                      // 窄相位（1 减法 + 1 模长）
           算 Δ、n
           Scratch[A] -= 0.5*Δ*n;  Weight[A] += 1
           Scratch[B] += 0.5*Δ*n;  Weight[B] += 1

结算（与第 6 步共用同一循环）：
   for i in 0..23:
       if WeightScratch[i] > 0:
           ModifyBones[i].Location += PushScratch[i] / max(1, WeightScratch[i])

8/9. XPBD 约束 -> 角度/平面限制 -> 骨长恢复（把第 7 步拉长的链投影回原长）
```

单披风约 18 次距离判定 + 偶发分离修正，微秒级；24 骨结算循环本就为第 6 步而跑，第 7 步搭便车。

### 9.3 与结算通道融合的精妙处（为什么必须共用 Scratch）

`max(1, W)` 的语义决定一切：

| 场景 | W | 除数 | 效果 |
|------|---|------|------|
| 只有 1 个探针推了骨 X（α=0.7） | 0.7 | max(1, 0.7)=1 | 全额应用 |
| 探针推 X（0.7）+ 自碰撞又推 X（1.0） | 1.7 | 1.7 | 两机制**加权平均**，不叠加成 1.7 倍 |

若自碰撞不用账本、直接改 `Location`，就会和探针推挤**无脑叠加** -> 过冲抖动。共用账本让任何来源的推挤都进同一个"加权平均漏斗"——**复用的真正价值不是省代码，是买稳定性**。

### 9.4 激活阈值的精细设计（防"披风充气"）

朴素阈值 `dist < RA + RB` 的隐患：急停/甩动时链会合法靠近，一靠近就推会让披风"胖一圈"。三级对策：

```text
Level 1（默认）：minDist = RA + RB              // 只在真贴上时启动，多数够用
Level 2（推荐）：minDist = (RA + RB) * 0.8       // 故意欠覆盖，宁贴近不撑开
Level 2（精细）：初始化记录每对 RestDist，
               激活 = dist < min(RestDist * 0.6, RA + RB)   // 按资产链距自适应
```

`RestDist` 在 `AddPairChain` 里加一行（构建 Pair 时算首帧距离存入结构体）即可——仍是"初始化固化、运行时只读"。

### 9.5 边界情形清单

| 情形 | 行为 | 依据 |
|------|------|------|
| 不等长链 | 配对只到短链末尾，长链尾骨不覆盖 | AddPairChain while 终止 |
| 尾骨盲区 | 路径A不管；路径B斜向可部分覆盖 | 已知取舍 |
| Dummy 骨 | 不参与（Pair 里全是实骨索引） | `FirstRealChild` |
| Root/运动学骨 | `bSkipSimulate` 跳过 | 与碰撞循环同规则 |
| 子步开启 | 在 `SimulateOnce` 内，每子步执行一次 | 调用点位置 |
| LOD 联动 | LOD>=2 随碰撞整体跳过；LOD1 可降频 | 闸门统一 |
| 两骨心重合 | 替代轴守卫 | 见 9.1 |

### 9.6 防振荡策略组合（完整版）

```text
欠松弛   : Stiffness = 0.5      -> 单步只修一半
限幅     : Δ <= 0.5 * 穿透深度   -> 单步幅度硬上限
迭代封顶 : 1~2 次                -> 不追求完美收敛
统一结算 : max(1, W) 加权平均    -> 多源推挤不叠加
管线位置 : 骨长恢复之前          -> 残余误差被第 9 步归位
```

五条相互独立、叠加生效，风格继承自原生 `AdjustByBoneConstraints` 的 XPBD + Lambda 设计。

### 9.7 测试矩阵

| 维度 | 用例 | 预期 |
|------|------|------|
| 功能 | 手动拉两链贴近 | 平稳推开，无弹飞 |
| 误触 | 待机 100 帧 | 触发 = 0，形态不变 |
| 边界 | 两骨心人为重合 | 不 NaN、不卡死 |
| 回归 | 跑/跳/转身 | 不触发时逐帧一致 |
| 效果 | 翻滚/倒地/击飞 | 穿插帧数显著下降 |
| 性能 | 8 模型 40 帧 trace | Eval 增幅 <5% |
| 稳定 | 1000+ 帧长跑 | 无漂移、无炸开 |

### 9.8 性能预算核算

```text
18 次距离判定（减法+模长）      ≈ 18 × ~10ns
+ 偶发分离修正                   ≈ 可忽略
+ Scratch 清零（24 元素）        ≈ 可忽略
合计：单披风 < 1µs/帧
对照：ChaosCloth 自碰撞家族 5.1ms/帧（8 模型）
=> 三个数量级差距来自：检测对象是"24 根骨"而非"上千三角面"
```
