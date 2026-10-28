# CUPID-CJPL 初级 Geant4 framework

v0.1：通过 `.mac` 选择几何组件，显示装置、检查重叠，运行测试粒子输运。

这是独立的新工程。参考工程只用于理解尺寸和装置关系，没有作为构建依赖。
当前仅测试粒子枪可运行；Th-232/Co-56 专属发生器保留接口占位，尚未确定源模型。
公共组件提供固定点、直接体积、直接表面、体积拒绝四种采样器及核素发射工具，供后续具体发生器自行调用。
当前支持晶体能量 ROOT 输出；岩石、大厅、电子学和波形尚未实现。
`EventAction` 通过 `RecordManager` 按名称选择记录类，当前默认开启两种统计；`TrackingAction` 为逐入射统计登记当前事件的轨迹关系。
`RunAction` 管理各线程的文件生命周期，由 Geant4 原生分析管理器合并结果；`SteppingAction` 保留空接口，`StackingAction` 维持默认分类。
`StackingAction::ClassifyNewTrack()` 返回 `fUrgent`，所有轨迹留在当前事件中继续处理。
`LMOSensitiveDetector` 与 Action 类位于同一层，提供按探测器和 TrackID 区分的基础沉积。
`record/` 实现逐事件、逐入射粒子两种统计；EventAction 通过 `OutputManager` 将原始记录向量交给 `RootOutput`，通过 Geant4 原生 ntuple 合并写入同一个 run 文件。

## 1. 在 WSL Ubuntu 中构建

要求 Geant4 **11.4.1**（启用多线程）、ROOT **6.36.10**、C++17、CMake >= 3.16。
默认构建可视化版本，链接 ROOT Core/RIO/Tree。输出集成测试同时调用 PyROOT 回读检查；
实际结果检查工具 `tests/check_root_records.py` 使用 ROOT 安装自带的 PyROOT。
本机 Vdt 开发依赖位置与重建配置见后文 ROOT 输出模块部分。

在 **WSL 的终端**中进入本工程目录：

```bash
source /home/loyh/geant4/install/bin/geant4.sh
source /home/loyh/root/bin/thisroot.sh
cmake -S . -B build \
  -DGeant4_DIR=/home/loyh/geant4/install/lib/cmake/Geant4 \
  -DROOT_DIR=/home/loyh/root/cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

`Geant4_DIR` 是当前机器的示例路径，不写死在 CMake 中。
如果没有图形环境，可在配置时加 `-DCUPID_WITH_VIS=OFF`。
无图形版本仍支持批处理及全部几何检查。

## 2. 运行

以下命令从工程根目录执行；可执行程序也会查找 build/macros 下的示例宏。

```bash
# 批处理；默认两条工作线程
./build/cupid-sim run.mac --threads 2

# 真正的串行运行管理器
./build/cupid-sim run.mac --serial

# 只构建两个晶体并检查几何
./build/cupid-sim lmo_only.mac --threads 2

# 全部组件的重叠检查
./build/cupid-sim check_geometry.mac --serial

# 交互式几何显示，需要 WSLg/X11 图形环境
./build/cupid-sim

# 无窗口渲染几何图，写出当前目录下的 geometry.png
./build/cupid-sim render.mac --serial --vis

# 自动验证，详细日志写到 build/test-results
ctest --test-dir build --output-on-failure
```

宏中设置 `/run/verbose 1` 后，成功运行应看到 Geant4 自带摘要，例如：

```text
Run Summary
  Number of events processed : 1000
```

这表示完成了粒子输运，不代表通过物理验证或已经得到能谱。
几何重叠检查成功时会输出 `CUPID_GEOMETRY_CHECK ... result=PASS`。
抽样重叠检查不能严格证明所有微小重叠都不存在。

## 3. 用宏文件选择组件

默认所有组件关闭。世界始终存在；没有启用任何组件时会明确提示 world-only。
一个完整的自定义宏可以写为：

```text
/random/setSeeds 12345 67890
/run/verbose 1
/cupid/geometry/enable Cryostat
/cupid/geometry/enable InnerShielding
/cupid/geometry/enable LMOArray
/cupid/geometry/lmoCount 1
/cupid/geometry/list
/run/initialize
/cupid/geometry/check 10000
/gun/particle gamma
/gun/energy 1 MeV
/gun/position 0 -350 -100 mm
/gun/direction 0 0 1
/run/beamOn 1000
```

命令约定：

| 命令 | 用途 |
| --- | --- |
| `/cupid/geometry/enable NAME` | 启用一个组件，重复启用不会重复构建 |
| `/cupid/geometry/disable NAME` | 关闭一个组件 |
| `/cupid/geometry/list` | 列出当前组件状态和晶体数量 |
| `/cupid/geometry/lmoCount 1` | 一颗居中晶体；也可以设为 2，沿 X 排列 |
| `/cupid/geometry/check 10000` | 初始化后对所有放置体积执行重叠检查 |

组件名称区分大小写：`Cryostat`、`InnerShielding`、`LMOArray`。
启用、关闭及晶体数量只允许在 `/run/initialize` 之前设置。
`list` 在初始化前后均可用。`check` 只能在初始化完成后、运行之外执行。
输入未知组件、非法参数、错误命令或检测出重叠时，批处理返回非零退出码。
切换组件组合请启动新进程；第一版不支持运行时重建几何。

`--threads N` 设置工作线程数量（1–256），`--serial` 选择串行模式；两者互斥。
默认批处理不启动可视化管理器。需要在自定义宏中使用 `/vis/` 时加 `--vis`。
自定义宏的目录、当前目录和编译目录中的 macros 都加入宏搜索路径。

示例 `macros/geometry.mac` 是共享的装置配置。修改示例宏后重新执行 CMake 配置，
或直接把源文件路径传给程序，例如 `./build/cupid-sim macros/run.mac`。

## 4. 文件职责

```text
main.cc                         程序入口、运行模式、物理列表注册
include/ + src/                 头文件和实现一一对应
  DetectorConstruction          世界创建、整体构造和灵敏探测器挂载
  LMOSensitiveDetector      接收晶体 step，生成事件内的基础命中集合
  ActionInitialization          创建线程局部的用户 actions
  PrimaryGeneratorAction        调用 GeneratorManager，统一传播源错误
  PhysicsList                   逐项注册物理构造器和设置参数
  RunAction                     持有 OutputManager，管理各线程文件生命周期，使用 G4 原生合并
  EventAction                   调用 RecordManager，以强类型将记录交给 OutputManager
  SteppingAction                每步处理接口，暂为空
  TrackingAction                首次跟踪时登记 TrackID、ParentID 和出生体积
  StackingAction                新轨迹分类、新阶段、事件准备接口
  generator/                    场景调度入口、公共组件及专属源占位目录
  DetectorHit                   每个探测器的逐轨迹基础沉积集合
  record/
    RecordManager               按名称选择记录器，保存本事件各类型结果
    EventEnergyRecorder         每事件、每探测器的总沉积及 EventEnergyRecord 结构体
    IncidentEnergyRecorder      每入射根的总沉积及 IncidentEnergyRecord 结构体
    TrackAncestry               本事件全部已跟踪粒子的父子关系和出生体积
  geometry/
    GeometryManager             组件开关、装配、源体积登记与查询、重叠检查
    SourceVolumeInfo.hh         物理放置指针及局部到世界坐标变换
    GeometryMessenger           /cupid/geometry/ 命令
    GeometryParameters.hh       主要尺寸、坐标和晶体数量
    Cryostat                    冷板、铜罩和铝罩
    InnerShielding              三段内铜屏蔽
    LMOArray                一颗或两颗 LMO
  output/
    VOutput                     文件生命周期、强类型 Write 重载及合并检查接口
    OutputManager               按名称创建一个输出模块，转发数据及文件操作
    RootOutput                  ROOT 分支映射、单位转换和文件合并
    RunLog                      作业日志与终端输出同步
  materials/Materials           统一材料定义
macros/                         几何选择、显示、检查和运行示例
tests/check_framework.py        小规模端到端检查
tests/check_materials.cc        材料配方与查询一致性检查
tests/check_physics.py          过程注册、衰变与输运冒烟检查
```

## 5. 几何事实与暂定假设

输入尺寸参考：`/home/loyh/simulation/1` 中的几何定义。
这些参数尚未与机械图纸或实物逐项确认，模型标识为 **prototype-v0.1**。

- 沿用参考模型的右手坐标系：制冷机公共轴沿 Y，MXC 主板底面为 Y=0。
- 世界为半边长 2 m 的 `G4_Galactic` 真空盒子。这是输运测试环境，**不是大厅或真实外部空气模型**。
- 组件都直接放在该真空母体积中，因此允许独立查看和测试。
- 六级冷板的高度、半径、厚度和参考材料集中在 `GeometryParameters.hh`。
  50K/300K 板采用参考代码中的不锈钢定义，尚未独立核实。
- MXC 暂用直径 500 mm、厚 8 mm 的圆板；未重建其凸台、盲孔和支柱。
- HEX 等上部冷板保留三处直径 17 mm 的贯通孔。
- 铜罩的主要壁厚、底板及法兰轮廓来自参考尺寸；轮廓表保存在 `Cryostat.cc`。
- 铝罩下部用参考轮廓的一组较稀疏节点进行分段线性近似，保留抬高的中央底面。
  此近似和 MXC 简化均需要在正式刻度/本底计算前评估。
- 内铜屏蔽外半径 160 mm，主腔半径 60 mm，顶部孔半径 15 mm。
- LMO 采用自然元素组成 Li2MoO4，密度 3.07 g/cm³，边长 45 mm。
  单晶体中心为 `(0,-350,0) mm`。双晶体中心为 `(-25,-350,0)` 和 `(25,-350,0) mm`；
  **双晶体排列是测试布局**。当前 copy number 0、1 只在这个单一阵列内作为晶体编号。
  每颗晶体具有独立的 LogicalVolume，名称为 `LMO0`、`LMO1`，数字与 copy number 一致。
  它们共用相同尺寸的 Solid 和 `Li2MoO4` 材料；每颗晶体的 PhysicalVolume 与 LogicalVolume 同名，均为 `LMO` 加编号。
- 本版没有支撑框架、外屏蔽、源杯或钨钍丝，也没有由参考 STL 隐式加载任何结构。

### 材料库

`src/materials/Materials.cc` 按“元素 → 自定义材料 → 标准材料”组织。
元素 H、C、N、O、F、Cu、Pb、Ge、Li、Mo、Fe 在开头逐项定义，
使用 Geant4 11.4.1 NIST 的天然同位素组成；没有使用手填的近似原子量，也没有富集元素。
材料分别明确写出密度、状态和配比，使用 `AddElementByNumberOfAtoms` 或
`AddElementByMassFraction` 区分原子数比与质量分数。

以下 8 种材料全部在初始化时构建，即使当前几何没有使用它们：

| 查询名称 | 密度（g/cm³） | 组成及说明 |
| --- | --- | --- |
| `Vacuum` | `CLHEP::universe_mean_density`，按 Geant4 单位使用 | 氢气近似；2.73 K、3×10⁻¹⁸ Pa |
| `Air` | 0.00129 | N 70%、O 30%，质量分数；简化空气 |
| `Copper` | 8.96 | 天然铜 |
| `Lead` | 11.35 | 天然铅 |
| `Steel` | 7.874 | Fe 96%、C 4%，质量分数；参考配方，不能视为真实不锈钢牌号 |
| `PTFE` | 2.2 | C₂F₄，原子数比 |
| `Li2MoO4` | 3.07 | 天然 Li₂MoO₄，固态，0.01 K |
| `NTD_Ge` | 5.323 | 纯天然锗近似；未定义掺杂 |

配方参考 [SimulationTool 的 MaterialLibrary.cc](https://github.com/Loyh617/SimulationTool/blob/main/src/materials/MaterialLibrary.cc)，
读取时文件 blob SHA 为 `6312b7f245280b2972246dedc498c2ddbb6dbfe1`。
保留其材料密度、配比和显式温度/压力；元素改用 NIST 天然组成，包括真空中的氢，
因此不声称与旧库中手填原子量的材料逐位相同。
这些配方是当前参考输入，尚未逐项与实际装置的材料检测或采购规格确认。

原几何的 `G4_Galactic`、`G4_Cu`、`G4_Al`、`G4_STAINLESS-STEEL` 继续单独构建，
保留 Geant4 标准定义。`Steel` 不会替换冷板的不锈钢，`Air` 也不会自动替换真空世界。
晶体直接使用材料名 `Li2MoO4`：密度和天然元素组成不变，
温度从原先 Geant4 默认值改为参考库的 10 mK。设置温度本身不计算热收缩或探测器响应。

`Materials::Build()` 在主线程几何初始化阶段调用，重复调用复用已有材料；
材料由 Geant4 管理，几何组件不自行删除。`Get()` 只接受材料的实际名称，未知名称抛出异常。
新增材料只需在元素段补充需要的元素，再添加一个独立材料段；调用时直接使用构造该材料时指定的名称。
`tests/check_materials.cc` 检查材料是否齐全、密度/配比/温度、实际名称查询、旧别名拒绝及重复初始化。

## 6. 当前物理与事件范围

`PhysicsList` 直接继承 `G4VModularPhysicsList`，不继承完整的 `Shielding`。
以下 Geant4 11.4.1 构造器在 `src/PhysicsList.cc` 中逐项包含和注册：

| 构造器 | 用途 |
| --- | --- |
| `G4EmLivermorePhysics` | 低能电磁与带电粒子输运 |
| `G4EmExtraPhysics` | 默认开启的光核、电子/正电子核和 μ 子核反应 |
| `G4DecayPhysics` | 普通不稳定粒子衰变 |
| `G4RadioactiveDecayPhysics` | 放射性核素衰变 |
| `G4HadronElasticPhysicsHP` | 强子弹性及低能中子 HP 模型 |
| `G4HadronPhysicsShielding` | 强子非弹性及中子俘获、裂变；这是单个构造器 |
| `G4StoppingPhysics` | 停止粒子核俘获等过程 |
| `G4IonElasticPhysics` | 离子弹性核反应 |
| `G4IonQMDPhysics` | 离子非弹性核反应 |

显式开启荧光、Auger 电子、原子退激忽略产生阈值和中子诱发裂变碎片。
暂定 γ/e-/e+ 产生长度阈值为 0.1 mm，proton 为 0 mm；这些不是步长或跟踪截断值。
放射性衰变时间限制为 `1e60 year`，以允许长寿命核素及子核继续衰变，
不改变核素真实半衰期，也不是探测器积分时间窗。
这组参数尚需 cut 收敛、刻度数据和中子/μ 子基准验证。

运行环境须提供 11.4.1 对应的数据，包括 G4LEDATA、G4RADIOACTIVEDATA、
G4LEVELGAMMADATA 和 HP 中子所需的 G4NEUTRONHPDATA；请使用本机 Geant4 环境脚本。
普通 HP 配置未额外启用材料相关的热中子束缚散射模型。

默认 `gun` 模式仍支持 `/gun/particle ion` 和 `/gun/ion Z A 0 0`。
Th-232/Co-56 专属发生器尚未实现。现有物理冒烟测试使用粒子枪发射初始核素，
再由物理列表处理其衰变和输运；这不代表已经实现实验刻度源模型。
暂不提供单次衰变截断、源平衡、活度归一化、衰变时间整理或 ROOT 计分。
整链沉积总和不能直接当作单次探测器脉冲能量。
宇宙线通量/角分布和双贝塔衰变末态发生器也尚未实现。
本版不包含光学光子、声子、电荷收集或电子学响应。

`tests/check_physics.py` 通过过程列表及真实跟踪日志检查注册、μ 子/中子输运、
Th-232 链到 Pb-208，以及双工作线程的 Co-56 衰变。这些是软件冒烟检查，
不是分支比、能谱、中子产额或信号效率的物理验证。

多线程采用 Geant4 事件并行；几何在初始化后共享，粒子枪和用户 actions 在线程内分别创建。
几何宏命令只由主线程处理，不广播到工作线程。
`ActionInitialization::BuildForMaster()` 只注册 `RunAction`；
`Build()` 为串行执行或每个工作线程分别注册粒子枪与上述五个 actions。
当前接口不累计能量、不筛选轨迹、不保存记录；`StackingAction` 也不推迟轨迹到后续事件。
未实现的 generator 场景目录用 `.gitkeep` 保留；`record/` 提供当前的两种能量统计及溯源工具。

## 7. 添加组件

1. 在 `include/geometry/` 和 `src/geometry/` 添加对应 `.hh/.cc`。
2. 按现有组件提供 `Build(parent, materials, parameters, geometry)`；`parent` 是母物理体，
   放置时使用 `parent->GetLogicalVolume()`。创建源体积后调用
   `geometry.RegisterSourceVolume(physical, parent)`，嵌套放置必须先登记母体。
3. 在 `GeometryManager` 的组件表中登记名称和构造入口，并更新表的大小及命令帮助。
4. 在顶层 `CMakeLists.txt` 显式加入源文件。
5. 增加一个宏算例和几何组合检查。

若新组件依赖特定母体积，应先在管理器中检查依赖并传递正确母体积，
不能仅因为有开关就随意放入世界。新增材料集中在 `Materials` 定义。

## 8. 小团队协作

几何成员主要修改 `geometry/`；源成员主要修改 `generator/` 中各自的场景目录；
运行与后续记录成员修改 actions。新增公共命令、坐标或晶体编号需在 README 中同步说明。
采用个人分支及代码审查；提交前运行上述 CTest 检查。
类的私有/受保护成员采用 `fGenerator`、`fGeometry` 等命名；局部变量和参数使用普通小写开头名称。
纯数据参数结构保留 `lmoCount` 等字段名，不为公开参数字段额外添加前缀。
`build/`、输出文件和机器本地路径不提交 Git。

## 9. 已核对的官方接口资料

- [Geant4 11.4.1 B1](https://github.com/Geant4/geant4/tree/v11.4.1/examples/basic/B1)：常规工程及 action 初始化结构。
- [11.4.1 G4UIcommand](https://github.com/Geant4/geant4/blob/v11.4.1/source/intercoms/include/G4UIcommand.hh)：命令可用状态、广播及失败返回。
- [11.4.1 G4RunManagerFactory](https://github.com/Geant4/geant4/blob/v11.4.1/source/run/include/G4RunManagerFactory.hh)：串行与多线程管理器选择。

工程组织参考 [SimulationTool](https://github.com/Loyh617/SimulationTool) 的 `include/src` 对应结构；
新工程不依赖或修改该仓库。

## 10. 本次验证记录（2026-10-17）

实际验证环境：WSL Ubuntu 24.04、GCC 13.3.0、CMake 3.28.3、
Geant4 11.4.1 Release / MT / C++17。

- 开启可视化的版本与 `CUPID_WITH_VIS=OFF` 版本均编译成功。
- 21 项自动检查通过：八种组件组合、双晶体、开关关闭、非法名称/数量/命令、
  初始化后修改拒绝、嵌套宏错误传播、重复运行、串行/双线程输运、
  晶体内部真实输运 step、缺失宏、无窗口 PNG 渲染。
- 全部组件的 20 个物理放置体积通过每体积 20,000 次抽样重叠检查。
- `run.mac` 在串行和四工作线程下分别完成 1,000 个事件。
- `lmo_only.mac` 在双工作线程下完成 1,000 个事件。
- 关闭可视化的版本在双工作线程下完成 1,000 个事件。
- 已查看 `build/test-results/geometry.png`，确认完整装置可见。
- 交互式 WSLg/OpenGL 窗口未在本次自动验证中启动；无窗口显示使用 `TSG_OFFSCREEN`。

日志在本地 `build/test-results/` 和 `build/Testing/Temporary/LastTest.log`。
源码压缩包不包含编译产物和日志；可按上述命令重新生成。
这些是软件与几何检查结果，不代表已完成刻度谱、本底率或实验几何的物理验证。

### 材料库更新验证（2026-10-18）

- 已在上述 WSL / Geant4 11.4.1 环境重新编译可视化版本。
- CTest 的 `material-checks` 和 `framework-checks` 均通过。
- 新增材料检查覆盖 8 种参考材料的密度、配比、同位素条目、显式温度/压力、
  材料名称查询、未知名称拒绝，以及重复初始化不新增材料/元素。
- 原有 21 项端到端检查全部通过，包括串行/双线程输运和无窗口渲染。

### PhysicsList 接入验证（2026-10-18）

- 在本地工程 `/home/loyh/simulation/CUPID-CJPL` 编译成功。
- CTest 的材料、框架和物理接入三组检查全部通过。
- 原有 21 项框架检查通过，包括串行/双线程输运与无窗口渲染。
- 新增 3 项物理冒烟检查通过：实际过程列表和 μ 子/中子输运、
  单事件 Th-232 子核链到 Pb-208、双工作线程下 4 个 Co-56 衰变事件。
- 日志：`build/physics-test-results/`。本记录不代表已完成物理精度验证。


### Action 接口占位验证（2026-10-21）

- 已核对本机 Geant4 11.4.1 的五种用户 Action 头文件接口并编译成功。
- `RunAction`、`EventAction`、`SteppingAction`、`TrackingAction` 的回调暂为空。
- `StackingAction` 默认返回 `fUrgent`，其余回调为空。
- CTest 三组检查全部通过：材料、21 项框架检查、3 项物理冒烟检查。
- 事件完成数检查使用 `/run/verbose 1` 的 Geant4 原生摘要，包含串行、双线程及重复运行。
- `include/` 和 `src/` 下的 `generator/`、`record/` 均已建立且为空。


## 11. 分场景 generator 与独立采样器

`PrimaryGeneratorAction` 调用线程内的 `GeneratorManager`。当前只有 `gun` 可运行，
使用原有 `/gun/` 命令设置粒子、位置、能量和方向。
Th-232、Co-56、2νββ 和 0νββ 的类仅为 `VGenerator` 接口占位，未注册为可选源。
直接调用这些占位类的 `Generate()` 会报告未实现，而不是产生空事件。

```text
include/generator/                  src/generator/ 有对应实现
  VGenerator.hh                     场景发生器的统一事件生成接口
  GeneratorManager.hh               发生器调度入口，当前仅测试粒子枪
  GeneratorMessenger.hh             选择、诊断、列出和检查命令
  toolkit/NuclideEmitter.hh               静止基态核素初级粒子工具
  toolkit/sampler/
    PointSampler.hh                 固定点采样
    DirectVolumeSampler.hh          简单实体内均匀采样
    DirectSurfaceSampler.hh         简单表面上均匀采样
    RejectionVolumeSampler.hh        实际几何内均匀拒绝采样
  calibration/th232/Th232Generator.hh   接口占位
  calibration/co56/Co56Generator.hh     接口占位
  background/environment/           预留
  background/copper_frame/          预留
  background/cosmic/                预留
  lmo_signal/
    lmo_radioactivity/         预留
    double_beta_decay/
      TwoNuDoubleBetaDecayGenerator.hh   接口占位
      ZeroNuDoubleBetaDecayGenerator.hh  接口占位
```

### 发生器选择与命令

`ActionInitialization` 只保存发生器名称和诊断输出级别两个成员，主线程在 PreInit
阶段设置，工作线程运行期间只读。没有通用的源配置结构，也不集中决定采样算法。

| 命令 | 当前用途 |
| --- | --- |
| `/cupid/generator/select gun` | 选择当前唯一已实现的发生器；初始化前设置 |
| `/cupid/generator/verbose 0或1` | 1 输出逐事件初级粒子诊断；初始化前设置 |
| `/cupid/generator/list` | 列出当前发生器；初始化前后可用 |
| `/cupid/generator/check` | 初始化后检查发生器名称是否受支持，不检查粒子枪位置或物理模型 |

通用 `/cupid/generator/position`、`/cupid/generator/point`、
`/cupid/generator/volume` 命令已移除，旧宏使用它们会明确报错。
两份刻度源示例宏也已移除；重新构建不会再复制这些宏。
测试粒子枪的 `/gun/position` 等命令保持可用。

### 由具体发生器选择采样方式

四个采样器没有共同基类，也没有按模式创建采样器的工厂。固定点和拒绝采样使用
`Sample()`；直接采样使用 `SampleBox()` 等明确形状的方法。后续每种具体发生器
自行决定用哪种算法，并直接传入位置、尺寸或体积名称。

下面的 `geometry` 是已完成构造和登记的 `const GeometryManager&`，
`LMO1` 必须已启用并登记。示例仅展示接口，不为任何刻度源预设模型。

```cpp
#include "generator/toolkit/sampler/PointSampler.hh"
#include "generator/toolkit/sampler/RejectionVolumeSampler.hh"

#include "G4SystemOfUnits.hh"

cupid::PointSampler pointSampler({0., -350.*mm, -100.*mm}, geometry);
const auto fixedPosition = pointSampler.Sample();

const G4String volume = "LMO1";
cupid::RejectionVolumeSampler volumeSampler(volume, geometry);
const auto sampledPosition = volumeSampler.Sample();
```

具体发生器可以把选定采样器作为成员，在几何准备好后构造，随后逐事件调用。
`PointSampler` 检查输入坐标有限且严格位于 World 内部，每次返回相同世界坐标，
不创建导航器、不消耗随机数，也不要求位置一定在晶体内部。

`RejectionVolumeSampler` 通过显式 `volume` 名称查询已登记的物理体；名称无默认值。
它在 Solid 的局部包围盒中拒绝采样，转换到世界坐标，并用线程独立的导航器
排除子体积。目标名称不存在、未登记或对应关闭的组件时会报错。
最多尝试 100000 次；薄壳、低填充率或没有剩余可采样区域的体积可能耗尽次数。
实际 Geant4 物理体表面的通用采样、污染深度和多体积权重尚未实现。

四个类都不读取发生器配置或几何尺寸参数来选择算法、坐标或目标名称。
`GeometryParameters.hh` 继续负责装置布局，不决定放射源位置。

### 直接体积与表面采样

两个直接采样器均接收中心坐标和局部到世界的旋转：

```cpp
DirectVolumeSampler(center = {}, localToWorldRotation = {});
DirectSurfaceSampler(center = {}, localToWorldRotation = {});
```

默认中心为原点，默认旋转为单位矩阵。旋转方向与几何登记表中的
`localToWorldRotation` 一致；结果为 `center + rotation * localPosition`。
中心必须有限，旋转必须有限、正交且保持右手性；不接受缩放或反射矩阵。
尺寸使用 Geant4 内部长度单位，必须是有限正数，建议显式使用 `mm` 等单位。

| 类 | 方法 | 采样区域及参数 |
| --- | --- | --- |
| `DirectVolumeSampler` | `SampleBox(halfLengths)` | 局部三轴上以原点为中心的长方体；三个参数分量都是半边长，三者相等即立方体 |
| `DirectVolumeSampler` | `SampleCylinder(radius, halfLength)` | 局部 Z 轴上的实心圆柱；第二个参数是半高 |
| `DirectVolumeSampler` | `SampleSphere(radius)` | 半径范围 0 到 radius 的完整球体内部 |
| `DirectSurfaceSampler` | `SampleRectangle(halfX, halfY)` | 局部 XY 平面上以原点为中心的矩形；参数是半边长，相等即正方形 |
| `DirectSurfaceSampler` | `SampleSphere(radius)` | 完整球面 |
| `DirectSurfaceSampler` | `SampleHemisphere(radius)` | 局部 z >= 0 的半球弯曲表面，不包含底部圆盘 |

示例：

```cpp
#include "generator/toolkit/sampler/DirectVolumeSampler.hh"
#include "generator/toolkit/sampler/DirectSurfaceSampler.hh"

#include "G4SystemOfUnits.hh"

G4RotationMatrix rotation;
rotation.rotateY(90.*deg);
const G4ThreeVector center{0., -350.*mm, 0.};
cupid::DirectVolumeSampler volume(center, rotation);
cupid::DirectSurfaceSampler surface(center, rotation);

// A 45 mm cube; a cylinder of radius 10 mm and full height 40 mm.
const auto inCube = volume.SampleBox({22.5*mm, 22.5*mm, 22.5*mm});
const auto inCylinder = volume.SampleCylinder(10.*mm, 20.*mm);
const auto inBall = volume.SampleSphere(10.*mm);

// A 20 mm by 40 mm rectangle in the rotated local XY plane.
const auto onRectangle = surface.SampleRectangle(10.*mm, 20.*mm);
const auto onSphere = surface.SampleSphere(10.*mm);
// The local +Z hemisphere points along world +X after this rotation.
const auto onHemisphere = surface.SampleHemisphere(10.*mm);
```

体积均匀与面积均匀使用不同的概率分布。圆柱横截面取 `r = R*sqrt(u)`；
球体取 `r = R*cbrt(u)`，方向上取均匀 `cos(theta)` 和方位角。
球面固定半径，`cos(theta)` 均匀分布于 [-1,1]；半球面为 [0,1]。
矩形和长方体各坐标独立均匀采样。所有随机数来自当前 Geant4 随机引擎，
不在采样器内设置种子；运行仍由 `/random/` 命令管理。
接口核对依据为本机 Geant4 11.4.1 源码，以及官方版本的
[Randomize.hh](https://github.com/Geant4/geant4/blob/v11.4.1/source/global/HEPRandom/include/Randomize.hh) 和
[G4PhysicalConstants.hh](https://github.com/Geant4/geant4/blob/v11.4.1/source/global/management/include/G4PhysicalConstants.hh)。

直接采样器只描述数学区域，不要求 World 或物理体已构建，也不自动检查 World 边界、
排除孔洞/子体积或匹配材料。若要求样本严格属于已登记物理体的自身区域，
使用 `RejectionVolumeSampler`。球面和矩形样本位于数学表面上，不自动偏移到边界某一侧；
发射方向和必要的边界处理由后续具体发生器决定。

### 几何与源位置的接口

`DetectorConstruction` 创建 World 后调用 `GeometryManager::Build(world, materials)`。
管理器执行 `BeginSourceVolumes(world)`、各组件构造和 `FinishSourceVolumes()`。
World 自动登记；Cryostat、InnerShielding 和 LMOArray 在创建物理体时显式登记：

```cpp
auto* physical = new G4PVPlacement(nullptr, position, logical, name,
                                 parent->GetLogicalVolume(), false, id);
geometry.RegisterSourceVolume(physical, parent);
```

`parent` 必须是已登记的实际母物理体，母体应先于子体登记。
`FindSourceVolume(name)` 返回物理体指针和局部到世界坐标的旋转、平移；这些指针
不拥有几何。嵌套变换为 `R_world = R_parent * R_local`，
`t_world = t_parent + R_parent * t_local`。

结束登记时一次性检查物理体表中的名称冲突和重复母逻辑体。登记完成后只读，
未完成或失败时不开放查询。当前不支持把复制体、参数化体或重复母逻辑体下的
多路径目标登记为源；普通嵌套旋转受支持。

`RejectionVolumeSampler` 构造时查询一次目标及其变换。子体积即使没有登记为源，
仍会被导航器排除。`PointSampler` 和 `RejectionVolumeSampler` 保存几何代次，
几何重新构造后旧实例会报错；两个直接采样器不引用实际几何，不需要几何代次。
这只是失效保护；目前不支持运行间自动重建发生器，更换几何仍应启动新进程。

### 测试与后续接入

`tests/check_vertex_sampler.cc` 独立检查固定点和体积采样、显式选择不同体积、
固定点不消耗随机数、非法坐标、嵌套旋转、孔洞和子体积排除，及登记表的错误处理。
`tests/check_direct_samplers.cc` 对六种采样区域各取 60000 个点，检查解析边界、
旋转和平移、均值、二阶矩以及联合等体积/等面积分区计数，并检查固定种子重现、
非法尺寸/坐标/旋转拒绝和初始化不消耗随机数。
`tests/check_generators.py` 检查粒子枪串行/双线程/重复运行、未实现源拒绝，
以及移除的旧命令不再被接受。物理冒烟测试继续通过粒子枪检查衰变和输运。
这些检查不构成实验能谱或本底率的物理验证。

新增场景时，在对应目录实现 `VGenerator`，由具体发生器内部决定采样器及其参数。
模型确定后再接入 `GeneratorManager`，更新命令候选项、CMake、示例、测试和说明。
不为尚未确定的 Th-232/Co-56 模型指定位置、采样器或源封装。

## LMO 灵敏探测器与记录流程

`DetectorConstruction::ConstructSDandField()` 根据实际构建的 LMO 逻辑体配置本线程的
`LMOSensitiveDetector`，注册到 `G4SDManager` 并挂载。SD 位于 `include/`、`src/` 顶层，
与 Action 同层；`record/` 负责记录策略。材料、体积和物理输运逻辑不依赖记录类。

当前事件中的数据流为：

```text
TrackingAction → TrackAncestry：TrackID、ParentID、首次跟踪时的出生物理体名称
LMOSensitiveDetector → LMOSD/Edep：每块 LMO、每条 Track 的原始沉积
EventAction → RecordManager → 所选 Recorder → 原始记录向量 → OutputManager → RootOutput
```

`LMOSD/Edep` 现在的类型是 `DetectorHitsCollection`，即 `G4THitsCollection<DetectorHit>`，
不再是 `G4THitsMap<G4double>`。每个 DetectorHit 对应一块 LMO，内部按 TrackID 累加
`GetTotalEnergyDeposit()`。保留零沉积的轨迹访问，并为未命中的 LMO 创建空条目。
SD 只保留逐轨迹累计值，不保存逐步位置、时间或脉冲信息。

每个事件都会创建新的命中集合；事件拥有并释放集合。每个工作线程独立持有 SD、
TrackAncestry 和 EventAction 的记录向量。记录是值对象，后续输出不能保存指向旧命中集合的指针。
同一轨迹暂停后恢复时，TrackingAction 保留首次记录的出生体积，不将恢复地点当作出生位置。
目前 StackingAction 不把轨迹延后到另一个事件；跨事件追踪谱系尚未支持。

### 两种记录的数据约定

| 记录类 | 记录字段 | 每行含义 |
| --- | --- | --- |
| EventEnergyRecorder | eventId、edep、detectorName | 一个事件中一块探测器的总沉积 |
| IncidentEnergyRecorder | eventId、trackId、edep、detectorName | 一个事件中一块探测器的一个入射根及归属后代的总沉积 |

字段 `eventId`、`trackId`、`edep` 分别对应所需的 EventID、入射根 TrackID 和 Edep。
`detectorName` 使用 `LMO0`、`LMO1` 等物理体名称；当前 LMO 的逻辑体名称与物理体相同。
记录类不硬编码 LMO 前缀，后续 NTD/LD 的 SD 可生成相同类型的集合，但名称必须全局唯一。
NTD/LD 几何和 SD 尚未实现；测试只用合成数据验证名称通用性。

能量使用 Geant4 内部单位，未乘统计权重、未做展宽或阈值处理；输出时再明确转换为 keV 或 MeV。
逐事件记录只包含该事件内总沉积 `Edep > 0` 的探测器，不额外设置能量阈值。
若仅 LMO0 有沉积，只记录 LMO0；所有探测器都为零时，不生成 EventEnergy 记录。
逐入射记录保留总沉积为零的根；完全没有轨迹访问的探测器没有入射记录。
中止事件不生成结果行。正常零沉积事件不生成逐事件行，但若有粒子访问晶体，仍可有零沉积入射行。

### 逐入射粒子的归属规则

对每块探测器单独处理，TrackID 只在当前 Event 内有意义：

1. 在该探测器外出生、随后访问它的轨迹，以自身 TrackID 为根。
2. 在该探测器内出生的次级粒子，沿 ParentID 追溯到该探测器中的入射根。
3. 该探测器内直接生成的初级粒子以自身为根，包括初级放射性核素；其内部衰变链后代也按此规则归属。
4. 同一 Track 离开后重返同一探测器，仍合并到同一个根；不区分多次入射片段。
5. 后代进入另一块探测器，在那块探测器中重新确定根；在外部出生的后代重新进入原探测器，也成为新的入射根。
6. 若父轨迹从未访问该探测器，就在当前轨迹停止追溯，以覆盖边界出生等情况，避免把未入射父粒子当作该探测器的入射根。

算法按每条轨迹向上迭代追溯并缓存结果，给每份沉积指定唯一的根；不对每个候选根无条件递归累加整棵后代树。
这避免同一子树中存在新的外部入射根时重复计能，也避免长链递归导致栈溢出。
缺失轨迹、缺失必要父轨迹及参与追溯的环会报错，不静默丢弃沉积。
ParentID 指直接父粒子的 TrackID，并不限于母核。

同一事件、同一探测器应满足：所有入射记录的 Edep 之和等于逐事件 Edep，允许浮点求和顺序引起的舍入差异。
在正常完成且两个记录器都启用的运行中，缺失的逐事件行按零参与这项比较。
EventID 保留原编号，不因筛选而重新编号；不能用 EventEnergy 的行数推导实际模拟事件总数。
完整衰变链仍属于一个 Geant4 event；这里的能量归属不是实验脉冲、符合窗口或 pile-up 模型。

### Action 调用与管理器

`ActionInitialization` 为每个工作线程分别创建 Action。`EventAction` 持有
`RecordManager` 和 `TrackAncestry`；`RunAction` 持有 `OutputManager`，后者独占所选输出对象。
项目管理器与记录向量属于各线程；底层 G4AnalysisManager 是 Geant4 管理的线程局部单例。

当前记录器选择入口位于 `src/EventAction.cc`：

```cpp
std::vector<G4String> EventAction::DefaultRecorders() {
  return {"EventEnergyRecorder", "IncidentEnergyRecorder"};
}
```

保留一个名称即只开启该记录器，保留两个即开启两种统计，空列表 `{}` 表示都不开启。
名称区分大小写；未知名称或重复名称在创建管理器时立即报错，不会静默跳过。
ActionInitialization 将同一选择传给主线程和工作线程的 RunAction，预先建立一致的数据表；
EventAction 使用对应 RunAction 的选择，显式不一致的选择会报错。修改默认选择后重新编译；没有新增宏命令。
`RequiresTrackAncestry()` 根据所选记录器的依赖决定是否启用 TrackingAction 的谱系登记。

输出选择位于 `include/RunAction.hh` 的构造参数 `outputName`，当前默认 `"RootOutput"`。
显式调用形式为 `RunAction(directory, recorderNames, "RootOutput")`；主线程和工作线程须选择同一模块。
当前只实现 ROOT，不提供虚假的 CSV/其他输出选项。`RunLog` 独立记录整个作业日志，不作为事件数据后端选择。

事件开始时清空谱系和记录；结束时 RecordManager 检查跨集合探测器名称唯一性，调用所选记录器的 Build()。
EventEnergyRecord 和 IncidentEnergyRecord 仍定义在各自记录器的头文件中，统计算法与单位保持不变。
RecordManager 不包含任何 output 头文件，也不执行文件操作。

`RecordBatch` 是原始记录向量的 `std::variant`：每批保留自己的具体类型，不转换成通用字段表。
EventAction 使用 `std::visit` 把实际向量按 const 引用传入 `OutputManager::Write()`：

```cpp
for (const auto& batch : fRecords.GetBatches()) {
  std::visit([&](const auto& records) { output.Write(runId, records); }, batch);
}
```

C++ 根据向量类型选择 Write 重载，输出管理器再调用已选后端的对应虚函数。
事件写入路径没有按记录器名称判断的 if/switch，不使用 void* 或 std::any。
文件开关、错误检查和 ROOT 分支创建条件仍由输出模块正常处理。

新增记录器时：保持其结构体与实现独立，将结果向量加入 RecordBatch，并在 RecordManager 的注册表添加
空结果类型、谱系需求和 Build 适配函数；新增的数据类型还须在 VOutput、OutputManager 和各后端实现明确的 Book 和 Write 映射。
这些纯虚接口要求后端提供实现，不能自动猜测任意结构的字段。无需为每个新类型修改 EventAction 的派发循环。
新增输出模块时：实现 VOutput 的类型建表/写入、扩展名、文件生命周期和结果检查接口，在 OutputManager 工厂表登记名称。

独立测试仍可省略 EventAction 的 RunAction 指针，只产生内存记录。
`GetEventRecords()`、`GetIncidentRecords()` 保留只读访问；结果保留到下一事件清理。
RunAction 用空向量的类型调用 Book()，只建立所选表结构，不写入任何行。
建表必须在第一次 OpenFile() 之前完成；未选择的记录器不创建对应树。

### 待后续讨论的字段

- RunID、作业/配置标识、随机种子：EventID 在不同 run/job 中会重复，需要组合标识保证追溯。
- 入射根粒子名称/PDG、ParentID、产生过程，以及“外部入射/内部初级/边界出生”分类。
- 入射能量、位置、方向及首次入射时间；若要做时间分组，还需保存分段时间数据而非一个总量。
- 统计权重、零沉积筛选策略、中止事件计数和归一化信息。

这些字段尚未加入最终记录结构；ParentID 和出生体积目前仅作为内部溯源信息使用。

### 验证范围

`energy-record-checks` 用确定的谱系检查多入射根、零沉积祖先、跨探测器、重返、内部初级、
未访问父轨迹、通用 LD/NTD 名称、20000 层链、异常谱系、集合 ID 空洞、事件清理与中止事件。
`lmo-sd-*` 在串行和双线程的无/单/双 LMO 几何中，使用真实电子、外部伽马、穿越 μ 子和 geantino，
调用生产代码的 EventAction/TrackingAction，将两种记录的总能量与独立逐步求和核对，并覆盖连续 run。
这些是软件记录验证，不等同于实验能谱或本底率的物理验证。

### LMO 命名约定

`LMO` 专指 Li2MoO4 吸收晶体，后续 NTD、LD 应各自使用独立的组件、体积和记录标识。
LMO 的逻辑体及物理体统一命名为 `LMO` 加零起始编号，例如 `LMO0`、`LMO1`；copy number 与编号一致。
几何组件为 `LMOArray`，数量通过 `/cupid/geometry/lmoCount` 设置；参数字段为
`lmoCount`、`lmoSide`、`lmoGap`、`lmoCenter`。材料名仍为 `Li2MoO4`。
顶层灵敏探测器为 `LMOSensitiveDetector`，SD 名称为 `LMOSD`，事件集合为 `LMOSD/Edep`。
LMO 内部源目录为 `generator/lmo_signal/`，下设 `lmo_radioactivity/` 和 `double_beta_decay/`。
示例宏为 `lmo_only.mac`。旧几何组件名、数量命令和体积名称不作为别名保留；外部宏及分析脚本需按此约定更新。


### ROOT 输出模块与 Action 接线

`output/RootOutput` 是 `G4AnalysisManager` 的薄适配层：负责记录结构到列的映射、字段检查和 keV 换算，
不调用记录器、不做能量汇总。`RunAction` 通过 `OutputManager` 管理生命周期；
`EventAction` 通过强类型 Write 重载逐事件写入。

初始化时调用 `SetDefaultFileType("root")`，MT 模式启用 `SetNtupleMerging(true)`，
并用 `SetNtupleRowWise(false, true)` 保留独立分支和同一行各列的对应关系。
主线程与工作线程在构造 RunAction 时按相同顺序建立所选表结构。
每个线程 BeginOfRunAction 调用 `SetFileName()`、`OpenFile()`；
每个线程 EndOfRunAction 都调用 `Write()`、`CloseFile()`，由 Geant4 负责原生合并。
MT 主线程不填写事件行，只在最终关闭后回读汇总结果。
不再自行拼接工作线程文件名，也不调用 TFileMerger 或另行发布 .partial 文件。

`--output DIR` 指定输出目录；省略时创建 `output/job_<timestamp>`。
有记录的 run 保存 `run0.root`、`run1.root` 等文件，作业共用一份 `run.log`。
如果所有已选表都没有数据行，Geant4 11.4.1 会删除空 ROOT 文件；日志明确写出
`event_rows=0 incident_rows=0 ... file=none(empty)`。不为保留文件而插入虚假物理记录。
IncidentEnergy 中的零沉积入射行仍是有效记录，因此“没有正沉积”并不必然意味着没有 ROOT 文件。
输出目录已有 run.log 或同名目标文件会报错，请为新作业使用新目录。

构建仍保留 ROOT 6.36.10 的 Core、RIO、Tree 依赖，使用 CMake 导出目标；在 WSL 中配置：

```bash
source /home/loyh/geant4/install/bin/geant4.sh
source /home/loyh/root/bin/thisroot.sh
cmake -S . -B build -DGeant4_DIR=/home/loyh/geant4/install/lib/cmake/Geant4 -DROOT_DIR=/home/loyh/root/cmake
cmake --build build -j4
ctest --test-dir build -R root-output-checks --output-on-failure
```

本机 ROOT 的官方 CMake 配置还要求 Vdt 开发依赖，原系统未安装。
本次验证把 Ubuntu 24.04 的 `libvdt-dev` 和 `libvdt0.4`（0.4.4-3build1）
解压在 `build/local-deps/vdt`，并在当前 build 的 CMake 缓存中设置了路径。
若重建构建目录，需重新准备依赖；已有系统 Vdt 开发包的环境可由 CMake 自动查找。
保留当前本地依赖时，也可在配置命令追加：

```bash
-DVDT_INCLUDE_DIR=/home/loyh/simulation/CUPID-CJPL/build/local-deps/vdt/usr/include \
-DVDT_LIBRARY=/home/loyh/simulation/CUPID-CJPL/build/local-deps/vdt/usr/lib/x86_64-linux-gnu/libvdt.so
```

模块编译为 `cupid-output`。当前 ROOT 链接用于关闭后的文件检查及分析测试，事件写入由 Geant4 执行。
下面展示单线程中直接调用后端的顺序；生产程序由 Action 完成这些步骤：

```cpp
#include "output/RootOutput.hh"

cupid::RootOutput output;
output.Book(std::vector<cupid::EventEnergyRecord>{});
output.Book(std::vector<cupid::IncidentEnergyRecord>{});
output.OpenFile("run0.root");
output.WriteEventRecords(runId, eventRecords);
output.WriteIncidentRecords(runId, incidentRecords);
output.CloseFile();
```

Book 的空向量仅表示记录类型，忽略其内容，不写数据行。只调用需要的类型的 Book 即可。
输出对象跨事件存活；重复提交同一批记录会产生重复行。关闭后可用同一对象打开下一个新文件，
已建立的表结构保留，Geant4 的默认 CloseFile 重置本次数据；不允许打开文件后再增加新表类型。
同一线程只允许一个活动 RootOutput 适配器；不要在其他模块另行操作同一个分析单例。
MT 中每线程有自己的适配器，不能跨线程共享对象。

| 树 | 分支 | 一行含义 |
| --- | --- | --- |
| `EventEnergy` | `RunID`、`EventID`、`Edep`、`DetectorName` | 一个 event 中一个探测器的正沉积总量 |
| `IncidentEnergy` | 上述字段加 `TrackID` | 一个 event 中一个探测器的一条入射根及其归属后代的总沉积，可为零 |

ID 是 Int_t，Edep 是 Double_t，能量单位仍为 **keV**。
DetectorName 使用 Geant4 字符串列，ROOT 6.36.10 读回表现为 **TLeafC**，不再是原来的 std::string 对象分支。
现有手写 SetBranchAddress 分析程序应适配这个变化；项目 C++ 和 PyROOT 检查工具已适配。
新文件格式标为 schema 2，树标题包含 `Edep in keV; CUPID schema 2`，run.log 同样记录单位与版本。
不再额外写入 EnergyUnit/SchemaVersion TNamed；检查工具仍支持带这两个对象的历史 schema 1 文件。

所选表预先建立：只要文件保留，其中某个表可以是零行；未选择的表不存在。
不按 EventID 排序；关联记录应使用 RunID/EventID/DetectorName，以及逐入射表中的 TrackID。
不做权重、活度、实时时间或实验响应换算。RunID 仅区分同一作业中的 run，
归一化需要日志里的请求/实际处理事件数，不能用数据行数代替。

打开/写入/关闭失败、无效记录、未建表写入等会抛出异常。整批记录先验证再填写，
但写盘和合并不是事务；失败文件不能视为完整结果。应显式 CloseFile 处理最终写盘错误，析构仅兜底。
父目录必须存在；不支持追加已有文件或断点续写。

`root-output-checks` 检查实际文件回读、字段和单位、错误输入、覆盖保护、对象复用、空文件删除及 100005 行读回。
`native-analysis-mt-*` 使用四线程和不同记录器选择（含反向顺序），逐行检查 RunID/EventID/TrackID/
名称/能量对应关系，覆盖连续 run、空闲工作线程和完全空运行。
`output-integration-checks` 使用生产程序检查串行及四线程输运、日志、输出文件集合、覆盖保护，
同时通过 C++ 和 PyROOT 检查工具回读。均属于软件验证，不等同于能谱、本底率的物理验证。

复用依据与现有模块审查见 [Geant4 源码复用审查](docs/geant4_reuse_audit.md)。


### 双 LMO、外置 Th-232 的 100000 事件运行

`macros/th232_two_lmo.mac` 使用现有 gun 生成零动能、零电荷、基态 Th-232，保留 Cryostat、
InnerShielding 和两个 LMO。专用 Th232Generator 仍为占位类，本次未给它增加源模型。
源在世界坐标 `(-354.2,-350,0) mm`，沿 +X 方向正对 `LMO0=(-25,-350,0) mm`。
该高度 AlLower 外半径为 344.2 mm，因此源距外表面 10 mm；几何检查使用真实 polycone
和 navigator 验证了距离及源点位于 World。`LMO1=(25,-350,0) mm`。
方向只说明源与晶体的相对位置，不约束放射性衰变产物的发射方向。

```bash
source /home/loyh/geant4/install/bin/geant4.sh
source /home/loyh/root/bin/thisroot.sh
./build/cupid-sim macros/th232_two_lmo.mac --threads 8 --output results/new_th232_run
python3 tests/check_root_records.py results/new_th232_run/run0.root --events 100000
```

历史结果位于 `results/th232_two_lmo_100000_20261026/`，现已按要求精简为
`th232_two_lmo_100000.root` 和 `run.log`。参数、原运行日志、结果说明及检查报告已嵌入 run.log；
试跑文件、线程文件和独立辅助资料已清理，最终 ROOT 数据保持字节不变。
输出与结果目录不纳入 Git，相关源码、测试与宏纳入版本控制。

以下为零沉积筛选修改之前保存的历史结果，原文件保持不变。
重新检查该旧文件时，在 `tests/check_root_records.py` 的命令末尾加 `--event-policy all`；新运行使用默认 `positive`。
正式运行使用 8 个工作线程、种子命令 `2322026 100000`，完成 100000 个事件。
EventEnergy 有 200000 行；IncidentEnergy 有 424 行（含零沉积入射根）。
按 EventID/DetectorName 对照，两种统计的总能量逐项一致，没有缺失或重复事件行。
LMO0 有 5 个非零沉积事件，总沉积 794.2298914956012 keV；LMO1 有 4 个，
总沉积 591.8330233710606 keV。统计量不足以作为有效的刻度谱。

这里是现有真空世界中的无封装理想点源。反冲核照常输运，可能植入制冷机或离开世界；
没有强制把所有子核固定在源点，也没有活度/实时时间、源封装或电子学响应模型。
单事件详细追踪确实观察到 Th-232 链到 Pb-208，但离开 World 的轨迹会终止，
不能把这个测试解释为所有子核始终留在实际源封装内的完整刻度源模型。
单位为 keV，事件聚合不使用探测器时间窗口，结果不能直接按实验脉冲解释。


### build 测试产物清理与零沉积筛选

已清理旧的 `build/test-results`、`physics-test-results`、`generator-test-results`、
`root-output-test-results` 和 `Testing` 日志，以及废弃的 `check-crystal-sd` 可执行文件与对象目录。
当前可执行文件、CMake 缓存、编译对象和 `build/local-deps/vdt` 构建依赖保留。
`results/` 中的正式 Th-232 输出及其追溯文件不属于这次清理范围。
验证后只保留最新测试日志及当前检查结果，不自动删除历史生产数据。

逐事件记录现在仅保存严格大于零的总沉积。零沉积事件不会占 EventEnergy 行，但
仍是实际运行事件的一部分；归一化分母应读取运行日志或运行配置。
`tests/check_root_records.py` 默认检查正沉积记录与逐入射总量的一致性，不能证明
未出现在两棵树中的零沉积事件是否完成；完整运行数量需另外核对运行日志。

历史直接 ROOT 写入模式的小样本线程文件可能仅含元数据、没有树；以下兼容选项只用于这类旧文件。
核对运行日志确认该线程事件数为零后，可给读回检查工具加 `--allow-empty-files`；
默认仍拒绝缺树文件。仅缺其中一棵树的文件不在此例外内。当前原生合并模式不生成这类独立线程文件。


### 单一运行日志

`output/RunLog` 在创建 Geant4 工作线程之前启用，在线程结束后退出，将应用的 C++ 标准输出、
错误输出及 Geant4 转发的日志同时写入控制台和输出目录中的 `run.log`。
无需另外重定向到该路径，尤其不要用 `> 输出目录/run.log`，以免提前创建文件触发保护。
日志包含命令行、工作目录、Geant4/ROOT 版本、主宏和可解析的静态子宏内容、实际 gun 参数、
run 的随机状态、请求/实际处理事件数、合并后的行数、格式版本及能量单位说明。
宏中的动态别名引用若无法预解析会在日志中注明；运行时实际 gun 参数仍在 run 开始时记录。
交互式会话中的静态启动宏被保存，任意交互命令不保证全部回显；完整配置复现应使用保存的宏。

正常作业目录示例：

```text
output/job_.../
  run0.root
  run.log
```

同一进程多次 beamOn 时，每个有记录的 run 增加一个 ROOT 文件，全部运行参数、控制台日志和结果摘要
继续写入同一 run.log。不再额外生成 manifest、结果说明、合并日志或自动源码快照。
目录精简不改变数据含义：EventEnergy 仍只记录正沉积，IncidentEnergy 仍保留零沉积入射根。


### 管理器验证

`record-output-manager-checks` 检查无记录器、单一记录器、双记录器和反向选择顺序；
拒绝未知/重复名称，检查谱系需求、多命中集合与重复探测器名、逐事件清空，以及真实 ROOT 回读中的
RunID/EventID/TrackID/探测器名称/keV 能量。还验证只生成所选树、管理器文件生命周期和完全空文件的删除。
`output-integration-checks` 使用生产 Action 检查串行及四线程连续运行、自动合并和日志中的模块选择。
