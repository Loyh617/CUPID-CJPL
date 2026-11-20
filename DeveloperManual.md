# CUPID-CJPL Developer Manual

本文面向维护和扩展框架的开发者，解释当前代码的模块职责、接口契约、调用方式和扩展步骤。用户侧的快速开始见 [README](README.md)，全部自定义宏命令见 [commands.mac](macros/commands.mac)。

**核对基准：2026-11-19，本地 main 的程序版本 0da6310；Geant4 11.4.1、ROOT 6.36.10、C++17，运行环境为 Windows 11 上的 WSL Ubuntu。** 本文描述现有实现；标注“扩展示例”的代码表示开发方法，不表示仓库已注册该功能。文中的 C++ 片段除完整类示例外，均需要放入相应函数并包含所用类型的头文件。

## 目录

1. [整体职责与一次运行的流程](#1-整体职责与一次运行的流程)
2. [源码布局与配置入口](#2-源码布局与配置入口)
3. [geometry：几何构建与体积查询](#3-geometry几何构建与体积查询)
4. [materials 与 PhysicsList](#4-materials-与-physicslist)
5. [generator：事件发生器](#5-generator事件发生器)
6. [toolkit：采样器与核素发射器](#6-toolkit采样器与核素发射器)
7. [SensitiveDetector、Hit 与 TrackingAction](#7-sensitivedetectorhit-与-trackingaction)
8. [record：统计策略与管理器](#8-record统计策略与管理器)
9. [output：ROOT 输出与运行管理](#9-outputroot-输出与运行管理)
10. [宏文件和完整运行示例](#10-宏文件和完整运行示例)
11. [analyse：离线处理接口](#11-analyse离线处理接口)
12. [常见扩展任务](#12-常见扩展任务)
13. [验证、复现与协作](#13-验证复现与协作)
14. [当前限制与维护注意事项](#14-当前限制与维护注意事项)

## 1. 整体职责与一次运行的流程

四大模块以 Geant4 的用户初始化类和 Action 为入口：

| 模块 | 输入 | 主要输出 | 谁调用 |
| --- | --- | --- | --- |
| geometry | 组件选择、尺寸、材料 | World 内的物理体、源体积注册表、LMO 逻辑体目录 | DetectorConstruction |
| generator | 发生器选择、源参数、几何信息、随机数 | G4Event 的初级顶点与初级粒子 | PrimaryGeneratorAction |
| record | 本事件的 Hit 集合、轨迹祖先表 | 有类型的记录向量 | EventAction |
| output | 记录向量、RunID、探测器目录、文件配置 | 各记录器的 ROOT 文件和运行日志 | RunAction、EventAction |

材料库和 PhysicsList 为这些模块提供共同条件。SensitiveDetector 和 TrackingAction 采集记录器的输入，位于模块外层。离线 analyse 宏读取已完成的 ROOT 文件，不参与 Geant4 的事件循环。

### 1.1 启动和初始化

[main.cc](main.cc) 按以下逻辑装配程序：

1. 解析命令行，确定宏路径、串行/多线程、可视化和输出父目录。
2. 创建 RunLog，保留终端显示并捕获日志，记录输入宏文本。
3. 用 G4RunManagerFactory 创建 SerialOnly 或 MTOnly run manager。
4. 注册 DetectorConstruction、PhysicsList、ActionInitialization。
5. 执行宏；宏中的 `/run/initialize` 触发几何、物理和用户 Action 初始化。

DetectorConstruction::Construct() 先创建材料和 World，再调用 GeometryManager::Build()。ConstructSDandField() 将线程对应的 LMOSensitiveDetector 挂到已注册的 LMO 逻辑体上。

ActionInitialization::BuildForMaster() 仅注册主线程 RunAction；Build() 为每个事件处理线程创建 PrimaryGeneratorAction、RunAction、EventAction、SteppingAction、TrackingAction、StackingAction。串行模式也走事件处理的 Build() 路径。

### 1.2 一次 run 和一个 event

```text
/run/beamOn N
  RunAction::BeginOfRunAction
    确定探测器目录、运行目录、文件名，准备 ROOT 表并打开输出
  对分配给本线程的每个 event：
    PrimaryGeneratorAction → GeneratorManager → 初级顶点/粒子
    LMOSensitiveDetector::Initialize → 创建本事件 Hit 集合
    EventAction::BeginOfEventAction → 清空祖先表和上次记录
    对每条轨迹：
      TrackingAction::PreUserTrackingAction → 保存出生信息
      每个 LMO 内的 step → LMOSensitiveDetector::ProcessHits
                           → DetectorHit 累加沉积能量
    EventAction::EndOfEventAction
      → RecordManager::Build
      → 各记录器生成向量
      → OutputManager::Write → RootOutput 填写 ntuple
  RunAction::EndOfRunAction
    各线程 Write/CloseFile，Geant4 合并，主线程检查文件并输出统计
```

这是业务数据流示意；初级生成发生在 BeginOfEventAction 之前，因此生成器不要依赖 BeginOfEventAction 才初始化的记录器状态。程序没有把一个 Geant4 event 切分为探测器时间窗。

### 1.3 对象所有权和线程边界

| 对象 | 生命周期/所有权 | 开发时的约束 |
| --- | --- | --- |
| 用户初始化类、注册的 Action | 注册后由 Geant4 管理 | 不重复 delete |
| GeometryManager | DetectorConstruction 持有 | 初始化阶段建立注册表，输运时只读 |
| G4 实体、逻辑体、物理体、材料 | 由 Geant4 相应存储机制管理 | 注册表中的指针不拥有对象 |
| GeneratorManager、具体发生器 | 各事件处理线程独立持有 | 可变采样状态不跨线程共享 |
| EventAction、TrackAncestry、RecordManager | 每线程，按事件清空 | 不保存上一事件轨迹关系 |
| DetectorHitsCollection/DetectorHit | 当前事件拥有 | 不将 Hit 指针留到下一个 event |
| OutputManager/RootOutput | 每个 RunAction 一份 | 同线程只允许一个 RootOutput 适配器 |
| G4AnalysisManager | Geant4 管理，线程局部实例 | 由 RootOutput 统一使用 |
| RunLog | main 创建，覆盖整个应用生命周期 | 主线程切换 run.log；所有线程可输出日志 |

记录器数据应在当前 EndOfEventAction 内消费；RecordManager::Clear() 后，先前结果不再代表可用的事件记录。

## 2. 源码布局与配置入口

```text
CUPID-CJPL/
  main.cc
  CMakeLists.txt
  include/                       # 声明
    geometry/                    # GeometryManager、组件、参数、Messenger
    materials/                   # Materials
    generator/
      toolkit/sampler/           # 四种位置采样器
      toolkit/NuclideEmitter.hh
      calibration/th232/        # 当前为占位类
      calibration/co56/         # 当前为占位类
      lmo_signal/double_beta_decay/
    record/
    output/
    *.hh                         # Construction、PhysicsList、Action、SD、Hit
  src/                           # 与 include 对应的实现
  macros/                        # 自包含运行宏及命令参考
  analyse/                       # 独立 ROOT 离线宏
  tests/                         # 软件与物理相关检查
  docs/                          # 图和现有审查资料
  DeveloperManual.md
  README.md
  history.log
  results/                       # 运行输出，不进入 Git
  build/                         # 构建输出，不进入 Git
```

名称在当前版本中是 **RecordManager**、**EventEnergyRecorder**、**IncidentEnergyRecorder**。讨论中的 EventRecorder/IncidentRecorder 是简称，不是可传给管理器的名称。

| 需要改变的内容 | 入口 | 是否需要重新编译 |
| --- | --- | --- |
| 启用哪个已有几何组件、LMO 数量 | /cupid/geometry/* | 否；在 PreInit 设置 |
| 尺寸、阵列排列、容器轮廓 | GeometryParameters.hh、组件 .cc | 是 |
| 材料配方 | Materials.cc | 是 |
| 发生器选择 | /cupid/generator/select | 否；在 PreInit 设置 |
| 粒子枪参数 | Geant4 原生 /gun/* | 否；初始化后设置 |
| 位置采样算法 | 具体 generator 内部 | 是 |
| 记录器选择 | EventAction::DefaultRecorders() | 是 |
| 输出后端选择 | ActionInitialization::CreateRunAction() | 是 |
| 输出目录、名称前缀 | /cupid/output/* | 否；在 Idle 设置 |
| 线程数 | --threads 或 --serial | 否；启动时设置 |
| 事件数、种子 | /run/beamOn、/random/setSeeds | 否 |
| 分支名、单位转换、字段映射 | RootOutput.cc | 是 |

当前没有统一 GeneratorConfig，也没有宏命令选择采样器、记录器或输出后端。不要在宏里使用尚未注册的命令。

## 3. geometry：几何构建与体积查询

源码入口：[GeometryManager.hh](include/geometry/GeometryManager.hh)、[GeometryManager.cc](src/geometry/GeometryManager.cc)、[GeometryParameters.hh](include/geometry/GeometryParameters.hh)。

### 3.1 Solid、LogicalVolume、PhysicalVolume

- Solid 定义形状。
- LogicalVolume 将形状与材料、显示属性、灵敏探测器关联。
- PhysicalVolume 表示一次具体放置，提供母体、位置、旋转、名称及 copy number。

当前 LMOArray 可共用一个 G4Box solid，但为每块晶体分别创建逻辑体和物理体。二者同名为 LMO0、LMO1，copy number 对应 0、1。不能仅改物理体名称却忽略逻辑体目录和 copy number。

### 3.2 DetectorConstruction 和组件 Build()

Construct() 创建边长 4 m 的 World，材料为 G4_Galactic，并设为不可见。不可见 World 不等于可视化背景颜色。

组件继承 [VGeometry](include/geometry/VGeometry.hh)，以 override 实现统一接口；签名不匹配会在编译时被发现。基类只有纯虚 Build 和虚析构函数，无需单独的 .cc。组件的声明形式为：

```cpp
void Build(G4VPhysicalVolume* parent,
           const Materials& materials,
           const GeometryParameters& parameters,
           GeometryManager& geometry) const override;
```

GeometryManager 的 Component 保存 std::unique_ptr<VGeometry>，按注册顺序调用 builder->Build(...)。管理器拥有组件构建器；Geant4 几何对象仍由其几何 stores 管理，构建器析构时不删除这些体积。const 约束构建器自身，不阻止通过 geometry 登记体积。

parent 是实际母物理体；组件使用其逻辑体放置子体，再向 geometry 注册。当前四个组件均由管理器传入 World，分别构建自己的物理体，不能从示意图外观推断它们存在未实现的真空容器母子层级。

| 组件 | 实现 | 参数来源/用途 |
| --- | --- | --- |
| Cryostat | [Cryostat.cc](src/geometry/Cryostat.cc) | plates 来自 GeometryParameters；壳体轮廓在 Sections() |
| InnerShielding | [InnerShielding.cc](src/geometry/InnerShielding.cc) | innerShield 和 shieldOuterRadius |
| LMOArray | [LMOArray.cc](src/geometry/LMOArray.cc) | lmoCount、lmoSide、lmoGap、lmoCenter |
| WThWireInCopperCup | [WThWireInCopperCup.cc](src/geometry/CalibrationSource/WThWireInCopperCup.cc) | 在 LMOArray 后构建；读取已登记 LMO0，源尺寸和间距在组件文件内定义 |

Cryostat 使用圆盘、布尔减法孔和旋转轮廓；局部圆柱轴 Z 旋转到 World 的 Y。MXC 为简化平板，铝壳底部为离散轮廓近似，几何不等于完整工程 CAD。

### 3.3 GeometryManager 接口

| 接口 | 作用及调用条件 |
| --- | --- |
| SetEnabled(name, enabled) | 开关已注册组件；宏入口只允许 PreInit |
| SetLMOCount(count) | 当前只接受 1 或 2 |
| Parameters() | 返回只读参数引用 |
| List() | 打印组件选择及 LMO 数量 |
| Build(world, materials) | 开启注册，依次构建启用组件，完成注册 |
| BeginSourceVolumes(world) | 构建阶段清空注册表、增加几何代数、登记 World |
| RegisterSourceVolume(volume, parent) | 登记放置及其局部到 World 变换；母体必须先登记 |
| RegisterLMOVolume(volume) | 在源登记之后登记 LMO 计分逻辑体 |
| FinishSourceVolumes() | 检查名称和放置约束，关闭注册 |
| FindSourceVolume(name) | 初始化后按物理体名称查询，未知名称报错 |
| FindConstructionVolume(name) | 仅在主线程构建注册期间查询已登记体积，用于组件之间的定位依赖 |
| GetWorldVolume() | 返回已初始化 World |
| GetLMOLogicalVolumes() | 给 SD 挂载和输出探测器目录提供逻辑体列表 |
| GetLMOPlacements() | 返回 LMO 中心、旋转和半长；要求 G4Box 且无子体 |
| GetGeometryGeneration() | 几何代数，供采样器判断缓存失效 |
| CheckOverlaps(samples) | 检查全部非 World 放置，返回 true 表示本次抽样检查通过 |

注册表支持普通放置，不支持 replica、parameterised placement 或被重复放置的母逻辑体。已注册源的物理体名称必须全局唯一；返回的指针和引用仅对当前几何代数有效。

SourceVolumeInfo 的三个成员：

```cpp
G4VPhysicalVolume* physicalVolume;
G4RotationMatrix localToWorldRotation;
G4ThreeVector localToWorldTranslation;
```

坐标关系为：

```text
WorldPosition = localToWorldTranslation + localToWorldRotation * LocalPosition
```

这让发生器无需从 World 自行遍历查找目标。拒绝采样仍需用 navigator 判断候选点最终属于哪块材料体积。

### 3.4 坐标、名称和尺寸

当前原点在制冷机轴线上、MXC 盘底面；Y 为轴向。LMO 中心默认 (0,-350,0) mm，边长 45 mm、间隙 5 mm。两晶体中心为 (-25,-350,0) 和 (25,-350,0) mm。尺寸输入必须区别“全长”与 Geant4 实体构造常用的“半长”。

源查询可使用 `Cryostat.Plate.MXC`、`Cryostat.Vessel.InnerLower`、`InnerShielding.Body`、`LMO0` 等已登记物理体名。带点的源体名合法；输出探测器树名称另有约束，只接受字母/下划线开头的字母数字下划线组合。

宏示例：

```text
/cupid/geometry/enable Cryostat
/cupid/geometry/enable LMOArray
/cupid/geometry/lmoCount 2
/run/initialize
/cupid/geometry/list
/cupid/geometry/check 10000
```

所有组件最初关闭。增加晶体数量不能仅解除 count 上限；还应修改阵列排列、SD 编号约定、几何验证和相关示例。

### 3.5 WThWireInCopperCup 刻度源组件

文件为 geometry/CalibrationSource/WThWireInCopperCup.hh/.cc，宏组件名称和整体物理体名称为
WThWireInCopperCup。它包含 Wire、CopperCup、CopperTape 三个子体，名称均加
WThWireInCopperCup. 前缀。整体包络不可见，采用母体材料，当前为 World 真空。

```text
/cupid/geometry/enable LMOArray
/cupid/geometry/lmoCount 1
/cupid/geometry/enable WThWireInCopperCup
/run/initialize
/cupid/geometry/check 10000
```

从 LMO-Th232-sim 的地上刻度装置保留的尺寸为：丝直径 1.6 mm、长 10 mm；
铜杯外径 18 mm、总高 4.9 mm，后盖厚 2 mm，内腔直径 14.6 mm、轴向高度
1.9 mm，前方小孔直径 2 mm、孔所在铜层厚 1 mm；胶带按铜圆片建模，半径
1.5 mm、厚 0.005 mm，不另建胶层。

以 LMO0 的局部坐标为准，源位于 -Z 侧、孔朝 +Z，丝轴沿 X。杯的孔端面至
晶体负 Z 表面为 7.5 mm；胶带最靠近晶体的面间距为 7.495 mm。读取实际 LMO0
盒子的半长和已登记的局部到 World 变换，避免照搬旧 20 mm 晶体的中心偏移。

当前 45 mm 晶体下，丝中心相对 LMO0 中心为 (0,0,-31.95) mm；
一块晶体时 World 丝中心为 (0,-350,-31.95) mm，两块时为
(-25,-350,-31.95) mm。没有启用 LMOArray 时构建会明确报错，不隐式启用其他组件。

WThWire 材料为 98% W、2% ThO2 质量比例，密度 18.95 g/cm³；
ThO2 密度 10.0 g/cm³。材料内含 Th 不会自动发射衰变。后续 generator 可调用
RejectionVolumeSampler("WThWireInCopperCup.Wire", geometry)，或用该丝的实际
变换和圆柱尺寸直接采样；不要在整个包络中采样来代替在丝中采样。

## 4. materials 与 PhysicsList

### 4.1 材料库

[Materials::Build()](src/materials/Materials.cc) 先取得元素，再逐一定义材料，最后取得当前几何所需的标准材料。重复 Build() 会复用已有条目，不在输运中修改配方。

```cpp
Materials materials;
materials.Build();                       // Geometry initialization only.
auto* crystal = materials.Get("Li2MoO4"); // Exact material name.
```

Get() 只查已建立的精确名称，不做别名替换，也不会按未知名称自动创建材料。

项目材料有 Vacuum、Air、Copper、Lead、Steel、PTFE、Li2MoO4、NTD_Ge、ThO2、WThWire；还建立 G4_Galactic、G4_Cu、G4_Al、G4_STAINLESS-STEEL。Copper 与 G4_Cu 是独立名称，不能默认当成别名。

当前 Li2MoO4 为天然元素组成，密度 3.07 g/cm³，温度 0.01 K；NTD_Ge 为天然纯锗近似，未建模掺杂。Steel 的 Fe/C 配方不是不锈钢。温度参数不会自动实现低温量热器响应。增加富集 Mo 材料需要明确同位素丰度并显式建模；选择 Mo100 发生器不会自动改变材料同位素组成。

### 4.2 PhysicsList

[PhysicsList.cc](src/PhysicsList.cc) 继承 G4VModularPhysicsList，main 将其注册给 run manager。它使用独立 G4 物理构造器：

| 构造器 | 在本项目中的职责 |
| --- | --- |
| G4EmLivermorePhysics | 电磁输运及低能模型 |
| G4EmExtraPhysics | 光核、电核、μ 核等额外过程 |
| G4DecayPhysics | 不稳定粒子衰变 |
| G4RadioactiveDecayPhysics | 放射性核衰变 |
| G4HadronElasticPhysicsHP | 强子弹性及低能中子 HP |
| G4HadronPhysicsShielding | 强子非弹性、中子俘获/裂变相关配置 |
| G4StoppingPhysics | 停止粒子的俘获/湮灭相关过程 |
| G4IonElasticPhysics | 离子弹性 |
| G4IonQMDPhysics | 离子非弹性 |

G4HadronPhysicsShielding 是一个构造器，程序没有直接实例化完整 Shielding 物理列表。当前还设置裂变碎片产生、荧光、Auger 和原子退激忽略 production cuts；放射性衰变时间阈值设为 1e60 year。该阈值不是寿命、源活度或探测器时间窗。

默认 production range cut 为 0.1 mm，质子为 0 mm。它们控制适用过程的次级产生阈值，不是“粒子低于某能量即停止跟踪”或最大步长。应针对研究对象做 cut 收敛检查。

核素发生器创建母核后由注册过程执行衰变；Mo100 发生器则直接提供两个电子的初态，由 PhysicsList 输运。不要把同一个双贝塔事件既手工产生电子，又让母核重复产生同一衰变。

本手册核对了本机 Geant4 11.4.1 官方源码中的 G4EventManager、G4Track、G4VAnalysisManager、G4BetaDecayCorrections 和 G4EmExtraPhysics 接口，以及 ROOT 6.36.10 安装头文件；不以版本号相近推断 API 兼容。这里只描述代码配置，不替代独立物理验证。

## 5. generator：事件发生器

### 5.1 外层适配与统一接口

[PrimaryGeneratorAction](src/PrimaryGeneratorAction.cc) 实现 GeneratePrimaries(G4Event*)，将事件传给自己的 GeneratorManager。生成失败时设置共享原子错误标志、请求停止 run，并由 main 返回失败状态。

[VGenerator](include/generator/VGenerator.hh) 只有：

```cpp
virtual ~VGenerator() = default;
virtual void Generate(G4Event*) = 0;
```

Generate() 向已给定的事件加入初级顶点，不创建新的 Geant4 event，不调用 beamOn。具体 generator 负责粒子种类、位置、能量、方向、相关性和所需参数；toolkit 不替它决定物理模型。

### 5.2 GeneratorManager 与宏选择

构造接口：

```cpp
GeneratorManager(const G4String& selection,
                 const G4int& verbosity,
                 const GeometryManager& geometry);
```

selection、verbosity 和 geometry 以引用保存，必须比 manager 活得更久。当前由 ActionInitialization 持有配置，PreInit Messenger 修改它们，输运期间不变。

- Generate(event)：按名称分发，Mo100 发生器首次需要时创建。
- Check(selection)：检查名称是否支持；不验证空间、数据集或物理正确性。
- List(selection)：打印选择及模型入口。

目前支持且仅支持 `gun`、`Mo100TwoNu`、`Mo100ZeroNu`。G4ParticleGun 默认每事件一个 1 MeV gamma，位置 (0,-350,-100) mm，沿 +Z。在初始化后用 /gun/* 改参数。

Th232Generator、Co56Generator 现在保留类声明和实现文件，但 Generate() 会抛出未实现异常，也没有接入管理器。Th232 运行宏使用原生粒子枪发射核素。

### 5.3 Mo100DoubleBetaDecayGenerator

接口：

```cpp
enum class Mode { TwoNu, ZeroNu };
Mo100DoubleBetaDecayGenerator(const GeometryManager& geometry, Mode mode);
void Generate(G4Event* event) override;
```

当前实现步骤：

1. 从 GetLMOPlacements() 获取中心、旋转和半长。
2. 按相同晶体 Mo100 库存假设等概率选择晶体。
3. 用 DirectVolumeSampler::SampleBox() 在该晶体内均匀取点。
4. 抽取两电子动能和相关方向，建立同位置、同时间零的一个顶点。
5. 将两个电子作为初级粒子交给 Geant4。

位置缓存随几何代数更新；若无启用 LMO，则报错。若未来晶体质量或富集度不同，须按实际 Mo100 原子数加权选择，不能继续等概率。

Q = 3034.40 keV，忽略反冲。内部动能以 Geant4 单位表示；计算动量的表达式 sqrt(T(T+2mc²)) 在这里使用能量单位的动量数值，不应额外乘除 SI 制的 c。

TwoNu 当前以 (Q−T1−T2)^5 为提议分布，再以两个电子的 pEF 因子拒绝采样；ZeroNu 固定 T1+T2=Q。电子因子通过 G4BetaDecayCorrections::FermiFunction(W) 计算，W 是总电子能量除以 electron_mass_c2，使用 4096 区间插值。

**按当前源码，两种模式都调用相同的角采样循环，密度正比于 1−β1β2 cosθ。** 这只是当前实现的事实，不能据此认定两种模式的物理角分布都已验证。尤其 TwoNu 的角相关近似和 SSD 等谱形修正应独立核对后，才能用于精密效率或谱形研究。本手册未修改模型。

没有产生中微子、子核反冲或激发态退激链，也没有绝对半衰期归一化。建议用 EventEnergyRecorder 得到两电子及其后代在每块晶体中的总沉积能谱；IncidentEnergyRecorder 会按根轨迹分开记录两个初级电子。

## 6. toolkit：采样器与核素发射器

四种采样器均采样**位置**，不设置发射粒子的动量方向。返回 G4ThreeVector，长度保持 Geant4 内部单位。类没有统一的虚拟 Sample 接口，各 generator 直接选用需要的方法。

### 6.1 PointSampler

```cpp
PointSampler(const G4ThreeVector& point);
G4ThreeVector Sample() const;
```

检查坐标有限后原样返回，不消耗随机数，不查询几何，不要求点位于某个源体积。

```cpp
cupid::PointSampler sampler({0., -350.*mm, -400.*mm});
const auto position = sampler.Sample();
```

用于初级顶点时传入 World 坐标。点是否在预期材料中，由 generator 的配置和验证负责。

### 6.2 DirectVolumeSampler

```cpp
DirectVolumeSampler(const G4ThreeVector& center = {},
                    const G4RotationMatrix& localToWorldRotation = {});
G4ThreeVector SampleBox(const G4ThreeVector& halfLengths) const;
G4ThreeVector SampleCylinder(G4double radius, G4double halfLength) const;
G4ThreeVector SampleSphere(G4double radius) const;
```

默认中心为零、旋转为单位阵。尺寸必须有限且严格为正。

| 方法 | 均匀测度与参数 |
| --- | --- |
| SampleBox | 体积均匀；三个参数为各方向半长 |
| SampleCylinder | 实心圆柱体积均匀；轴为局部 Z；r=R√U，高度全长为 2h |
| SampleSphere | 实心球体积均匀；r=R∛U，球面位置方向各向同性 |

```cpp
const auto placements = geometry.GetLMOPlacements();
const auto& lmo = placements.at(0);
cupid::DirectVolumeSampler sampler(lmo.center, lmo.rotation);
const auto position = sampler.SampleBox(lmo.halfLengths);
```

这些方法不查询几何，也不排除子体。若几何盒内有孔或放入其他材料，应改变采样模型；不能仍把整盒均匀采样称为目标材料内部均匀。

### 6.3 DirectSurfaceSampler

```cpp
DirectSurfaceSampler(const G4ThreeVector& center = {},
                     const G4RotationMatrix& localToWorldRotation = {});
G4ThreeVector SampleRectangle(G4double halfX, G4double halfY) const;
G4ThreeVector SampleSphere(G4double radius) const;
G4ThreeVector SampleHemisphere(G4double radius) const;
```

矩形位于局部 XY 平面，z=0；半球仅取局部 z≥0 的曲面，不含底圆盘。全部按面积均匀。程序不提供盒子六个面的组合采样接口。

```cpp
G4RotationMatrix rotation;
rotation.rotateX(90.*deg);
cupid::DirectSurfaceSampler sampler({0., -300.*mm, 0.}, rotation);
const auto position = sampler.SampleRectangle(20.*mm, 30.*mm);
```

采样器不自动给表面点加偏移、不检查导航归属。表面污染模型需明确在材料哪一侧、深度分布及发射方向；不要用随意偏移代替物理模型。

### 6.4 RejectionVolumeSampler

```cpp
RejectionVolumeSampler(const G4String& volume, const GeometryManager& geometry);
G4ThreeVector Sample();
```

必须在几何初始化后构造：

```cpp
cupid::RejectionVolumeSampler sampler("Cryostat.Plate.MXC", geometry);
const auto position = sampler.Sample();
```

构造时按注册表找到目标，取得实体局部包围盒、变换和一个私有 G4Navigator。每次 Sample()：

1. 在局部包围盒均匀取候选点。
2. 要求实体 Inside(local)==kInside。
3. 转为 World 坐标。
4. 用私有 navigator 确认最深层所在物理体就是目标，从而排除子体。
5. 接受或重抽；超过 100000 次报错。

私有 navigator 避免改变输运导航状态。每个 worker 单独持有采样器；几何重建后必须重建这个采样器。稀薄壳体的包围盒接受率可能较低，优化应保持体积均匀性。

体积采样不需要为了“采到表面”扩大包围盒；表面在三维连续体积测度中为零，表面源应选表面采样器。

### 6.5 NuclideEmitter

```cpp
void Emit(G4Event* event, G4int z, G4int a,
          const G4ThreeVector& position) const;
```

通过 G4IonTable 获取基态核素，建立时间零、动能零、电荷零的初级粒子。后续衰变由 PhysicsList 完成。它没有限制核素必须放射性，也不表达活度、衰变链平衡或源封装。

在具体 generator 的 Generate() 中可以这样组合：

```cpp
cupid::PointSampler point({0., -350.*mm, -400.*mm});
cupid::NuclideEmitter emitter;
emitter.Emit(event, 90, 232, point.Sample());
```

此处调用时粒子定义已经初始化；不要在初始化之前访问尚未就绪的离子物理状态。

## 7. SensitiveDetector、Hit 与 TrackingAction

### 7.1 LMOSensitiveDetector 的职责

[LMOSensitiveDetector](src/LMOSensitiveDetector.cc) 继承 G4VSensitiveDetector，负责采集原始输运数据，不选择统计方式，也不写 ROOT。

- SD 名：LMOSD。
- 集合名：Edep。
- 完整集合名：LMOSD/Edep。
- SetDetectorNames(names)：设置顺序与 copy number 对应的探测器名称。
- Initialize(collections)：每事件新建 DetectorHitsCollection，为每块 LMO 创建一个 Hit 槽位。
- ProcessHits(step, history)：由 Geant4 调用，按步前 touchable 的 copy number 选中槽位，采集 TrackID、总沉积能量、步前位置和方向。

必须按**步前体积**归属这一步，不能把在边界结束的步错误归给下一体积。直接调用 ProcessHits() 不是业务侧的常规接口；由 Geant4 在灵敏体积内触发。

### 7.2 DetectorHit 的数据形式

[DetectorHit.hh](include/DetectorHit.hh) 的一个对象对应“一个 event 中的一个探测器”，不是一条 step。

```cpp
struct TrackDeposit {
  G4double edep = 0.;
  G4ThreeVector position;
  G4ThreeVector direction;
};
```

对象内部用 map<TrackID, TrackDeposit> 保存各轨迹在该探测器的沉积。接口：

```cpp
DetectorHit(const G4String& detectorName);
void AddEnergyDeposit(G4int trackId, G4double energyDeposit,
                      const G4ThreeVector& position,
                      const G4ThreeVector& direction);
const G4String& GetDetectorName() const;
const std::map<G4int, TrackDeposit>& GetTrackDeposits() const;
```

AddEnergyDeposit 累加同轨迹能量，但只保存首次调用时的位置和方向。零沉积的访问也会保留，以支持正确的祖先归并；零能记录的过滤在 recorder 聚合后进行。

当前没有存储逐步时间、入射次数、每段路径或所有散射过程。后处理不能从此 map 还原已经丢弃的信息。

### 7.3 TrackAncestry 与 TrackingAction

[TrackAncestry](include/record/TrackAncestry.hh) 是每事件轨迹表：

```cpp
struct TrackOrigin {
  G4int parentId;
  G4String birthVolume;
  G4int pdg;
  G4String creatorProcess;
};
```

| 接口 | 用途 |
| --- | --- |
| RegisterTrack(trackId,parentId,birthVolume,pdg,creatorProcess) | 登记出生信息；拒绝无效输入或同 ID 冲突 |
| Contains(trackId) | 是否已登记 |
| Get(trackId) | 返回只读出生信息，缺失时报错 |
| Clear() | 新事件开始时清空 |

TrackingAction::PreUserTrackingAction() 对每条轨迹首次回调时登记；已登记轨迹再次恢复时不覆盖出生信息。它记录的范围包括不在 LMO 内沉积的轨迹。

creatorProcess 来自 G4Track::GetCreatorProcess()->GetProcessName()；指针为空时记为 Primary。这个字段表示**轨迹出生原因**，不是其进入 LMO 的原因，也不是其在 LMO 中最后一次 step 的物理过程。

只有启用 IncidentEnergyRecorder 时才要求祖先表；RecordManager::RequiresTrackAncestry() 控制 TrackingAction 是否执行登记。

### 7.4 其余 Action

| Action/方法 | 当前行为 |
| --- | --- |
| TrackingAction::PostUserTrackingAction | 占位 |
| SteppingAction::UserSteppingAction | 占位；能量由 SD 收集 |
| StackingAction::ClassifyNewTrack | 所有轨迹返回 fUrgent |
| StackingAction::NewStage / PrepareNewEvent | 占位 |
| EventAction::BeginOfEventAction | 清空祖先表与记录 |
| EventAction::EndOfEventAction | 跳过中止事件，读取 Hit、调用记录器并输出 |

因此没有按粒子类型、衰变时间或寿命过滤、延后轨迹，也没有在 StackingAction 中切开长衰变链。

## 8. record：统计策略与管理器

### 8.1 EventEnergyRecorder

数据结构定义在 [EventEnergyRecorder.hh](include/record/EventEnergyRecorder.hh)：

```cpp
struct EventEnergyRecord {
  G4int EventID;
  G4double Edep;
  G4String DetectorName;
};
```

接口：

```cpp
std::vector<EventEnergyRecord> Build(
    G4int eventId, const DetectorHitsCollection& hits) const;
```

对每块探测器把 Hit 内所有轨迹能量相加，仅当总和 >0 时输出一行。没有额外能量阈值，没有时间积分窗，没有能量分辨率或权重。返回值的 Edep 仍为 Geant4 内部单位。

### 8.2 IncidentEnergyRecorder

数据结构定义在 [IncidentEnergyRecorder.hh](include/record/IncidentEnergyRecorder.hh)：

```cpp
struct IncidentEnergyRecord {
  G4int EventID;
  G4int TrackID;
  G4int PDG;
  G4double Edep;
  G4String DetectorName;
  G4ThreeVector IncidentPosition;
  G4ThreeVector IncidentDirection;
  G4String CreatorProcess;
};
```

接口：

```cpp
std::vector<IncidentEnergyRecord> Build(
    G4int eventId, const DetectorHitsCollection& hits,
    const TrackAncestry& ancestry) const;
```

每行是一个探测器内的一个入射根及归属后代的总沉积。位置、方向、PDG、CreatorProcess 均对应最终根 TrackID，不是各后代信息的平均值。

**当前根判定的精确规则**：从一个在本探测器留有访问记录的 track 向上追溯，若满足任一条件则停止，以该 track 为根：

1. parentId==0，即事件初级粒子。
2. 该 track 的出生物理体名不是当前探测器名。
3. 其母 track 没有当前探测器的访问记录。

否则继续追溯母 track。算法为迭代追溯并缓存路径结果，检查循环和缺失祖先。能量累加完成后仅保存根总 Edep>0 的行。

例如：

| 情况 | 当前输出归并 |
| --- | --- |
| A 从外部进入 LMO0，产生 B，B 在 LMO0 沉积 | B 的能量归到 A |
| A 离开后重新进入同一 LMO0 | 仍是一条 A 记录 |
| B 出生在 LMO0，离开后再回 LMO0，母 A 在该 LMO 有访问 | 仍沿本地祖先链归到 A |
| A 进入 LMO0 后到 LMO1，在 LMO1 产生 B，B 进入 LMO0 | B 在 LMO0 独立成根，不归到 A 的 LMO0 记录 |
| 初级电子直接生成于 LMO0 内 | 以该电子自身为根 |

TrackID 仅在单个 event 中有意义；跨事件关联至少使用 RunID、EventID、探测器名和 TrackID。Geant4 真实轨迹 ID 为正数，ParentID=0 表示初级粒子。

这个算法依赖出生体积和整个事件的访问表，**不是逐次边界穿越、按时序分段的记录算法**。特别是以后加入同一事件复杂的跨体积往返或探测器子结构时，需要重新核对归并语义。入射根统计也不表示实验能逐个分辨这些粒子；量热器脉冲需结合时间、热响应和读出模型。

### 8.3 RecordManager

[RecordManager](src/record/RecordManager.cc) 根据名称选择统计器，持有本事件的结果向量，本身不写文件。

```cpp
cupid::RecordManager records({
  "EventEnergyRecorder", "IncidentEnergyRecorder"
});
records.Build(eventId, collections, ancestry);
const auto& eventRows = records.GetEventRecords();
const auto& incidentRows = records.GetIncidentRecords();
```

这里 collections 是 G4HCofThisEvent 引用，ancestry 是本事件的 TrackAncestry。

| 接口 | 契约 |
| --- | --- |
| 构造函数 names | 支持两个精确名称或空列表；重复/未知名称报错 |
| IsEnabled(name)、GetNames()、Empty() | 查询选择 |
| RequiresTrackAncestry() | 当前等于是否启用 IncidentEnergyRecorder |
| Clear() | 清空两个结果向量 |
| Build(eventId,collections,ancestry) | 先 Clear，扫描兼容 Hit 集合，调用启用记录器 |
| GetEventRecords()/GetIncidentRecords() | 返回 manager 持有的 const vector 引用 |

Build 会扫描所有集合，只消费可转成 DetectorHitsCollection 的集合；跨集合 DetectorName 重复会报错。以后 NTD/LD 若采用同一 Hit 接口可以接入这一路，但还必须配置 SD 和输出名称目录。

### 8.4 选择记录器和调用输出

修改 [EventAction::DefaultRecorders()](src/EventAction.cc)，例如只保留事件总能量：

```cpp
std::vector<G4String> EventAction::DefaultRecorders() {
  return {"EventEnergyRecorder"};
}
```

可选两个、一个或空列表，修改后重新编译。ActionInitialization 将同一列表传给主线程和 workers；EventAction 构造时也会检查与 RunAction 一致。

事件结束的核心调用为：

```cpp
fRecords.Build(event->GetEventID(), *collections, fTrackAncestry);
auto& output = fRunAction->GetOutput();
const auto runId = fRunAction->GetRunId();
if (fRecords.IsEnabled("EventEnergyRecorder"))
  output.Write(runId, fRecords.GetEventRecords());
if (fRecords.IsEnabled("IncidentEnergyRecorder"))
  output.Write(runId, fRecords.GetIncidentRecords());
```

统计策略和文件格式分离：Build 产生记录，Write 消费记录。不在 recorder 内调用 OpenFile，也不让 SD 决定输出表。

## 9. output：ROOT 输出与运行管理

### 9.1 OutputManager 和 VOutput

[OutputManager](include/output/OutputManager.hh) 构造时按名称工厂创建后端，目前唯一注册项是 RootOutput。选择入口是 ActionInitialization::CreateRunAction() 的 outputName 参数。

[VOutput](include/output/VOutput.hh) 定义如下协议：

| 接口 | 调用者/用途 |
| --- | --- |
| GetFileExtension() | RunAction 获取后缀 |
| BookEventEnergy()/BookIncidentEnergy() | RunAction 构造时选择需要的表类型 |
| SetDetectorNames(names) | BeginOfRunAction 提供完整探测器目录 |
| GetFileNames(baseName) | 计算各类型实际输出文件名 |
| OpenFile(baseName) | 打开本次 run 输出 |
| IsOpen() | 查询状态 |
| Write(runId, vector<EventEnergyRecord>) | 写事件记录 |
| Write(runId, vector<IncidentEnergyRecord>) | 写入射记录 |
| CloseFile() | 写入并关闭 |
| Inspect(fileName) | 检查完成文件，返回 OutputSummary |

OutputManager 对后端 Inspect 的外层接口叫 **InspectFile**。OutputSummary.rows 保存各类型总行数，Count(name) 对不存在项返回 0。

两个 Write 是 C++ 重载，由向量类型选择，不通过 bool incident 判断字段，也没有自动反射结构体成员。新增记录类型需显式增加对应接口和映射；这种少量、直接的适配是当前架构的选择。

### 9.2 RunAction 的装配参数

```cpp
using DetectorNames = std::function<std::vector<G4String>()>;

RunAction(const G4String& outputDirectory,
          const std::vector<G4String>& recorders,
          const G4String& outputName = "RootOutput",
          DetectorNames detectorNames = {},
          RunLog* log = nullptr);
```

- outputDirectory：父目录；空值转为 ./results/runs。
- recorders：记录器名称列表。
- outputName：后端名称。
- detectorNames：几何已就绪后调用的目录回调；不是构造时立即查询。
- log：非拥有指针，main 保证它的生命周期。

延迟回调很重要：多线程主 RunAction 可能早于几何构建创建。不要在其构造阶段强行访问尚未完成的 LMO 注册表。

### 9.3 RootOutput 的生命周期和线程约束

[RootOutput.cc](src/output/RootOutput.cc) 使用：

```cpp
auto* am = G4AnalysisManager::Instance();
am->SetDefaultFileType("root");
am->SetNtupleMerging(G4Threading::IsMultithreadedApplication());
am->SetNtupleRowWise(false, true);
```

完整调用顺序：

```text
选择表类型
→ 设置完整探测器名称目录
→ OpenFile：统一建表并设置各表文件名
→ 事件线程 Write：填写列、AddNtupleRow
→ 所有线程 CloseFile：原生 Write、CloseFile
→ 主线程 InspectFile
```

BookEventEnergy/BookIncidentEnergy 设置选择标志；真正 CreateNtuple 在首次 OpenFile 的 BookSchemas 中完成。后续 run 复用 schema，不能在已建表后改变记录器种类或探测器目录。

探测器名称先排序，再按固定顺序建立各类型表，保证主/worker ntuple ID 对应。无事件 worker 也参与相同配置。主线程不填事件行；不共享一个普通 TFile 让所有 worker 并发写，也不额外调用外部线程文件合并。

每个输入向量先验证再写入，包括 ID、有限数值、已登记探测器、方向单位长度或零向量，以及非空 CreatorProcess。I/O 写入失败不具备数据库事务式回滚保证。

### 9.4 实际 ROOT 数据结构：schema 7

每个记录器一个文件，每个探测器一个 TTree：

```text
<prefix>_EventEnergy.root
  LMO0
    RunID             Int_t
    EventID           Int_t
    Edep/keV          Double_t
  LMO1
    ...

<prefix>_IncidentEnergy.root
  LMO0
    RunID                  Int_t
    EventID                Int_t
    TrackID                Int_t
    PDG                    Int_t
    Edep/keV               Double_t
    IncidentPosition/mm    vector<double>, [x,y,z]
    IncidentDirection      vector<double>, [x,y,z]
    CreatorProcess         string column, ROOT leaf representation Char_t/TLeafC
  LMO1
    ...
```

DetectorName 保留在内存记录中用于路由，不另写 Branch。RunID 不在 recorder 结构体中，由 RunAction 提供并在 Write 时写入。

Edep/keV 是实际分支名，斜杠是名称字符；C++ 字段仍为合法标识符 Edep。单位转换发生在 FillEventEnergy/FillIncidentEnergy：Edep/keV、IncidentPosition/mm。方向无量纲，零向量允许表示静止粒子。

CreatorProcess 是该条记录根轨迹的出生过程；初级为 Primary。字符串通过 CreateNtupleSColumn/FillNtupleSColumn 写入。向量列在 booking 时绑定线程本地缓冲区，在 AddNtupleRow 时读取该行缓冲区值，不能让缓冲区提前析构。

行顺序不保证 EventID 升序。没有沉积的 event 不产生行，不能把树行数作为模拟事件总数。应使用 run.log 的 processed_events 做模拟事例数归一化，并另行加入活度、质量、时间等研究条件。

### 9.5 输出文件名与目录

OutputMessenger 在 output/ 下，用 G4GenericMessenger 将两个命令绑定到主/串行 RunAction 保存的字符串。它负责命令入口；RunAction 负责运行开始时解释、校验和应用。

```text
/run/initialize
/cupid/output/directory ./results/runs
/cupid/output/fileName calibration
/run/beamOn 1000
```

生成：

```text
results/runs/calibration/
  calibration_EventEnergy.root
  calibration_IncidentEnergy.root
  run.log
```

默认父目录 ./results/runs，默认 fileName=auto，按进程本地时间产生 runYYYYMMDD_HHMMSS。目录重名时依次尝试 _1、_2 等，原子创建目录为整个 run 保留名称。两个记录器共享前缀和 run 目录。

自定义前缀可带末尾 .root，程序先移除再加记录器后缀。前缀必须字母/数字开头，只包含字母数字、下划线、连字符、点，不能以点结尾；路径由 directory 单独设置。

--output 设启动父目录；后续宏 directory 覆盖它。相对路径均以进程工作目录为基准。未命中探测器可以有空树；没有任何行的记录器文件可能被原生后端删除，全空运行通常只留下日志。

### 9.6 RunLog 与 ROOT 读回

[RunLog](include/output/RunLog.hh) 在 main 中创建，主 RunAction 调用 BeginRun/EndRun 切换当前文件。初始化和运行间上下文保留供后续日志写入，事件输出只写本 run。RecordMacro 保存宏内容；Check() 检查日志错误。

读取分支时不要把带 / 的名字当作普通 ROOT 表达式中的变量除法。以下 Python 可在启用 PyROOT 的终端运行，FILE 应替换为实际文件：

```python
import ROOT

f = ROOT.TFile.Open("FILE_IncidentEnergy.root")
if not f or f.IsZombie():
    raise RuntimeError("Cannot open ROOT file")
tree = f.Get("LMO0")
if not tree:
    raise RuntimeError("Missing LMO0 tree")
energy = tree.GetBranch("Edep/keV").GetListOfLeaves().At(0)
creator = tree.GetLeaf("CreatorProcess")
hist = ROOT.TH1D("edep", "LMO0;Edep (keV);Entries", 300, 0, 3000)
hist.SetDirectory(0)
for i in range(tree.GetEntries()):
    tree.GetEntry(i)
    hist.Fill(energy.GetValue())
    # creator.GetValueString() gives the text of the current row.
canvas = ROOT.TCanvas("c", "Energy spectrum", 900, 600)
hist.Draw("HIST")
canvas.SaveAs("spectrum.png")
f.Close()
```

GetValueString() 用于当前输出的字符叶；直接 str(tree.CreatorProcess) 在 PyROOT 中可能显示 LowLevelView，而不是内容。向量位置可通过 getattr(tree, "IncidentPosition/mm") 读取后逐分量访问。跨文件必须区分事件总能谱和入射根能谱，两者的 Entries 含义不同。

## 10. 宏文件和完整运行示例

下面示例按源码接口编写，执行目录为仓库根目录。本次编写手册只做静态核对，未为这些示例启动新的模拟。

### 10.1 构建

```bash
# Set these to the actual installations on your machine.
export CUPID_GEANT4_PREFIX="$HOME/geant4/install"
export CUPID_ROOT_PREFIX="$HOME/root"
source "$CUPID_ROOT_PREFIX/bin/thisroot.sh"
source "$CUPID_GEANT4_PREFIX/bin/geant4.sh"

cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$CUPID_GEANT4_PREFIX;$CUPID_ROOT_PREFIX" \
  -DCMAKE_BUILD_TYPE=Release -DCUPID_WITH_VIS=OFF
cmake --build build -j4
```

CMake 精确要求 Geant4 11.4.1、ROOT 6.36.10；即使运行 --serial，当前构建仍要求 Geant4 带 multithreaded 支持。可视化改用 CUPID_WITH_VIS=ON，依赖已安装的 UI/vis 库。既有环境通过构建不等于原生 Windows 已验证。

CMake 显式列出 .cc，不自动扫描目录；增加实现文件应加入 cupid-sim 或 cupid-output 等相关目标，也检查引用它的测试目标。默认 BUILD_TESTING 开启；Python 用于测试。

### 10.2 最小 gamma 宏

保存为自选宏路径，例如 macros/my_gamma.mac：

```text
/random/setSeeds 12345 67890
/cupid/geometry/enable LMOArray
/cupid/geometry/lmoCount 2
/cupid/generator/select gun
/run/initialize
/cupid/geometry/check 10000
/gun/particle gamma
/gun/energy 1 MeV
/gun/position -25 -350 -100 mm
/gun/direction 0 0 1
/cupid/output/fileName gamma_example
/run/beamOn 1000
```

```bash
./build/cupid-sim macros/my_gamma.mac --threads 2 --output results/runs
```

预期输出目录和文件名称取决于是否重名；查看日志中的 CUPID_RUN_DIRECTORY、CUPID_RUN_RESULT，不假定固定后缀。

### 10.3 Mo100 宏

```text
/random/setSeeds 12345 67890
/cupid/geometry/enable LMOArray
/cupid/geometry/lmoCount 1
/cupid/generator/select Mo100TwoNu
/run/initialize
/cupid/output/fileName mo100_two_nu
/run/beamOn 10000
```

ZeroNu 单独启动，将选择改为 Mo100ZeroNu、前缀改为 mo100_zero_nu。select 只允许 PreInit，不能在初始化后的两个 beamOn 之间用该命令切换模式。/gun/energy 不控制 Mo100 的能谱。

### 10.4 Th232 和已有宏

现有 [th232_two_lmo.mac](macros/th232_two_lmo.mac) 为裸点核素源例子，使用 /gun/particle ion 和 /gun/ion 参数，不调用占位的 Th232Generator。若修改位置，要按实际外壳轮廓和 LMO 高度核对距离，不能把示例位置直接推广到修改后的几何。

| 已有宏 | 用途 |
| --- | --- |
| lmo_only.mac | 两块 LMO 的最小 gamma 运行 |
| run.mac | 启用三个组件的批处理例子 |
| check_geometry.mac | 重叠检查 |
| vis.mac | 交互可视化 |
| render.mac | 渲染用途，批处理需要 --vis |
| th232_two_lmo.mac | Th232 点源运行 |
| commands.mac | 注释化命令汇总，不执行运行 |

无宏参数时程序尝试交互可视化；无可视化构建必须传批处理宏。现有可运行宏在 CMake 配置时复制到 build/macros，编辑源宏后使用源路径，或重新配置同步副本。

自定义命令状态：

| 命令 | 状态 |
| --- | --- |
| /cupid/geometry/enable、disable、lmoCount | PreInit |
| /cupid/geometry/list | PreInit、Idle |
| /cupid/geometry/check | Idle |
| /cupid/generator/select、verbose | PreInit |
| /cupid/generator/list | PreInit、Idle |
| /cupid/generator/check | Idle |
| /cupid/output/fileName、directory | Idle |

命令语法、默认值、取值限制以 [commands.mac](macros/commands.mac) 为集中参考。新增命令必须同步该文件；运行所需命令尽量写在一个自包含宏中。

## 11. analyse：离线处理接口

分析顺序固定为：

```text
原始 IncidentEnergy.root
→ ParticleTimeInformation → *_timed.root
→ PileUp                  → *_timed_pileup.root
→ Trigger                 → *_timed_pileup_trigger.root
```

每阶段生成独立文件，拒绝覆盖已有路径。它们是简化的离线模型，与主程序中的逐步时间和探测器响应不是一回事。

### 11.1 ParticleTimeInformation

[接口](analyse/ParticleTimeInformation.C)：

```cpp
void ParticleTimeInformation(
    const char* inputName, double activityBq,
    const char* outputName = "", const char* logName = "",
    unsigned int shuffleSeed = 20250828,
    unsigned int timeSeed = 12345);
```

activityBq 为母源活度，必须正且有限。logName 省略时读取输入旁的 run.log，按文件名匹配 CUPID_RUN_RESULT 的 processed_events=N。

对每个 LMO 的 n 条记录：

- 用 std::shuffle 洗牌记录顺序。
- 假设记录率为 activityBq × n/N。
- 指数分布平均间隔为 1000N/(activityBq×n) ms。
- 第一条到达时间定为 0，后续累计随机间隔。
- 新增分支 ArriveTime/ms。

**每块 LMO 分别造时间轴，原有事件内和跨探测器的时间相关性被丢弃。** 这适合指定假设下的随机符合研究，不是恢复 Geant4 真实衰变时刻；不能据此分析 prompt coincidence 或衰变链延迟符合。

### 11.2 PileUp

```cpp
void PileUp(const char* inputName, double windowMs = 300.,
            const char* outputName = "");
```

要求输入为 timed 阶段，时间已排序。若一条记录与前一条或后一条的时间差 ≤ windowMs，则该记录被剔除。连续相邻的堆积簇全部剔除。

例如时间 0、200、400 ms，窗口 300 ms 时三条均去掉。这里没有合并能量、模拟波形或重新测量峰值。名称 PileUp 表示堆积处理，具体策略是剔除。

### 11.3 Trigger

```cpp
void Trigger(const char* inputName,
             double thresholdKeV = 200., double sigmaKeV = 100.,
             unsigned int seed = 20250825,
             const char* outputName = "");
```

要求输入为 pileup 阶段。所有 LMO 使用同一阈值和噪声，接受条件：

```text
Edep + Gaussian(0, sigmaKeV) > thresholdKeV
```

噪声只影响是否保留；输出 Edep 不加上随机噪声。因此该处理并不把能谱卷积成含分辨率的测量能谱。

### 11.4 IncidentAnalysis.hh

[共享辅助文件](analyse/IncidentAnalysis.hh) 位于 cupid_analysis 命名空间：

| 函数 | 职责 |
| --- | --- |
| Require(ok,message) | 输入/读写检查，失败抛出异常 |
| Column(tree,name,type) | 按精确分支名寻找唯一标量叶，并检查类型 |
| Read(tree,row) | 检查 GetEntry 成功 |
| Keep(tree) | 检查 Fill 成功 |
| Process(input,requestedOutput,suffix,previous,stage,settings,transform) | 公共文件处理流程 |

Process 检查 AnalysisStage、选择 LMO[0-9]+ 树、验证基础列，然后 CloneTree(0) 复制结构，由 transform lambda 决定哪些记录保留。额外列随记录保留，包括位置、方向和 CreatorProcess。

写完后保存 AnalysisStage 和 AnalysisHistory，再通过同目录临时文件及硬链接发布最终文件。目标父目录应预先存在，文件系统需支持硬链接；失败会清理临时 .part。它只读取当前 Edep/keV、ArriveTime/ms 名称，没有旧分支名兼容层。

当前只处理 LMO 数字名称。以后增加 NTD、LD 或更改探测器命名，要显式扩展筛选和物理处理策略。

### 11.5 调用示例

从仓库根目录执行，替换实际输入路径和经确认的活度：

```bash
root -l -b -q 'analyse/ParticleTimeInformation.C("results/runs/example/example_IncidentEnergy.root",3.0992)'
root -l -b -q 'analyse/PileUp.C("results/runs/example/example_IncidentEnergy_timed.root",300.)'
root -l -b -q 'analyse/Trigger.C("results/runs/example/example_IncidentEnergy_timed_pileup.root",200.,100.)'
```

3.0992 Bq 仅是调用参数示例，不是框架默认活度。阶段输出中的 AnalysisHistory 保存处理参数；原始 ROOT 和 run.log 应继续保留。

## 12. 常见扩展任务

### 12.1 新增几何组件

以未来 CopperFrame 为例，以下是扩展步骤，不表示当前已有此组件：

1. 在 include/geometry、src/geometry 添加同名头文件和实现，继承 VGeometry，并实现 Build(...) const override。
2. 从 Materials 获取材料，创建 Solid、LogicalVolume、PhysicalVolume。
3. 放置完成后调用 RegisterSourceVolume(physical,parent)，母体先登记。
4. 修改 GeometryManager 的 Component 数组容量，并添加名称、初始开关和 std::make_unique<CopperFrame>()；依赖其他组件时放在它们后面。
5. 同步 GeometryMessenger 的候选值、管理器错误提示、commands.mac 和 README。
6. 将 .cc 加入 CMake 中实际依赖此几何的目标。
7. 验证尺寸、变换、材料、源查询和重叠。

仅仅创建一个 geometry 子文件不会被自动发现。对于纯被动结构，不调用 RegisterLMOVolume；对于 NTD/LD，应设计各自计分目录和 SD，避免混入 LMO 专有登记接口。

若要实现更大的 LMO 阵列，应同时处理空间布局、名称、copy number 和 count 限制。GetLMOPlacements 的直接体积采样依赖“无子体的完整盒子”，修改形状时要同步发生器。

### 12.2 新增 generator：最小组合示例

下面是可用于理解接口的扩展示例，将基态 Th232 放在固定 World 点。它没有源封装和实际活度模型，也没有注册到现有管理器。

```cpp
#pragma once

#include "generator/VGenerator.hh"
#include "generator/toolkit/NuclideEmitter.hh"
#include "generator/toolkit/sampler/PointSampler.hh"

namespace cupid {
class ExampleNuclideGenerator final : public VGenerator {
 public:
  explicit ExampleNuclideGenerator(const G4ThreeVector& position)
      : fPosition(position) {}

  void Generate(G4Event* event) override {
    fEmitter.Emit(event, 90, 232, fPosition.Sample());
  }

 private:
  PointSampler fPosition;
  NuclideEmitter fEmitter;
};
}
```

接入时需要：

1. 为物理参数规定意义、单位和默认值，选择合适目录。
2. 在 GeneratorManager 中保存发生器实例，并增加 Generate 分发分支。
3. 扩展 Check、List 和 GeneratorMessenger 的名称候选。
4. 若需要几何查询，延后到几何可用时创建采样器；将 GeometryManager 以只读引用传入。
5. 若增加源参数命令，明确 PreInit/Idle 状态、主/worker 同步方式，并同步 commands.mac。
6. 更新 CMake、README、对应源验证。

新增模式若允许 Idle 切换，必须同步处理缓存发生器重建；当前 Mo100 缓存只在首次使用创建，不能单独放开宏命令状态就认为切换已正确实现。

每个 generator 自行选择采样器，但不要复制已有采样算法。涉及新的原子核模型时，先检查对应 Geant4 版本是否已有适用实现，再决定调用现成代码、引入外部库还是本地建模，并记录近似及验证依据。

### 12.3 新增 recorder

当前采用显式、有类型的适配路线：

1. 在 include/record/<Name>.hh 定义记录结构和 Build 接口；src/record 对应实现聚合算法。
2. 先检查 SD/Hit 是否已提供所需原始量。若没有，在采集层补充，不能仅给输出增加一个没有来源的字段。
3. 扩展 RecordManager 的合法名称、存储向量、Clear、Build 和 getter。
4. 若需要祖先信息，更新 RequiresTrackAncestry；若需要时间或新的轨迹状态，明确清理和生命周期。
5. 扩展 VOutput 和 OutputManager 的 Book/Write 重载。
6. RootOutput 添加独立 booking/filling 函数、列 ID、单位换算、文件路由和数据验证。
7. RunAction 根据选择调用 Book；EventAction 按新向量类型调用 Write。
8. 更新 readback 检查、schema、手册和数据流图，并验证主/worker 表结构一致。

如果只是给已有 IncidentEnergyRecord 加字段，至少核对以下链路：

```text
采集来源 → Hit/TrackAncestry → 记录结构 → Recorder::Build
         → RootOutput::BookIncidentNtuple
         → RootOutput::FillIncidentEnergy
         → 校验、分析透传、文档
```

例如 CreatorProcess 来源是 TrackingAction，不应在后处理按 PDG 猜测。分支名包含单位时，单位标记放在输出定义中，C++ 字段不使用 / 字符。

### 12.4 新增 output 后端

1. 继承 VOutput，实现已声明的全部接口。
2. 在 OutputManager 构造函数工厂表登记名称。
3. 在 ActionInitialization::CreateRunAction() 选择名称。
4. 明确后端的文件后缀、记录文件划分、线程写入/合并和检查行为。
5. 为目录冲突、空运行、多 run、无事件 worker、非法输入和读回添加验证。

当前 RunAction 的结构主要围绕已实现的 ROOT 多线程路径，换一个后端名并不自动获得线程安全。新后端应自己实现与这套生命周期一致的并行行为；不能让多个线程无协调地写同一文本文件。

### 12.5 新增记录字段的例子：时间

若未来需要真实输运时间，应先区分：

- 初级顶点的时间；
- track 出生时间；
- 首次进入探测器的时间；
- 各次能量沉积的时间；
- 电子学测量的触发时间。

当前 IncidentEnergyRecord 只保留首次步前位置/方向及整事件归并能量。给它添加一个时间值并不能恢复一个粒子多次入射的独立脉冲。若研究时间窗内能量积分，应保留足够的沉积时间信息，再定义窗口合并规则和跨事件拼接策略。

离线 ArriveTime/ms 属于人为采样的到达序列，不能替代上述输运时间。修改时间统计时应同步检查 ParticleTimeInformation 的用途是否仍成立。

## 13. 验证、复现与协作

### 13.1 不同检查回答不同问题

| 类别 | 回答的问题 | 当前代码入口示例 |
| --- | --- | --- |
| 编译/链接 | 接口、依赖是否能构建 | CMake、编译器 |
| 几何检查 | 抽样能否发现重叠、尺寸/注册是否满足约束 | check_geometry.mac、check_framework.py |
| 材料检查 | 名称、配方等软件约定是否一致 | check_materials.cc |
| 采样检查 | 范围、变换、均匀性、子体排除是否正确 | check_vertex_sampler.cc、check_direct_samplers.cc |
| 记录器回归 | 能量归并、零值过滤、祖先规则是否保持 | check_energy_records.cc、check_lmo_sd.cc |
| 输出回归 | 列类型、单位、线程合并、目录/文件生命周期是否正确 | check_root_output.cc、check_native_analysis.cc、check_output_integration.py |
| 运行结果读回 | 两类记录的总能是否一致 | check_root_records.py |
| 物理相关检查 | 指定过程执行、简单谱/行为是否符合测试预期 | check_physics.py、check_th232_source.cc |
| 独立物理验证 | 源模型和输运是否满足研究精度 | 需指定参考测量/理论、统计量及容差 |

不能把所有文件名含 physics 的脚本等同于完整实验物理验证。特别是 Mo100 谱形、角分布、效率、衰变链和材料污染，需要独立参考及不确定度分析。

### 13.2 典型针对性检查命令

先加载 Geant4/ROOT 环境，在仓库根目录执行。例如修改了记录器和输出：

```bash
cmake --build build -j4
mkdir -p results/tests/record-output-check

./build/check-energy-records
./build/check-lmo-sd serial 2
./build/check-lmo-sd mt 2
./build/check-root-output results/tests/record-output-check
./build/check-native-analysis results/tests/record-output-check both
```

对一次实际模拟的两个文件读回检查：

```bash
python3 tests/check_root_records.py \
  results/runs/example/example_EventEnergy.root \
  results/runs/example/example_IncidentEnergy.root \
  --events 1000 --detectors LMO0 LMO1
```

替换为该次实际处理事件数与探测器列表。两类记录在同一个 event/探测器下能量和一致，是归并完整性的检查，不证明物理能谱正确。

部分 CTest 条目仍将结果写在 build 下，不应未经检查直接运行整套 ctest。按项目约定，将助手测试输出显式放到 results/tests/<task>。用户要求的测试、分析和绘图保留脚本及结果；助手主动自检通过后报告结果，再仅清理本次自检文件。失败证据保留用于排查。

### 13.3 运行复现

正式结果至少同时保存：

- commit 和有无未提交修改；
- 编译配置、Geant4/ROOT 与数据集版本；
- 实际宏、源模型、几何和材料参数；
- 随机数引擎和种子/状态、线程数、事件数；
- 执行命令、run.log、ROOT 文件；
- 离线分析的输入文件、参数、脚本版本和随机种子。

程序日志已记录命令、工作目录、宏文本、版本、运行随机状态、输出路径和事件数等信息；不要认为它自动收集了全部 Git 状态、数据集校验和和源码参数。需要额外保存的内容可集中写入同一个运行日志，不必另造大量描述文件。

固定种子不保证跨 Geant4 版本、随机调用顺序、不同构建或线程设置逐事件一致。比较物理结果时应说明要求统计一致还是逐事件重复，并制定相应检验方法。

### 13.4 团队开发约定

- 一般开发使用功能分支和 Pull Request，按 README 约定审核后合并 main；维护者明确要求的直接 main 操作另行遵循。
- 项目头文件在前，G4 头文件其次，ROOT/标准库等其他头文件最后，分组留空行。
- 私有成员用 f 加大写名称；遵循所在类已有字段命名，记录结构的公开字段当前为大写开头。
- 注释用英文，解释用途、参数单位、默认值、调用位置、所有权与算法假设。
- 修改历史写 history.log，不在代码处加入修改日期或保留大段已弃用实现。
- 新增宏命令同时更新 commands.mac；变更输出 schema 同步更新字段映射、分析和读回检查。
- 源码、构建配置、小型宏、测试、文档进入 Git；build、results 和大规模 ROOT 输出不进入 Git。
- AGENTS.md 按当前用户约定仅本地保留，由 .gitignore 排除，不提交到 GitHub。

协作时可以按 geometry、generator、record、output 分工，但跨模块数据结构的修改应提前对齐。接口审查应明确谁生产数据、单位是什么、何时清空、何时写入，以及线程局部状态在哪里。

## 14. 当前限制与维护注意事项

以下事项来自本次源码审阅，不是已完成的新功能：

1. 几何包含原型 cryostat、inner shielding、1–2 块 LMO 和可选 WThWireInCopperCup 刻度源；实验大厅、岩石、完整铜支架、NTD/LD 计分尚未接入。
2. Th232/Co56 专用类仍占位，当前通用粒子枪不代表真实源封装、强度或活度模型。
3. Mo100 使用本地简化模型；两个模式当前共享角因子，需要进一步物理审核。材料仍是天然 Li2MoO4，不会随发生器模式变为富集材料。
4. 事件和入射根记录没有真实时间列；一个事件内归并所有相关能量，不表示量热器能把所有时间尺度的沉积同时测量。
5. IncidentEnergyRecorder 按出生体积/访问关系归并，不区分同轨迹重复入射段；后续时序研究需要新的数据设计。
6. PileUp 是簇剔除，Trigger 仅做接受筛选；离线随机时间会破坏原始事件相关性。
7. ROOT 树标题当前为 schema 7，但 RunAction 的 CUPID_OUTPUT_MODULE 日志行仍硬编码 schema=4。检查现有文件应以实际树标题和分支为准；该日志标记待后续修正。
8. README 中个别旧说明仍使用小写 detectorName/edep 描述内存字段；实际结构体为 DetectorName/Edep，以头文件和本手册接口表为准。
9. RootOutput::InspectFile 保留历史文件布局识别，但 analyse 宏只支持当前分支名，二者兼容范围不同。
10. 当前输出不包含统计权重或绝对活度归一化。软件检查通过不等于本底率、效率或谱形已获实验验证。

维护本手册时优先核对实际头文件、实现、Messenger 和测试。若接口、默认参数或统计语义改变，应在同一修改中更新相应章节，而不是只添加新的说明却保留相矛盾的旧描述。
