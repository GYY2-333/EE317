# KawaiiPhysics — Directional Collision 功能实现文档

> 本文档整理 `Plugins/KawaiiPhysics` 中 **Directional Collision（方向/连线碰撞填充）** 功能的完整实现：参数获取、结构体格式、代码修改位置，以及与**原始 KawaiiPhysics**（pafuhana1213 官方版）的对比。
>
> 引擎版本：UE 5.5（源码构建）。插件模块：`KawaiiPhysics`（Runtime）、`KawaiiPhysicsEd`（Editor）。
>
> 路径前缀（下文简写 `<KP>`）：
> `Plugins/KawaiiPhysics/Source/KawaiiPhysics`

---

## 0. 功能概述

原始 KawaiiPhysics **没有** Directional Collision 这一功能。本插件在其基础上新增了一套「以临时探针球加厚碰撞覆盖」的机制，经过多轮迭代，最终形成两个子功能：

| 子功能 | 作用 | 方向 |
|--------|------|------|
| **父子连线碰撞**（DirectionalCollision） | 在每根骨与其父骨的连线上均匀生成碰撞球 | 纵向（沿骨链） |
| **相邻链碰撞**（AdjacentLinkCollision） | 在两条相邻骨链的逐层对应骨之间生成碰撞球 | 横向（列与列之间） |

两者共用同一套「临时探针球 → 走默认球碰撞管线 → 竞争/转嫁 push」的核心机制。

### 演进历史（简述）
1. 初版为「Up/Down/Left/Right 四方向探针 + 子探针链」——已废弃。
2. 重构为「父子连线均匀球（Count + Radius）」——即当前纵向功能。
3. 新增「相邻链横向填充」——即当前横向功能。

> 注：为减少资产破坏，外层属性名 **保留** `DirectionalCollisionSettings`、`bUseDirectionalCollision`、`AdjustByDirectionalProbes`、`InitDirectionalCollision` 等旧名，但内部语义已是「父子连线」。

---

## 1. 核心机制：临时探针球

所有 Directional/Adjacent 碰撞球都不是真实骨骼，而是**每帧临时构造的探针球**，特性如下：

- 不参与 Verlet 积分、不参与 BoneConstraint 约束；
- 复制一份 `FKawaiiPhysicsModifyBone`（继承 IgnoreBones 等碰撞属性），仅替换 `Location/PrevLocation/Radius`；
- 走**与默认球完全相同**的碰撞管线：Sphere / Capsule / Box / Planar（本地 + DataAsset）+ Shared + World sweep；
- 计算 `push = 碰撞后位置 − 原位置`，再决定如何应用：
  - **纵向**：取最深 push 加到本骨中心（纯平移）；
  - **横向**：push 按 alpha 距离比转嫁给两端骨（加权平均防过冲）。

### 共享函数 `RunSingleProbeCollision`
- 位置：`<KP>/Private/AnimNode_KawaiiPhysicsCollision.cpp:679`
- 声明：`<KP>/Public/AnimNode_KawaiiPhysics.h:1070`
- 由纵向 `AdjustByDirectionalProbes` 与横向 `AdjustByAdjacentLinkProbes` 共用，避免碰撞管线逻辑重复。

```cpp
// AnimNode_KawaiiPhysicsCollision.cpp:679
FVector FAnimNode_KawaiiPhysics::RunSingleProbeCollision(
    FComponentSpacePoseContext& Output, const FKawaiiPhysicsModifyBone& TemplateBone,
    const FVector& Center, const FVector& PrevCenter, float Radius,
    const USkeletalMeshComponent* OwningComp)
{
    if (Radius <= 0.0f) return FVector::ZeroVector;

    FKawaiiPhysicsModifyBone ProbeBone = TemplateBone;   // 继承 IgnoreBones 等属性
    ProbeBone.bUseDirectionalCollision = false;          // 防递归
    ProbeBone.Location = Center;
    ProbeBone.PrevLocation = PrevCenter;
    ProbeBone.PhysicsSettings.Radius = Radius;

    // 与默认球同一套碰撞处理
    AdjustBySphereCollision(...); AdjustByCapsuleCollision(...);
    AdjustByBoxCollision(...);    AdjustByPlanerCollision(...);
    if (bUseSharedCollision && !bSharedCollisionSource) { /* Shared 四类 */ }
    if (bAllowWorldCollision) { AdjustByWorldCollision(...); }

    return ProbeBone.Location - Center;  // push
}
```

---

## 2. 结构体格式（参数定义）

文件：`<KP>/Public/KawaiiPhysicsTypes.h`

### 2.1 `FKawaiiPhysicsDirectionalCollisionSetting`（父子连线设置）
位置：`KawaiiPhysicsTypes.h:85`

| 字段 | 类型 | 默认 | meta | 说明 |
|------|------|------|------|------|
| `RootBone` | `FBoneReference` | — | — | 目标骨（子树起点） |
| `bIncludeChildBones` | `bool` | `true` | — | 是否展开整个子树 |
| `ExcludeBones` | `TArray<FBoneReference>` | — | `EditCondition="bIncludeChildBones"` | 排除的骨及其子孙（截断分支） |
| `Count` | `int32` | `1` | `ClampMin=0, ClampMax=10` | 父子连线上均匀球数 |
| `Radius` | `float` | `3.0` | `ClampMin=0` | 统一半径 |

### 2.2 `FKawaiiPhysicsAdjacentLinkSetting`（相邻链设置）
位置：`KawaiiPhysicsTypes.h:132`

| 字段 | 类型 | 默认 | meta | 说明 |
|------|------|------|------|------|
| `TopBone1` | `FBoneReference` | — | — | 相邻链 A 的顶骨 |
| `TopBone2` | `FBoneReference` | — | — | 相邻链 B 的顶骨 |
| `Count` | `int32` | `1` | `ClampMin=0, ClampMax=10` | 每对相邻骨连线球数 |
| `Radius` | `float` | `3.0` | `ClampMin=0` | 统一半径 |

### 2.3 `FKawaiiPhysicsAdjacentSiblingGroup`（兄弟配对分组）
位置：`KawaiiPhysicsTypes.h:167`

用于**分割网格**（如披风由两个独立面组成）时限定兄弟自动配对的范围：只在**组内按填写顺序相邻配对**，跨组不连，避免交界处被误连。

| 字段 | 类型 | 默认 | meta | 说明 |
|------|------|------|------|------|
| `TopBones` | `TArray<FBoneReference>` | — | — | 该组的顶骨列表；仅按顺序相邻的对 (k,k+1) 配对（不含对角） |

> 说明：外层由 `TArray<FKawaiiPhysicsAdjacentSiblingGroup> SiblingGroups` 表达「多组、每组多骨」。套一层结构体是因 UE 不支持 `TArray<TArray<>>` 作 UPROPERTY。

### 2.4 `FKawaiiPhysicsAdjacentPair`（运行时对，非 USTRUCT）
位置：`KawaiiPhysicsTypes.h:181`

```cpp
struct FKawaiiPhysicsAdjacentPair   // Transient，不序列化
{
    int32 BoneIndexA = -1;   // ModifyBones 索引
    int32 BoneIndexB = -1;
    int32 Count = 0;
    float Radius = 0.0f;
};
```
由 `InitAdjacentLinkCollision` 从设置 + 兄弟自动配对展开。

### 2.5 `FKawaiiPhysicsModifyBone` 的运行时缓存字段
位置：`KawaiiPhysicsTypes.h:370 / 377 / 381`（均 Transient）

```cpp
bool  bUseDirectionalCollision = false;   // 本骨是否生成父子连线球
int32 DirectionalCollisionCount = 0;      // 球数
float DirectionalCollisionRadius = 0.0f;  // 半径
```

---

## 3. 节点属性（参数获取入口）

文件：`<KP>/Public/AnimNode_KawaiiPhysics.h`，Category 均为 `Bones|Directional Collision`。

| 属性 | 类型 | 默认 | 位置 | 说明 |
|------|------|------|------|------|
| `DirectionalCollisionSettings` | `TArray<FKawaiiPhysicsDirectionalCollisionSetting>` | — | `AnimNode_KawaiiPhysics.h:79` | 父子连线设置列表 |
| `AdjacentLinkCollisionSettings` | `TArray<FKawaiiPhysicsAdjacentLinkSetting>` | — | `:89` | 相邻链手动配对列表 |
| `bAutoAdjacentSiblings` | `bool` | `false` | `:96` | 兄弟骨自动配对开关 |
| `SiblingAdjacentCount` | `int32` | `1` | `:100` 附近 | 兄弟配对球数 |
| `SiblingAdjacentRadius` | `float` | `3.0` | `:105` 附近 | 兄弟配对半径 |
| `bSiblingIncludeTopBone` | `bool` | `true` | `:116` | 兄弟配对是否含顶骨（否则从次级骨开始） |
| `SiblingGroups` | `TArray<FKawaiiPhysicsAdjacentSiblingGroup>` | — | `:127` | 兄弟配对分组（非空则组内顺次相邻、跨组不连；空则回退全兄弟相邻） |
| `AdjacentPairs`（运行时） | `TArray<FKawaiiPhysicsAdjacentPair>` | — | `:653`（public） | 运行时构建，供 Editor 可视化读取 |

参数由 Editor 节点在编译时经 `CopyNodeDataToPreviewNode` 拷入运行时节点（见 §6）。

---

## 4. 初始化流程（参数 → 运行时）

### 4.1 BoneReference 初始化
`InitializeBoneReferences` — `<KP>/Private/AnimNode_KawaiiPhysicsModifyBones.cpp:35`
- 初始化 `DirectionalCollisionSettings` 的 `RootBone` 与 `ExcludeBones`
- 初始化 `AdjacentLinkCollisionSettings` 的 `TopBone1` / `TopBone2`（`:59` 附近）
- 初始化 `SiblingGroups` 各组的 `TopBones`（`:64` 附近）

### 4.2 父子连线：`InitDirectionalCollision`
位置：`AnimNode_KawaiiPhysicsModifyBones.cpp:624`
- 清空所有骨的 `bUseDirectionalCollision / Count / Radius`
- `ApplyProbesToBone`（`:640`）：把 setting 的 `Count`（Clamp[0,10]）/`Radius` 写入命中的 ModifyBone
- 子树展开（栈遍历 `ChildIndices`）；`IsExcluded`（`:686`）按 BoneName 匹配排除，命中即**截断分支**；inter-bone dummy 按其实子骨判断

### 4.3 相邻链：`InitAdjacentLinkCollision`
位置：`AnimNode_KawaiiPhysicsModifyBones.cpp:740`
- 局部 `FirstRealChild`：取某骨第一个**实子骨**（跳过 dummy）
- 局部 `AddPairChain(TopA, TopB, Count, Radius)`：双链同步下行，每层登记一对，带 `SafetyLimit` 防死循环
- **手动配对**：定位 `TopBone1/TopBone2` → `AddPairChain`
- **兄弟自动配对**（`bAutoAdjacentSiblings`）：局部 `AddSiblingPair`（`:816`）封装「按 `bSiblingIncludeTopBone` 决定起点是否推进一层」再 `AddPairChain`：
  - **分组模式**（`SiblingGroups` 非空，`:828`）：每组解析顶骨为 ModifyBone index（`GroupTops`），按**顺序相邻** (k,k+1) 配对（`:846`），跨组不连——用于分割披风避免交界误连
  - **回退**（`SiblingGroups` 为空，`:868`）：对每骨收集实子骨，相邻兄弟 (k,k+1) 全部配对（后方兼容）

### 4.4 调用点
`<KP>/Private/AnimNode_KawaiiPhysics.cpp`：
- 首次/重建 init：`:418 InitDirectionalCollision();` → `:419 InitAdjacentLinkCollision();`
- 编辑器 live-edit（`WITH_EDITOR` 非 PIE，每帧）：`:394 / :395`

---

## 5. 求解流程（每帧碰撞）

文件：`<KP>/Private/AnimNode_KawaiiPhysicsCollision.cpp`，调用在 `SimulateOnce`（Simulation.cpp）。

### 5.1 纵向 `AdjustByDirectionalProbes`
位置：`AnimNode_KawaiiPhysicsCollision.cpp:726`
调用：`AnimNode_KawaiiPhysicsSimulation.cpp:428`（每骨碰撞循环内、`bSkipSimulate` 之后）

```cpp
// 无父骨则跳过
if (!ModifyBones.IsValidIndex(Bone.ParentIndex)) return;
const auto& ParentBone = ModifyBones[Bone.ParentIndex];

FVector BestPush; float BestPenetrationSq = 0;
const int32 Count = FMath::Clamp(Bone.DirectionalCollisionCount, 0, 10);
for (int32 i = 1; i <= Count; ++i) {
    const float LerpAlpha = float(i) / float(Count + 1);   // 端点不含
    Center     = Lerp(ParentBone.Location,     Bone.Location,     LerpAlpha);
    PrevCenter = Lerp(ParentBone.PrevLocation, Bone.PrevLocation, LerpAlpha);
    Push = RunSingleProbeCollision(Output, Bone, Center, PrevCenter, Radius, OwningComp);
    if (Push.SizeSquared() > BestPenetrationSq) { BestPenetrationSq = ...; BestPush = Push; }
}
Bone.Location += BestPush;   // 最深 push，纯平移
```

### 5.2 横向 `AdjustByAdjacentLinkProbes`
位置：`AnimNode_KawaiiPhysicsCollision.cpp:771`
调用：`AnimNode_KawaiiPhysicsSimulation.cpp:491`（bridge feedback 之后、Constraint-After 之前，单独一 pass）

```cpp
// 独立 scratch（与 bridge dummy 分开）
AdjacentFeedbackPushScratch.SetNumZeroed(NumBones);
AdjacentFeedbackWeightScratch.SetNumZeroed(NumBones);

for (const auto& Pair : AdjacentPairs) {
    const auto& BoneA = ModifyBones[Pair.BoneIndexA];
    const auto& BoneB = ModifyBones[Pair.BoneIndexB];
    for (int32 i = 1; i <= Pair.Count; ++i) {
        LerpAlpha = i/(Count+1);
        Center     = Lerp(BoneA.Location,     BoneB.Location,     LerpAlpha);
        PrevCenter = Lerp(BoneA.PrevLocation, BoneB.PrevLocation, LerpAlpha);
        Push = RunSingleProbeCollision(Output, BoneA, Center, PrevCenter, Pair.Radius, OwningComp);
        // 按 (1-alpha):alpha 转嫁两端
        Scratch[A] += Push*(1-alpha); Weight[A] += (1-alpha);
        Scratch[B] += Push*alpha;     Weight[B] += alpha;
    }
}
// 加权平均应用，divisor=max(1,W) 防 N 倍过冲
for each endpoint: ModifyBones[idx].Location += Scratch[idx] / max(1, Weight[idx]);
```

独立 scratch 成员声明于 `AnimNode_KawaiiPhysics.h`（`AdjacentFeedbackPushScratch` / `AdjacentFeedbackWeightScratch`）。

---

## 6. 可视化与编辑器传递

### 6.1 可视化 `RenderModifyBones`
文件：`<KP>/../KawaiiPhysicsEd/Private/KawaiiPhysicsEditMode.cpp`
- 入口 `RenderModifyBones` — `:171`（由 `:126` 调用）
- 父子连线球（**White**）：`bEnableDebugDrawDirectionalCollision` 块 `:210` 起
- 相邻链球（**Cyan**）+ 对间连线：`:254` 起，遍历 `RuntimeNode->AdjacentPairs`（`:267`）

配色：纵向 White；横向 Cyan。均按 `Count`、`alpha=i/(Count+1)` 复现 Runtime 位置。

### 6.2 属性传递 `CopyNodeDataToPreviewNode`
文件：`<KP>/../KawaiiPhysicsEd/Private/AnimGraphNode_KawaiiPhysics.cpp`
- `:162` `DirectionalCollisionSettings`
- `:163` `AdjacentLinkCollisionSettings`、`bAutoAdjacentSiblings`、`SiblingAdjacentCount/Radius`
- `:167` `bSiblingIncludeTopBone`

---

## 7. 与原始 KawaiiPhysics 的对比

> 原始版 = pafuhana1213 官方 KawaiiPhysics。**该功能整体为本插件新增，原始版不存在**。下表按「文件 / 结构」标明差异。

### 7.1 新增的结构体（原始版无）
| 结构体 | 位置 | 原始版 |
|--------|------|--------|
| `FKawaiiPhysicsDirectionalCollisionSetting` | `KawaiiPhysicsTypes.h:85` | 无 |
| `FKawaiiPhysicsAdjacentLinkSetting` | `KawaiiPhysicsTypes.h:132` | 无 |
| `FKawaiiPhysicsAdjacentSiblingGroup` | `KawaiiPhysicsTypes.h:167` | 无 |
| `FKawaiiPhysicsAdjacentPair` | `KawaiiPhysicsTypes.h:181` | 无 |

### 7.2 `FKawaiiPhysicsModifyBone` 字段对比
| 字段 | 本插件位置 | 原始版 |
|------|-----------|--------|
| `bUseDirectionalCollision` | `KawaiiPhysicsTypes.h:370` | 无 |
| `DirectionalCollisionCount` | `:377` | 无 |
| `DirectionalCollisionRadius` | `:381` | 无 |

> 说明：迭代过程中曾存在 `FKawaiiPhysicsDirectionalProbe`、`FKawaiiPhysicsDirectionalChildProbe`、`DirProbes[4]` 等结构，**已在重构中删除**，当前代码无残留。

### 7.3 `FAnimNode_KawaiiPhysics` 属性对比
| 属性 | 本插件位置 | 原始版 |
|------|-----------|--------|
| `DirectionalCollisionSettings` | `AnimNode_KawaiiPhysics.h:79` | 无 |
| `AdjacentLinkCollisionSettings` | `:89` | 无 |
| `bAutoAdjacentSiblings` / `SiblingAdjacent*` / `bSiblingIncludeTopBone` | `:96`–`:116` | 无 |
| `SiblingGroups` | `:127` | 无 |
| `AdjacentPairs`（运行时） | `:653` | 无 |

### 7.4 新增/修改的函数
| 函数 | 位置 | 原始版 |
|------|------|--------|
| `RunSingleProbeCollision` | `Collision.cpp:679` | 无 |
| `AdjustByDirectionalProbes` | `Collision.cpp:726` | 无 |
| `AdjustByAdjacentLinkProbes` | `Collision.cpp:771` | 无 |
| `InitDirectionalCollision` | `ModifyBones.cpp:624` | 无 |
| `InitAdjacentLinkCollision` | `ModifyBones.cpp:740` | 无 |
| `InitializeBoneReferences`（**修改**：加 Directional/Adjacent/SiblingGroups 骨初始化） | `ModifyBones.cpp:35` | 原始版仅初始化 RootBone/Limits/BoneConstraint 等，无本功能的骨 |

### 7.5 对原始执行流程的插入点
| 位置 | 修改 | 原始版 |
|------|------|--------|
| `SimulateOnce` 每骨碰撞循环末 `Simulation.cpp:428-431` | 每骨碰撞后调 `AdjustByDirectionalProbes` | 原始版此处只有默认球 + World 碰撞，无探针 |
| `SimulateOnce` bridge feedback 后 `Simulation.cpp:491` | 新增横向 `AdjustByAdjacentLinkProbes` pass | 无 |
| `EvaluateSkeletalControl` 首次 init `AnimNode.cpp:418-419` | 新增两个 Init 调用 | 无 |
| Editor 可视化 `KawaiiPhysicsEditMode.cpp:210 / :254` | 新增 White/Cyan 球绘制 | 原始版 EditMode 无此绘制 |
| Editor 属性传递 `AnimGraphNode_KawaiiPhysics.cpp:162-168` | 新增属性拷贝（含 `SiblingGroups`） | 无 |

---

## 8. 关键设计取舍备注

- **命名保留**：外层名保留 `Directional*` 旧名以减少资产破坏；内部语义已是「父子/相邻连线」。
- **临时探针不参与积分/约束**：性能优先的近似，横向 push 切向分量通过 length restore 保留，径向分量部分被抵消。
- **Count 上限 10**：Init/求解/可视化三处均 Clamp[0,10]。
- **横向 push 独立 scratch**：`AdjacentFeedback*` 与 bridge dummy 的 `BridgeFeedback*` 分开，互不干扰。
- **兄弟配对分组**：`SiblingGroups` 非空时按「组内顺次相邻」配对、跨组不连，用于**分割网格**（如披风由两个独立面组成、六骨同父）避免交界处被误连；为空时回退「同父兄弟全相邻」保持向后兼容。典型用法：
  ```
  bAutoAdjacentSiblings = true
  SiblingGroups:
    Group[0].TopBones = [ cloak_L_01, cloak_L_02, cloak_L_03 ]   // 左面
    Group[1].TopBones = [ cloak_R_01, cloak_R_02, cloak_R_03 ]   // 右面
  // 结果：左面 L1-L2、L2-L3 各自下行填球；右面同理；交界 L3-R1 不连
  ```
- **已知的待改进项（当前未修）**：
  1. `AdjustByAdjacentLinkProbes` 未检查两端骨 `bSkipSimulate`（LOD 场景可能污染有效端点的 push）；
  2. `KawaiiPhysicsEditMode.cpp:281` 存在未使用变量 `PrevDrawCenter`；
  3. 兄弟/手动配对可能产生重复 pair（有加权平均防过冲，不崩溃）；
  4. `ShouldReinitModifyBones` 未追踪 Adjacent/Directional 设置变更（运行时改这些设置需重建 ModifyBones 才生效；与原始纵向设置行为一致）。

---

## 9. 功能的作用阶段（运行时时序）

### 9.1 一句话定位

它是 **KawaiiPhysics 动画节点（骨骼控制器）在动画蓝图求值时**的一个子步骤，属于**动画线程（Worker Thread）的姿势求值阶段**，在 **Chaos 主物理之外**，晚于主物理、早于蒙皮渲染。它是纯数学的骨骼后处理，**不进入物理引擎场景**。

### 9.2 以 UE 每帧流水线定位（宏观）

```
游戏帧 Tick
 └─ 世界 Tick
     ├─ Actor/Component Tick
     ├─ SkeletalMeshComponent Tick
     │    └─ 触发 Animation Update / Evaluation
     │         ├─ [GameThread] UpdateAnimation（更新时间/曲线/通知）
     │         └─ [Worker Thread] ParallelEvaluation（求姿势）★ 本功能在此
     ├─ 主物理模拟（Chaos：刚体/布料 Cloth）  ← 与本功能无关，另一套系统
     └─ 渲染提交
```

本功能不在 Chaos 物理引擎里，而是在 SkeletalMesh 的**动画求值**里，靠数学解算修正骨骼位置。

### 9.3 以 KawaiiPhysics 节点内部流程定位（微观）

KawaiiPhysics 是一个 `FAnimNode_SkeletalControlBase`（AnimGraph 骨骼控制节点，类似 Control Rig / AnimDynamics）。每帧 `EvaluateSkeletalControl_AnyThread`（`AnimNode_KawaiiPhysics.cpp`）执行顺序：

```
EvaluateSkeletalControl_AnyThread
 1. 构建仿真空间缓存
 2. 首次/重建 → InitModifyBones / InitDirectionalCollision / InitAdjacentLinkCollision  ← 【初始化阶段】
 3. UpdatePhysicsSettings（曲线调制每骨参数）
 4. 更新各碰撞体到仿真空间
 5. UpdateModifyBonesPoseTransform（读输入姿势）
 6. SimulateModifyBones
     └─ SimulateOnce（固定子步长时每步跑一次）
          a. Verlet 积分（重力/风/刚性）
          b. 默认球碰撞（Sphere/Capsule/Box/Planar/Shared/World）
          c. ★ AdjustByDirectionalProbes（纵向父子连线球）— 每骨碰撞后
          d. bridge dummy 反馈
          e. ★ AdjustByAdjacentLinkProbes（横向相邻链球）— 单独 pass
          f. BoneConstraint（After Collision）
          g. 角度限制 + 平面约束 + 骨长复原
 7. ApplySimulateResult（写回骨骼 Transform）
```

**两个作用点（★）都在第 6 步 `SimulateOnce` 内、碰撞求解阶段**：
- **纵向**（`AdjustByDirectionalProbes`）：在**每根骨的默认球碰撞之后**立刻执行（`AnimNode_KawaiiPhysicsSimulation.cpp:428`），加固该骨碰撞覆盖。
- **横向**（`AdjustByAdjacentLinkProbes`）：作为**独立的一个 pass**，在所有骨碰撞完、bridge feedback 之后、Bone Constraint(After) 之前执行（`AnimNode_KawaiiPhysicsSimulation.cpp:491`）。

### 9.4 关键时序特征

| 特征 | 说明 |
|------|------|
| 线程 | 动画 Worker 线程（AnyThread），非游戏主线程 |
| 每帧频率 | 每次动画求值一次；开启固定子步长时 `SimulateOnce` 每帧跑 N 次（1–4），探针随之跑 N 次 |
| 相对主物理 | 在 Chaos 主物理**之后读取世界碰撞体**（World Collision sweep 查询当前物理场景），但自身不写入物理场景 |
| 相对渲染 | 在骨骼姿势最终确定前——修改的骨骼位置由 `ApplySimulateResult` 写回，再驱动蒙皮渲染 |
| 数据依赖 | 输入姿势（上游动画）+ 场景碰撞体；输出修正后的骨骼 Transform |

### 9.5 初始化 vs 每帧求解（两个不同阶段）

- **初始化阶段**（第 2 步，仅首次或拓扑设置变更时）：`InitDirectionalCollision` / `InitAdjacentLinkCollision` 解析设置、构建每骨 Count/Radius 缓存与 `AdjacentPairs` 对列表。**不做碰撞**，只准备数据。
- **每帧求解阶段**（第 6 步）：生成临时探针球、跑碰撞、把 push 应用到骨骼。

### 9.6 触发前提（什么情况下才参与）

1. AnimGraph 里放置了 KawaiiPhysics 节点且 `IsValidToEvaluate` 通过；
2. `DeltaTime > 0`（暂停/首帧不跑）；
3. 对应骨设置了 `Count > 0 && Radius > 0`（纵向 `bUseDirectionalCollision`，横向 `AdjacentPairs` 非空）；
4. 不在 WorldSpace 传送帧（传送时跳过整个 Simulate）。

### 9.7 总结

> 本功能作用在 **动画蓝图求值阶段**（动画 Worker 线程），是 KawaiiPhysics 骨骼控制节点每帧 `SimulateModifyBones → SimulateOnce` 里**碰撞求解子步骤**的一部分：纵向球紧跟每骨默认球碰撞之后，横向球作为独立 pass 在所有碰撞之后。它发生在 **Chaos 主物理之后、蒙皮渲染之前**，通过临时探针球做纯数学的骨骼位置修正，不进入物理引擎场景。

---


