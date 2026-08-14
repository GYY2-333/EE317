# ChaosCloth 披风解算性能分析报告

## 一、基本信息

| 项目 | 内容 |
|---|---|
| Trace 文件 | `C:\Users\guoyueyuan\AppData\Local\UnrealEngine\Common\UnrealTrace\Store\001\20260723_160235.utrace`（76.48 MB） |
| 分析工具 | Unreal Insights（UE 5.5 源码版，`D:\UESourceCode\UnrealEngine\Engine\Binaries\Win64\UnrealInsights.exe`） |
| 分析对象 | ChaosCloth 插件对披风（Cloak/Cape）的运行时解算消耗 |
| 场景 | 8 个相同的带披风模型（网格资产 `ChaosNewCape`，见 6.7），循环播放同一动作 |
| **分析选区** | **Frame 1624 – 1663，共 40 帧** |
| 实例换算 | 每类计时器 Count = 320 = 8 模型 × 40 帧（每模型每帧解算一次） |

> 说明：每帧 8 模型合计 = Incl ÷ 40；单模型每帧 = Incl ÷ 320。

---

## 二、总体消耗（40 帧选区）

层级关系（源码确认）：
`FClothingSimulation_Simulate`（组件入口）→ `FClothingSimulationSolver_Update`（解算器）→ `FClothingSimulationSolver_UpdateSolverStep`（约束求解）→ 各类约束子作用域。

| 指标 | 数值 |
|---|---|
| 披风每帧总消耗（8 模型合计） | **312 ms ÷ 40 = 7.8 ms/帧** |
| 单模型每帧消耗 | 312 ms ÷ 320 = **≈0.98 ms** |
| `UpdateSolverStep` 占解算器比例 | 286.4 / 311.3 = **91.8%**（7.16 ms/帧） |

蒙皮、碰撞更新、法线、包围盒等合计 <10%，非瓶颈。**问题集中在 `UpdateSolverStep` 的 286 ms 里。**

> 注意：7.8 ms/帧为 **Worker 线程跨线程累加的 CPU 时间**，不是帧墙钟，不在游戏线程关键路径上（见第六节）。

---

## 三、ChaosCloth 主要计时器消耗（`ClothingSimulation` 过滤，40 帧）

Group by: Timer Type ｜ Mode: Instance

| 计时器（Timing 面板作用域名） | Count | Incl | Excl | 每帧(8模型) | 说明 |
|---|---|---|---|---|---|
| `FClothingSimulation_Simulate` | 320 | 312 ms | 727.3 µs | 7.80 ms | 披风组件每帧解算总入口 |
| `FClothingSimulationSolver_Update` | 320 | 311.3 ms | 113.4 µs | 7.78 ms | 解算器总调度 |
| **`FClothingSimulationSolver_UpdateSolverStep`** | 320 | **286.4 ms** | 1.1 ms | **7.16 ms** | **PBD/XPBD 约束求解（热点）** |
| `FClothingSimulationSolver_UpdateCloths` | 320 | 13 ms | 592.4 µs | 0.33 ms | 属性/风/网格→粒子 |
| `FClothingSimulationSolver_UpdatePostSolverStep` | 320 | 6.7 ms | 204.4 µs | 0.17 ms | 重算法线 |
| `FClothingSimulationSolver_UpdatePreSolverStep` | 320 | 5 ms | 210.1 µs | 0.13 ms | 预处理 |
| `FClothingSimulationSolver_ApplyPreSimulationTransforms` | 320 | 4.8 ms | 105.5 µs | 0.12 ms | 预仿真变换 |
| `FClothingSimulationMesh_SkinPhysicsMesh` | 320 | 4.2 ms | 4.2 ms | 0.11 ms | 物理网格蒙皮 |
| `FClothingSimulationSolver_ParticlePreSimulationTransforms` | 320 | 3.7 ms | 3.7 ms | 0.09 ms | 粒子预仿真变换 |
| `FClothingSimulation_GetSimulationData` | 320 | 1.7 ms | 1.7 ms | 0.04 ms | 结果回写渲染网格 |
| `FClothingSimulationSolver_PreSubstep` | 320 | 1.4 ms | 55 µs | 0.04 ms | 子步准备 |
| `FClothingSimulationSolver_ParticlePreSubstepKinematicInterpolation` | 320 | 1.1 ms | 1.1 ms | 0.03 ms | 运动学插值 |
| `FClothingSimulationCollider_Update` | 320 | 1 ms | 1 ms | 0.03 ms | 碰撞体更新 |
| `FClothingSimulationSolver_CollisionPreSimulationTransforms` | 320 | 632.7 µs | 632.7 µs | 0.02 ms | 碰撞预仿真变换 |
| `FClothingSimulationSolver_CalculateBounds` | 320 | 573.5 µs | 573.5 µs | 0.01 ms | 包围盒 |
| `FClothingSimulationCollider_PreUpdate` | 320 | 344.8 µs | 344.8 µs | ~0 | 碰撞体预更新 |
| `FClothingSimulationSolver_UpdateSolverFields` | 320 | 69.7 µs | 69.7 µs | ~0 | 力场（风等） |

---

## 四、`UpdateSolverStep`（286.4 ms）100% 拆解 —— 消耗根源

`UpdateSolverStep` 自身 Excl 仅 1.1 ms，285 ms 全在子作用域。通过 `Collision` / `Constraint` / `Bending` / `LongRange` 过滤逐条补齐：

核心是 **`ChaosXPBDConstraintsInit` = 208.3 ms（占解算 73%）**，Excl 仅 2.3 ms —— 即约束初始化阶段的**自碰撞检测**几乎吃掉了整个解算。

| 子作用域 | Incl(40帧) | 每帧(8模型) | 占解算 | 归属 / 对应参数 |
|---|---|---|---|---|
| **`ChaosFPBDTriangleMeshCollisions_IntersectionQuery`** | **109.6 ms** | 2.74 ms | **38.3%** | **自相交分析 `bUseSelfIntersections`** |
| **`ChaosPBDCollisionSpring_ProximityQuery`** | **77 ms** | 1.93 ms | **26.9%** | **自碰撞 `bUseSelfCollisions`** |
| `ChaosPBDCollisionRule` | 24.8 ms | 0.62 ms | 8.7% | 碰撞约束求解（按迭代） |
| `ChaosPBDCollisionRuleP` | 24.4 ms | 0.61 ms | 8.5% | 碰撞约束求解 P（按迭代） |
| `ChaosPBDConstraintRule` | 31.3 ms | 0.78 ms | 10.9% | 约束求解分发 |
| `ChaosFPBDTriangleMeshCollisions_BuildSpatialHash` | 12.5 ms | 0.31 ms | 4.4% | 自碰撞空间哈希构建 |
| `FPBDBendingConstraints_Apply` | 12.9 ms | 0.32 ms | 4.5% | 弯曲约束（拉伸/弯曲刚度） |
| `FPBDAxialSpringConstraints_Apply` | 4.1 ms | 0.10 ms | 1.4% | 斜向弹簧约束 |
| `FPBDSpringConstraints_Apply` | 3.2 ms | 0.08 ms | 1.1% | 边弹簧约束 |
| `ChaosFPBDTriangleMeshCollisions_BuildGlobalContour` | 1.8 ms | 0.05 ms | 0.6% | 自相交分析（轮廓） |
| `ChaosFPBDTriangleMeshCollisions_BuildIntersection` | 1.7 ms | 0.04 ms | 0.6% | 自相交分析 |
| `FPBDBendingConstraintsBase_Init` | 1.7 ms | 0.04 ms | 0.6% | 弯曲约束初始化 |
| `ChaosFPBDTriangleMeshCollisions_FloodFillContours` | 957 µs | 0.02 ms | 0.3% | 自相交分析（轮廓填充） |
| `ChaosPBDPostCollisionConstraintRule` | 718.4 µs | 0.02 ms | 0.3% | 碰撞后处理 |
| `FPBDLongRangeConstraints_Apply` | 570.1 µs | 0.01 ms | 0.2% | 长距离绑定（Tether） |
| `ChaosPBDConstraintPostprocessings` | 225.8 µs | ~0 | ~0 | 约束后处理 |

> 存在层级嵌套，占比之和非精确 100%，但量级明确。

### 关键结论

- **自碰撞家族合计 ≈ 203 ms / 40帧 = 5.1 ms/帧 = 整个披风开销(7.8 ms/帧)的 65~70%。**
- **自相交分析 `IntersectionQuery`（109 ms，38%）比自碰撞本体 `ProximityQuery`（77 ms，27%）更贵**，是首要优化目标。
- **真正的约束求解（边/弯曲/斜向弹簧）极便宜**，合计 <0.5 ms/帧 —— **降迭代数、调刚度对性能几乎无收益**。

---

## 五、从 Trace 反推的解算参数（当前值）

| 参数 | 推断值 | 依据 |
|---|---|---|
| `NumSubsteps` | **1** | Init 类作用域 Count(320) = 解算步 Count，无翻倍 |
| 有效迭代 `NumUsedIterations` | **≈2** | Apply 类作用域 Count(640) = 2 × 解算步 Count(320) |
| **`bUseSelfCollisions`** | **开启**（引擎默认 false） | 出现 `ProximityQuery` + `BuildSpatialHash` |
| **`bUseSelfIntersections`** | **开启**（引擎默认 false，最贵） | 出现 `IntersectionQuery` + `FloodFillContours` + `BuildGlobalContour` |
| `SelfCollisionThickness` | 默认 2.0（影响邻近对数量） | — |
| 外部碰撞 `CollisionThickness` / `bUseCCD` | 开启（有 `ChaosPBDCollisionRule`） | 8 个角色碰撞体 |

对应配置源码位置：`Engine/Plugins/ChaosCloth/Source/ChaosCloth/Public/ChaosCloth/ChaosClothConfig.h`
- `CollisionThickness`（L225）、`bUseCCD`（L236）、`bUseSelfCollisions`（L240）、`SelfCollisionThickness`（L244）、`SelfCollisionFriction`（L248）、`bUseSelfIntersections`（L252）

> `bUseSelfCollisions` 与 `bUseSelfIntersections` 引擎默认均为 `false`；本披风资产将两者开启，是消耗根源。

---

## 六、线程模型与 GameThread 关系（代码级分析结论）

> 本节为后续源码走查 + Timing 实测验证的结论。

### 6.1 解算（含自碰撞/自相交）全部在 Worker 线程

任务链：`FParallelClothTask`（Worker）→ `FClothingSimulation_Simulate` → `Update` → `UpdateSolverStep` → `ChaosXPBDConstraintsInit` → `IntersectionQuery` / `ProximityQuery`。

- 派发点：`USkeletalMeshComponent::UpdateClothStateAndSimulate`（`SkeletalMeshComponentPhysics.cpp:3831`）；
- 任务体：`FParallelClothTask::DoTask`（L3688），内层 `SCOPE_CYCLE_COUNTER(STAT_ClothTotalTime)` + `FScopeCycleCounterUObject(网格资产)`（L3690）；
- **退化例外**：`CVarEnableClothPhysicsUseTaskThread = 0` 时 `FParallelClothTask::GetDesiredThread()` 返回 GameThread（L3675-3682），解算会直接跑在游戏线程。本 trace 实测 `FClothingSimulation_Simulate` 全在 Worker，未踩此退化。

### 6.2 自碰撞的内部并行结构（读数陷阱）

- `ChaosFPBDTriangleMeshCollisions_IntersectionQuery` 作用域打开后紧跟 `PhysicsParallelFor(NumIntersectableEdges, …)`（`PBDTriangleMeshCollisions.cpp:29`→`47`）；
- `ChaosPBDCollisionSpring_ProximityQuery` 同理（`PBDCollisionSpringConstraintsBase.cpp:176`→`195`）。

即：**查询作用域块画在"发起 cloth 任务的那条 Worker 线程"上（容器），真正的逐边/逐粒子计算被 `PhysicsParallelFor` 撒到多条 Worker 线程并行执行**。

> **陷阱**：`IntersectionQuery` 的 Incl（109.6 ms）是这些容器块的 inclusive 墙钟**跨线程累加**；不是 `PhysicsParallelFor` 子块时长相加，会重复计数。

### 6.3 GameThread 的五个角色（收发，不解算）

`USkeletalMeshComponent::TickClothing` → `UpdateClothStateAndSimulate`（`SkeletalMeshComponentPhysics.cpp:3798`，GameThread）每帧做：

1. **决策门槛**（`TickClothing` L3871）：可见性策略（`VisibilityBasedAnimTickOption`/`bRecentlyRendered`）、`DeltaTime==0` 跳过等，决定是否派发；
2. **签收上一帧结果**（L3817 `HandleExistingParallelClothSimulation`，作用域 `EndParallelClothTask` L2386）：等上帧任务（实测≈0）→ `CompleteParallelClothSimulation`（L2315）→ `WritebackClothingSimulationData`（L2396）把模拟顶点拷回 `CurrentSimulationData`（放在 GameThread 回写是为了数据有效期/线程安全，L3701 注释）；
3. **打包本帧输入**（L3829 `UpdateClothSimulationContext`）：骨骼姿势、碰撞体变换、风力等填入 `ClothingSimulationContext`；
4. **派发本帧任务**（L3831）：`FParallelClothTask` 扔到 Worker，fire-and-forget，自己继续；
5. **EOF 同步把关 + 转交渲染**（默认路径 `OnPreEndOfFrameSync` L3754）：在 `UWorld_SendAllEndOfFrameUpdates`（`LevelTick.cpp:980`）前确认 cloth 完成，随后布料数据随 EndOfFrameUpdates 交渲染线程。

### 6.4 一帧延迟异步模型

帧 N 派发的 cloth 任务，**帧 N+1 的 tick 才签收**——cloth 有约一整帧的异步窗口。只有 `ShouldWaitForClothInTickFunction()`（L3762：`bWaitForParallelClothTask` 或 `CVarClothPhysicsTickWaitForParallelClothTask`，默认 false）为 true 时，才会在 tick 内通过 `FParallelClothCompletionTask`（L3704，`STAT_ClothWriteback`）等本帧 cloth。

### 6.5 依赖传导模型（cloth 何时才会影响 GameThread）

```
影响 = max(0, cloth完成时刻 − GameThread到达依赖点时刻)
```

三个传导点：
1. **签收点**（`HandleExistingParallelClothSimulation`）——上帧任务没完才阻塞；
2. **EOF 同步点**（`OnPreEndOfFrameSync`）——cloth 没完则 EndOfFrameUpdates 推迟；
3. **Worker 池挤占（间接）**——cloth 内部 `PhysicsParallelFor` 占满核心 → GameThread 派发的并行动画求值完成变晚 → 而打包输入依赖动画姿势 → 全链路被推迟。

**窗口内零影响；超出窗口才等量传导。** cloth 的"分发动作本身的执行速度"恒定，受影响的只是其发生时刻。

### 6.6 `WaitUntilTasksComplete` 的正确解读（`TaskGraph.cpp:1464`）

GameThread 上的**通用汇合点**（TRACE 作用域 L1467，cpu 通道即可见）。**名字不说明在等谁**，必须靠父级作用域 + 时间对齐判断。进入后三层行为（TASKGRAPH_NEW_FRONTEND）：

1. **收回自己执行**（`TryRetractAndExecute` L1476）：把要等的任务抢回 GameThread 跑掉——此时目标任务会嵌在 Wait 块内部；
2. **边等边干别的活**（L1524-1527 `ProcessThreadUntilRequestReturn`）：处理本线程队列里其它 pending 任务；
3. **真空转**（内层 `WaitForTasks` L753）：队列空了才休眠——**只有这部分是纯浪费**。

常见父级：`TickCompletionEvents`（`TickTaskManager.cpp:813`，等**整个 tick 组**的完成事件，非 cloth 专属）、`USkeletalMeshComponent::BlockOnParallelEvaluationTask`（等并行动画求值，`SkeletalMeshComponent.cpp:4353`）。

> 推论：**`WaitForTasks` / `GameThreadWaitForTask` 数值大 ≠ cloth 造成**。它们是每帧都有的通用同步点累计，归因必须逐块对齐。

### 6.7 `ChaosNewCape` 作用域正名

不是工程自定义代码作用域。它是**骨骼网格资产名**（`D:\Cloth-Repos\guoyueyuan\Cloth\Content\ChaosNewCape\ChaosNewCape.uasset`，3.6MB，配套 `_PhysicsAsset`/`_Skeleton`/`_Skeleton_AnimBlueprint`），引擎用 `FScopeCycleCounterUObject` 把资产/对象名打在三个任务上：

| 任务 | 线程 | 标签来源 | 内含 |
|---|---|---|---|
| `FParallelAnimationEvaluationTask` | Worker | `SkeletalMeshComponent.cpp:216`（组件名） | 动画蓝图求值 |
| **`FParallelAnimationCompletionTask`** | **GameThread** | **L277-278（组件名 + 网格资产名）** | → `CompleteParallelAnimationEvaluation`（L4329 黄色 named event）→ FinalizeBoneTransform/蒙皮派发 |
| `FParallelClothTask` | Worker | `Physics.cpp:3690`（网格资产名） | → `FClothingSimulation_Simulate`（解算本体） |

- **Incl 387 ms** = 同名标签下两类任务聚合：Worker 的 cloth 任务（≈312 ms）+ GameThread 的动画收尾任务；
- **Excl 44.3 ms** 主要是**动画收尾任务自身的固定开销**（每帧 8 个 × 50~100 µs，FinalizeBoneTransform 等）；
- GameThread 上的 ChaosNewCape 块是**动画管线固定成本，与是否模拟 cloth 无关**（关掉 cloth 它照跑、时长基本不变），不应记在 cloth 头上；
- 区分方法：看线程 + 看内嵌——Worker 大块内嵌 `FClothingSimulation_*` 是解算；GameThread 小块内嵌 `CompleteParallelAnimationEvaluation` 是动画收尾。

### 6.8 实测验证结论：cloth 未堵塞 GameThread

Timing 面板三条实测证据：

1. `FClothingSimulation_Simulate` **全部在 Worker 线程** → 排除 GameThread 收回执行 cloth 的可能；
2. GameThread 的 `WaitForTasks` **比 Worker 上的 cloth 块短** → cloth 在异步窗口内完成，汇合点零等待；
3. `TickCompletionEvents (1.2 ms)` → `WaitUntilTasksComplete` **内部嵌满被执行的 tick 任务**（PlayerController / FActorComponentTickFunction / ChaosNewCape 动画收尾 / `CompleteParallelAnimationEvaluation`）→ 等待期间在干活，非空转。

→ **cloth 对 GameThread 的直接影响 ≈ 0**；7.8 ms/帧是 Worker 池的 CPU 预算账，不是帧时间账。

### 6.9 对最终帧率的影响

帧率公式：`FPS = 1000 / 帧时间`；**帧时间 ≈ max(GameThread, RenderThread, GPU) + 同步损耗**（流水线最慢一级决定，非求和；另有 vsync / `t.MaxFPS` / 帧平滑等外部钳制）。

- Worker **不直接出现在公式里**，只能通过"GT 等待 / 挤占"使 GT 变长来影响帧率；
- 当前 GT 等待≈0 → **cloth 对帧率影响为零**，但消耗着 Worker 池余量；余量耗尽（更多实例 / 更少核心 / 更多并行系统）时会沿 6.5 的传导点变现；
- 判定流程：`stat unit` 看 Game / Draw / GPU 谁最大定位瓶颈——GPU/Draw 瓶颈时 cloth 无关；Game 瓶颈且随披风数 / 自碰撞开关显著变化时，才归因 cloth。

---

## 七、优化建议

1. **关闭 `Use Self Intersections`（`bUseSelfIntersections`）** —— 直接省 ~2.7 ms/帧（109 ms/40帧）。自相交分析仅在披风严重自穿插时才需要，多数披风用不上。**首选。**
2. **评估 `Use Self Collisions`（`bUseSelfCollisions`）** —— 若视觉可接受，关闭再省 ~2.2 ms/帧；若要保留，**调小 `SelfCollisionThickness` 减少邻近对**（厚度越大 → 邻近对越多 → 越贵；调小需防穿插），或改用更省的球体近似自碰撞方案。
3. **降低披风网格分辨率 / 远处上 LOD** —— 自碰撞检测成本随粒子/三角面数超线性增长，对 `IntersectionQuery` 和 `ProximityQuery` 双重有效；Cloth Paint 收紧 MaxDistance 蒙版（pin 更多顶点）同理。
4. **外部碰撞** —— 确认 `bUseCCD` 是否必要、精简角色碰撞体数量。
5. **Worker 池调度（另一条轴）** —— 目标不是"GameThread 干等"（实测不存在），而是**缓解 Oversubscription、保护同池的并行动画求值**：8 个披风分帧轮转解算 / 降频（30Hz+插值）/ 离屏与远景停模拟 / 控制同屏实例数。
6. **迭代 / 子步 / 刚度最后再微调** —— 求解本身便宜，收益极小；`NumSubsteps` 保持 1，勿调高（会整体翻倍）。

### 全局视图（整段捕获）中的调度开销（Excl 自身耗时 Top）

| 作用域 | Excl | 性质|
|---|---|---|
| PhysicsParallelFor | 302.9 ms | cloth 自碰撞/约束的**内部并行执行容器**（IntersectionQuery/ProximityQuery 的实际工作体）；Incl 重叠， |
| WaitForTasks | 292.6 ms | **任意线程**的通用汇合点真空转部分（TaskGraph.cpp:753），非 cloth 专属 |
| Oversubscription | 170.8 ms | 线程池超额订阅：8 披风派发 + 内部并行把 Worker 占满，其它并行任务排队——**Worker 池压力指标**，cloth 间接影响 GameThread  |
| GameThreadWaitForTask | 166 ms | GameThread 通用汇合点（tick 组完成等）的 idle 累计。**修正：实测等待期间在执行 tick 任务、且 cloth 在窗口内完成，不能归因给 cloth** |
| TaskWorkersIsLookingForWork | 152.5 ms | worker 空转找活 |
| ChaosNewCape | 44.3 ms（Incl 387 ms） | **已正名（见 6.7）**：网格资产名对象作用域，聚合 cloth 任务（Worker）+ 动画收尾任务（GameThread）；Excl 主要是动画收尾固定成本，非 cloth 专属 |

---

## 八、结论

> **解算侧（Worker）**：选区 40 帧内，披风（8 模型）每帧解算约 **7.8 ms（Worker 跨线程累加 CPU，非帧墙钟）**，其中 **65~70% 是自碰撞检测**：头号为「自相交分析 IntersectionQuery」（38%），其次为「自碰撞 ProximityQuery」（27%）；实际约束求解（边/弯曲/拉伸）非常便宜。优化应优先关闭/削减自相交与自碰撞，而非调迭代与刚度。
>
> **线程侧（GameThread）**：ChaosCloth 的布料模拟、自相交与碰撞运算**全部在 Worker 线程**（内部 `PhysicsParallelFor` 再并行）；GameThread 只做"签收上帧结果（≈0 等待）→ 打包输入 → 派发 → EOF 转交渲染"的收发工作，**不做任何解算**。实测 cloth **未堵塞 GameThread**——其运算速率只有在超出一帧异步窗口时，才会通过签收点 / EOF 同步点 / Worker 池挤占传导到 GameThread 调度。
>
> **帧率侧**：帧时间由 `max(GameThread, RenderThread, GPU)` 决定，Worker 上的 cloth 目前不进入该公式，**对当前帧率影响为零**；它消耗的是 Worker 池余量（Oversubscription 已偏高），实例增多 / 核心减少 / 并行系统变多时需重新评估。
