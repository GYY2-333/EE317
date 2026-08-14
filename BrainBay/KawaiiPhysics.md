# KawaiiPhysics 披风解算性能分析报告

## 一、基本信息

| 项目 | 内容 |
|---|---|
| Trace 文件 | `C:\Users\guoyueyuan\AppData\Local\UnrealEngine\Common\UnrealTrace\Store\001\20260728_144203.utrace`（82.95 MB） |
| 分析工具 | Unreal Insights（UE 5.5 源码版，`D:\UESourceCode\UnrealEngine\Engine\Binaries\Win64\UnrealInsights.exe`） |
| 分析对象 | KawaiiPhysics 插件对披风（Cloak/Cape）的运行时解算消耗 |
| 场景 | 8 个相同的带披风模型（`/Game/KawaiiTest/SKM_ArmorH1M`），循环播放同一动作 |
| **分析选区** | **手动框选 40 个 Game 帧** |
| 实例换算 | 每类顶层计时器 Count = 320 = 8 模型 × 40 帧（每模型每帧解算一次，1 个 KawaiiPhysics 节点/披风，已验证） |
| 采集前提 | 重录并启用 `-statnamedevents`（`SCOPE_CYCLE_COUNTER` 才会作为 CPU Timer 进入 Timing/Timers；否则不可见） |

> 说明：每帧 8 模型合计 = Incl ÷ 40；单模型每帧 = Incl ÷ 320。
> KawaiiPhysics 全部作用域用 `SCOPE_CYCLE_COUNTER`（`STATGROUP_Anim`），非 `TRACE_CPUPROFILER_EVENT_SCOPE`。

---

## 二、总体消耗（40 帧选区）

层级关系（源码确认）：
`KawaiiPhysics_Eval`（节点求值总入口 `Evaluate_AnyThread`）→ `UpdatePhysicsSetting`/各限制体更新 → `SimulateModifyBones`（`Simulate` / `AdjustByCollision` / `AdjustByLimitsAndLength` / `AdjustByBoneConstraint`）→ `ApplySimulateResult`。

| 指标 | 数值 |
|---|---|
| 披风每帧总消耗（8 模型合计） | **44.5 ms ÷ 40 = 1.11 ms/帧** |
| 单模型每帧消耗 | 44.5 ms ÷ 320 = **≈139 µs** |
| 线程归属 | **全部在 Worker 线程（并行动画求值 ParallelAnimEvaluation）** |
| 关键路径影响 | 1.11 ms 为**跨 Worker 累加 CPU 时间**，非帧墙钟；实际墙钟增量 ≈ 单模型量级 + 调度开销 |

> `KawaiiPhysics_Eval` 是最外层作用域，其 Incl = 全部披风解算总成本。各子作用域 **Excl 之和 ≈ 44.3 ms** 与 Eval 的 44.5 ms 吻合，证明所有解算都嵌在 Eval 内，用 Excl 拆分不会重复计数。

---

## 三、KawaiiPhysics 主要计时器消耗（`KawaiiPhysics` 过滤，40 帧）

Group by: Instance Count ｜ Mode: Instance ｜（已剔除同名非解算项 `L_KawaiiValidation.DirectionalLight`、`SkeletalMesh:SKM_ArmorH1M`）

| 计时器（作用域名） | Count | Incl | Excl | 每帧(8模型) | 说明 |
|---|---|---|---|---|---|
| **`KawaiiPhysics_Eval`** | 320 | **44.5 ms** | 17.7 ms | **1.11 ms** | **节点求值总入口（含全部子作用域）** |
| `KawaiiPhysics_SimulateModifyBones` | 320 | 8.9 ms | 2.1 ms | 0.22 ms | 逐骨模拟总调度 |
| `KawaiiPhysics_ApplySimulateResult` | 320 | 5.2 ms | 4.0 ms | 0.13 ms | 模拟结果回写骨骼 |
| `KawaiiPhysics_AdjustByCollision` | 7,392 | 5.1 ms | 5.1 ms | 0.13 ms | 碰撞修正（逐骨，≈23/披风） |
| `KawaiiPhysics_UpdateCapsuleLimit` | 8,320 | 5.0 ms | 4.7 ms | 0.125 ms | 胶囊限制体更新（26 次/帧/披风） |
| `KawaiiPhysics_UpdateModifyBonesPoseTransform` | 13,760 | 4.5 ms | 4.5 ms | 0.11 ms | 骨骼姿势变换（43 次/帧/披风） |
| `KawaiiPhysics_UpdatePhysicsSetting` | 320 | 1.8 ms | 1.8 ms | 0.045 ms | 物理设置更新 |
| `KawaiiPhysics_UpdateSphericalLimit` | 640 | 1.3 ms | 1.3 ms | 0.033 ms | 球形限制体更新（2 次/帧/披风） |
| `KawaiiPhysics_AdjustByLimitsAndLength` | 176 | 798.8 µs | 798.8 µs | 0.020 ms | 限制体 + 骨长约束 |
| `KawaiiPhysics_ConvertSimulationSpaceTransform` | 22,720 | 786.5 µs | 786.5 µs | 0.020 ms | 模拟空间变换转换（71 次/帧/披风） |
| `KawaiiPhysics_Simulate` | 7,392 | 692.9 µs | 692.9 µs | 0.017 ms | 逐骨积分（≈23/披风） |
| `KawaiiPhysics_ConvertSimulationSpaceLocation` | 13,440 | 429.8 µs | 429.8 µs | 0.011 ms | 模拟空间位置转换 |
| `KawaiiPhysics_ConvertSimulationSpaceRotation` | 11,520 | 323.5 µs | 323.5 µs | 0.008 ms | 模拟空间旋转转换 |
| `KawaiiPhysics_ConvertSimulationSpaceVector` | 320 | 53.7 µs | 53.7 µs | ~0 | 模拟空间向量转换 |
| `KawaiiPhysics_AdjustByBoneConstraint` | 352 | 44.6 µs | 44.6 µs | ~0 | 骨骼约束（几乎未用） |
| `KawaiiPhysics_BridgeDummy` | 176 | 29 µs | 29 µs | ~0 | 虚拟骨桥接 |

---

## 四、`Eval`（44.5 ms）自耗时（Excl）100% 拆解 —— 消耗根源

`Eval` 自身 Excl 17.7 ms（核心逐骨积分/主循环），其余分布于子作用域。以 Excl 排序（不重复计数，合计 ≈ 44.3 ms）：

| 作用域 | Excl(40帧) | 每帧(8模型) | 占 Eval 总 | 归属 / 对应参数 |
|---|---|---|---|---|
| **`KawaiiPhysics_Eval`（自身）** | **17.7 ms** | 0.44 ms | **39.8%** | **核心逐骨积分，随骨骼数×迭代** |
| **`AdjustByCollision`** | **5.1 ms** | 0.13 ms | **11.5%** | **碰撞修正** |
| **`UpdateCapsuleLimit`** | **4.7 ms** | 0.12 ms | **10.6%** | **胶囊碰撞体（`CapsuleLimits`），8,320 次** |
| `UpdateModifyBonesPoseTransform` | 4.5 ms | 0.11 ms | 10.1% | 骨骼姿势变换回写 |
| `ApplySimulateResult` | 4.0 ms | 0.10 ms | 9.0% | 模拟结果回写 |
| `SimulateModifyBones`（自身） | 2.1 ms | 0.05 ms | 4.7% | 逐骨模拟调度 |
| `UpdatePhysicsSetting` | 1.8 ms | 0.045 ms | 4.0% | 物理设置更新 |
| `UpdateSphericalLimit` | 1.3 ms | 0.033 ms | 2.9% | 球形碰撞体（`SphericalLimits`） |
| `AdjustByLimitsAndLength` | 798.8 µs | 0.020 ms | 1.8% | 限制体 + 骨长约束 |
| `ConvertSimulationSpaceTransform` | 786.5 µs | 0.020 ms | 1.8% | 模拟空间转换（非 ComponentSpace 才有） |
| `Simulate`（自身） | 692.9 µs | 0.017 ms | 1.6% | 逐骨积分 |
| `ConvertSimulationSpaceLocation` | 429.8 µs | 0.011 ms | 1.0% | 模拟空间转换 |
| `ConvertSimulationSpaceRotation` | 323.5 µs | 0.008 ms | 0.7% | 模拟空间转换 |
| `ConvertSimulationSpaceVector` | 53.7 µs | ~0 | 0.1% | 模拟空间转换 |
| `AdjustByBoneConstraint` | 44.6 µs | ~0 | 0.1% | 骨骼约束（几乎未用） |
| `BridgeDummy` | 29 µs | ~0 | 0.07% | 虚拟骨桥接 |

### 按类别归并

| 类别 | Excl 合计 | 占比 | 构成 |
|---|---|---|---|
| **核心逐骨积分** | 17.7 ms | **39.8%** | `Eval` 自身 |
| **碰撞相关** | 11.9 ms | **26.7%** | `AdjustByCollision` + `UpdateCapsuleLimit` + `UpdateSphericalLimit` + `AdjustByLimitsAndLength` |
| **结果回写** | 8.5 ms | **19.1%** | `UpdateModifyBonesPoseTransform` + `ApplySimulateResult` |
| 逐骨模拟调度 | 2.8 ms | 6.3% | `SimulateModifyBones` + `Simulate` 自身 |
| 物理设置更新 | 1.8 ms | 4.0% | `UpdatePhysicsSetting` |
| 模拟空间转换 | 1.6 ms | 3.6% | `ConvertSimulationSpace*`（4 项） |
| 骨骼约束 | <0.1 ms | ~0.2% | `AdjustByBoneConstraint` + `BridgeDummy` |

### 关键结论

- **三大头合计约 86%**：逐骨积分（39.8%）+ 碰撞（26.7%）+ 结果回写（19.1%）。
- **碰撞是最容易优化的一块（26.7%）**：`UpdateCapsuleLimit` 每帧每披风重算 **26 次**（8,320 次/40 帧），疑似逐骨重算胶囊限制体变换 —— 有缓存/复用空间。
- **核心积分（39.8%）与结果回写（19.1%）均随骨骼数增长**，根因是每披风约 **23 根**模拟骨骼。
- **骨骼约束、虚拟骨、模拟空间转换极便宜**，非瓶颈。

---

## 五、从 Trace 反推的解算参数（当前值）

| 参数 | 推断值 | 依据 |
|---|---|---|
| 模拟骨骼数 | **≈23 根/披风** | `Simulate` Count(7,392) ÷ 320 ≈ 23.1 |
| 每帧调用次数 | 1 次/模型/帧 | 顶层作用域 Count 均为 320 = 8×40（`NumSubsteps` 无翻倍） |
| **`SimulationSpace`** | **非 ComponentSpace（World 或 BaseBoneSpace）** | 出现大量 `ConvertSimulationSpace*`（ComponentSpace 无需转换） |
| **`CapsuleLimits`（胶囊碰撞体）** | **启用，且数量偏多** | `UpdateCapsuleLimit` 26 次/帧/披风（8,320 次） |
| `SphericalLimits`（球形碰撞体） | 启用（少量） | `UpdateSphericalLimit` 2 次/帧/披风（640 次） |
| `BoxLimits` / `PlanarLimits` | **未使用** | 对应作用域 Count = 0，未出现 |
| `BoneConstraints`（骨骼约束） | **基本未用** | `AdjustByBoneConstraint` 仅 44.6 µs |
| `WarmUp` / `InitModifyBones` | 未触发（稳态） | 对应作用域 Count = 0（未重置模拟） |

对应配置源码位置：`Plugins/KawaiiPhysics/Source/KawaiiPhysics/Public/AnimNode_KawaiiPhysics.h`
- `SimulationSpace`（L607，枚举 `EKawaiiPhysicsSimulationSpace` L41，默认 `ComponentSpace`）
- `SphericalLimits`（L758）、`CapsuleLimits`（L764）、`BoxLimits`（L770）、`PlanarLimits`（L776）
- `BoneConstraints`（L852）、`BoneConstraintsDataAsset`（L860）

> 该版本插件已内置 `FSimulationSpaceCache`（每次求值缓存，H:L1463+）与外力缓存（H:L1050），但当 `SimulationSpace ≠ ComponentSpace` 时仍存在逐骨转换开销。

---

## 六、优化建议


1. **精简胶囊碰撞体 / 复用限制体变换** —— 最高性价比。`UpdateCapsuleLimit` 每帧每披风 26 次（10.6%），叠加 `AdjustByCollision`（11.5%），碰撞合计 26.7%。核实是否**逐骨重算**限制体变换，若可每帧缓存一次即可省下大头；同时减少胶囊碰撞体数量与参与骨骼。
2. **`SimulationSpace` 改回 `ComponentSpace`** —— 若视觉可接受，直接消除 `ConvertSimulationSpace*`（3.6%）及相关开销。当前为 World/BaseBoneSpace 才产生逐骨转换。
3. **减少模拟骨骼数 / 降迭代** —— ≈23 根偏多，直接影响 `Eval` 自身（39.8%）与结果回写（19.1%）这两块占比最大的成本。缩短骨链或合并骨骼收益显著。
4. **多模型 / 远景用 LOD 降解算** —— 8 个完全相同模型，远处降迭代 / 降骨骼 / 停模拟（KawaiiPhysics 支持基于 LOD 的求值控制）。同屏实例越多收益越大。
5. **骨骼约束、虚拟骨、模拟空间转换无需优先处理** —— 均 <4%，调整收益极小。

### 线程 / 并行视角（重要背景）

- 8 个 `KawaiiPhysics_Eval` **全部在 Worker 线程并行执行**，1.11 ms/帧 是跨线程累加 CPU，**当前不在游戏线程关键路径上**。
- 需关注的风险：帧末**同步点**（等待并行动画求值完成）是否因披风占满 Worker 而让游戏线程空等；以及 Worker 数不足 / 被其它任务挤占时的 CPU 竞争。
- 因此上述优化**优先级取决于是否 worker-bound**；若帧率未受动画求值同步点制约，则当前披风解算已属"温和开销"。

---

## 七、结论

> 选区 40 帧内，披风（8 模型）每帧解算约 **1.11 ms（跨 Worker 累加 CPU，全部并行，非关键路径）**，单模型每帧 **≈139 µs**。消耗集中在 **逐骨积分（39.8%）+ 碰撞处理（26.7%）+ 结果回写（19.1%），合计约 86%**；根本驱动因素是**每披风约 23 根模拟骨骼**与**偏多的胶囊碰撞体**，而骨骼约束、模拟空间转换等均可忽略。
>
> 优化应优先**精简碰撞体 / 缓存限制体变换**与**减少骨骼数 / 降迭代 / LOD 降解算**，而非调整骨骼约束或模拟空间转换。相较 ChaosCloth（同场景约 7.8 ms/帧、含昂贵自碰撞），KawaiiPhysics（≈1.11 ms/帧 CPU 且 Worker 并行）在本场景下**性能显著更友好**。
