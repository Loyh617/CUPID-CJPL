# CUPID-CJPL 初级 Geant4 framework

v0.1：通过 `.mac` 选择几何组件，显示装置、检查重叠，运行测试粒子输运。

这是独立的新工程。参考工程只用于理解尺寸和装置关系，没有作为构建依赖。
当前不包含晶体能量 ROOT 输出、Th-232/Co-56 源模型、岩石、大厅、电子学或波形。
`RunAction`、`EventAction`、`SteppingAction`、`TrackingAction`、`StackingAction` 已注册，暂只保留接口。
`StackingAction::ClassifyNewTrack()` 返回 `fUrgent`，所有轨迹留在当前事件中继续处理。
`include/` 与 `src/` 下的 `generator/`、`record/` 目录暂为空；现有粒子枪继续可用。

## 1. 在 WSL Ubuntu 中构建

要求 Geant4 **11.4.1**（启用多线程）、C++17、CMake >= 3.16。
默认构建可视化版本。自动检查使用 Python 3 标准库，无需安装 Python 包。
本版本不直接链接 ROOT。

在 **WSL 的终端**中进入本工程目录：

```bash
source /home/loyh/geant4/install/bin/geant4.sh
cmake -S . -B build \
  -DGeant4_DIR=/home/loyh/geant4/install/lib/cmake/Geant4 \
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
./build/cupid-sim crystal_only.mac --threads 2

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
/cupid/geometry/enable CrystalArray
/cupid/geometry/crystalCount 1
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
| `/cupid/geometry/crystalCount 1` | 一颗居中晶体；也可以设为 2，沿 X 排列 |
| `/cupid/geometry/check 10000` | 初始化后对所有放置体积执行重叠检查 |

组件名称区分大小写：`Cryostat`、`InnerShielding`、`CrystalArray`。
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
  DetectorConstruction          世界创建和整体构造入口
  ActionInitialization          创建线程局部的用户 actions
  PrimaryGeneratorAction        G4ParticleGun，支持 /gun/ 命令
  PhysicsList                   逐项注册物理构造器和设置参数
  RunAction                     运行开始/结束接口，暂为空
  EventAction                   事件开始/结束接口，暂为空
  SteppingAction                每步处理接口，暂为空
  TrackingAction                轨迹开始/结束接口，暂为空
  StackingAction                新轨迹分类、新阶段、事件准备接口
  generator/                    空目录，预留事件发生器模块
  record/                       空目录，预留数据记录模块
  geometry/
    GeometryManager             组件登记、开关、装配、重叠检查
    GeometryMessenger           /cupid/geometry/ 命令
    GeometryParameters.hh       主要尺寸、坐标和晶体数量
    Cryostat                    冷板、铜罩和铝罩
    InnerShielding              三段内铜屏蔽
    CrystalArray                一颗或两颗 LMO
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
  每颗晶体具有独立的 LogicalVolume，名称为 `Crystal0`、`Crystal1`，数字与 copy number 一致。
  它们共用相同尺寸的 Solid 和 `Li2MoO4` 材料；每颗晶体的 PhysicalVolume 与 LogicalVolume 同名，均为 `Crystal` 加编号。
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

当前一个事件为粒子枪的一次发射及其后续输运；可用 `/gun/particle ion` 和
`/gun/ion Z A 0 0` 注入静止核素来检查衰变。已具备核素衰变输运能力，
但尚未实现专用 Th-232/Co-56 源的空间分布、归一化、仅衰变一次的选择策略、
衰变时间整理或 ROOT 计分。整链沉积总和不能直接当作单次探测器脉冲能量。
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
空目录不含占位文件，Git 不跟踪空目录；新检出时需自行建立这些目录。

## 7. 添加组件

1. 在 `include/geometry/` 和 `src/geometry/` 添加对应 `.hh/.cc`。
2. 按现有组件提供 `Build(mother, materials, parameters)`，使用传入母体积。
3. 在 `GeometryManager` 的组件表中登记名称和构造入口，并更新表的大小及命令帮助。
4. 在顶层 `CMakeLists.txt` 显式加入源文件。
5. 增加一个宏算例和几何组合检查。

若新组件依赖特定母体积，应先在管理器中检查依赖并传递正确母体积，
不能仅因为有开关就随意放入世界。新增材料集中在 `Materials` 定义。

## 8. 小团队协作

几何成员主要修改 `geometry/`；源成员修改 `PrimaryGeneratorAction`；
运行与后续记录成员修改 actions。新增公共命令、坐标或晶体编号需在 README 中同步说明。
采用个人分支及代码审查；提交前运行上述 CTest 检查。
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
- `crystal_only.mac` 在双工作线程下完成 1,000 个事件。
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
