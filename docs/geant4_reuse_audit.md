# Geant4 源码复用审查

日期：2026-10-28。范围：当前已实现的几何、发生器工具、Action、SD、record、output、UI 和物理列表。
依据：本机 `/home/loyh/geant4/geant4-v11.4.1/source/` 中的官方 **11.4.1** G4 头文件与实现；
ROOT 回读接口核对本机 **6.36.10**。下文路径均相对该 source 目录，不以 examples 的应用代码代替源码依据。
结论只针对当前接口和需求，不宣称任意 Geant4/ROOT 版本组合均兼容。

## 已采用的复用

| 模块 | G4 源码依据 | 本次决定 |
| --- | --- | --- |
| ROOT 写入与多线程合并 | `analysis/factory/include/G4AnalysisManager.hh`、`analysis/factory/src/G4GenericAnalysisManager.cc`、`analysis/root/src/G4RootNtupleFileManager.cc` | 用原生建表、Fill、SetNtupleMerging、SetFileName、Write/CloseFile 替代手写 TTree 和 TFileMerger；保留薄类型映射和输入检查 |
| 球体/球面/半球面采样的方向生成 | `global/HEPRandom/include/G4RandomDirection.hh` | 使用无参数版本得到均匀立体角方向，参数 0 的重载得到 +Z 半球；保留体积径向立方采样与局部坐标变换 |

原生分析管理器是线程局部单例；主线程和工作线程必须建立相同的表，工作线程负责填行。
`SetNtupleRowWise(false, true)` 保留各列在同一行中的关联。它不保证 EventID 排序。
`analysis/hntools/src/G4ToolsAnalysisManager.cc` 与 `analysis/management/include/G4TFileManager.icc`
说明默认关闭会重置本次数据并删除完全空文件；项目日志明确记录空运行。
`analysis/management/src/G4NtupleBookingManager.cc` 的 ClearData 保留 first-ntuple-ID 的锁定状态，
因此适配器复用时检查现有编号而不重复设置。字符串列在 ROOT 回读为 TLeafC，格式标为 schema 2。

G4RandomDirection 与旧实现统计分布一致，但消耗随机数的方式不同；相同种子不保证与旧版本逐事件相同。

## 已有模块逐项评估

| 当前模块 | 可用 G4 类/源码 | 结论与理由 |
| --- | --- | --- |
| GeometryManager 的组件注册、体积查找与世界坐标 | `geometry/management/src/G4PhysicalVolumeStore.cc`，`geometry/navigation/` 的 G4Navigator | 保留项目注册表。GetVolume 按名称查找，重复名称只能警告并返回某个对象，不能替代唯一放置标识、层级坐标组合和重建后的失效检查；几何查询已使用 G4Navigator |
| 几何构造与重叠检查 | `geometry/solids/` 的 G4Box/G4Tubs/G4Polycone/G4SubtractionSolid；`geometry/volumes/include/G4PVPlacement.hh` | 已复用 G4 实体、放置及 CheckOverlaps；装置尺寸、组件组织仍是实验代码 |
| 材料 | `materials/include/G4NistManager.hh`、G4Element/G4Material | 已使用 G4 元素/材料接口和合适的 NIST 材料；不能仅凭材料名称相似就替换实验配方、密度或富集比例 |
| 点和简单体积/表面采样 | `event/include/G4SPSPosDistribution.hh`、`event/src/G4SPSPosDistribution.cc` | 原生支持 Point/Plane/Surface/Volume 等，后续配置型源优先考虑。当前纯函数接口还带参数检查与坐标约定，SPS 需要有状态配置及 G4SPSRandomGenerator，不是直接同签名替换；本次复用其中可直接适配的 G4RandomDirection |
| RejectionVolumeSampler | 同上 SPS 的 ConfineSourceToVolume/IsSourceConfined | 保留。原生按物理体名称约束，未知名称会解除约束，达到尝试上限可返回未约束采样点；项目要求明确报错、特定放置、排除子体积、独立 navigator 和几何失效检测，语义不同 |
| NuclideEmitter 与初级发生器 | `particles/management/include/G4IonTable.hh`；`event/include/G4ParticleGun.hh`、G4GeneralParticleSource | 已复用粒子定义、离子表和初级粒子接口。未来具体背景/刻度源先核对 GPS/SPS 能量、角度和位置能力；核素发射不等于自行实现衰变模型 |
| LMOSensitiveDetector 与逐事件记录 | `digits_hits/detector/include/G4VSensitiveDetector.hh`；`digits_hits/scorer/src/G4PSEnergyDeposit.cc` | SD/hits 已建立在 G4 接口上。G4PSEnergyDeposit 跳过零沉积，乘以轨迹权重，并按 copy index 累计；不能整体替代当前不加权、保留入射零沉积轨迹信息的数据流。单位权重下可作为事件总量的独立验证手段 |
| 逐入射记录与谱系 | `track/include/G4VUserTrackInformation.hh`、G4Track | G4 提供 track payload 和 ParentID，但不会自动完成“每块探测器的入射根”归属。Track information 随 track 生命周期结束；当前记录器还需事件结束后的谱系查询，保留项目算法和事件级存储 |
| RecordManager / OutputManager | G4AnalysisManager | 原生分析接口处理列与文件，不会自动理解项目记录结构或选定统计语义；保留名称注册、强类型派发及薄适配。文件合并交给 G4 |
| GeometryMessenger / GeneratorMessenger | `intercoms/include/G4GenericMessenger.hh`，G4UImessenger/G4UIcmd* | 已使用 G4 UI 命令。GenericMessenger 可减少属性/方法命令样板，后续命令优先评估；当前域校验、状态限制和错误报告有既定行为，不在输出迁移中改动 |
| RunLog | `intercoms/include/G4UImanager.hh` 的 SetCoutDestination；G4UIsession | 原生可接收 G4cout/G4cerr，但不能单独覆盖当前标准流、启动错误、宏内容与参数追溯，保留项目日志封装 |
| PhysicsList | `physics_lists/constructors/` 的 G4 电磁、衰变、放射性衰变和强子物理构造器 | 已直接注册 G4 构造器，没有重写输运模型；模型组合、截断与实验验证属于项目职责。不能假定任意核的 2νββ/0νββ 专属发生分布已由普通放射性衰变接口提供 |

这里的“保留”表示已检查现成接口后发现职责或行为不同，不表示禁止以后使用原生能力。
后续新增源时优先评估 G4GeneralParticleSource/G4SPS*；新增简单 UI 命令优先评估 G4GenericMessenger。
先明确目标分布、权重、子体积处理和错误语义，再决定直接调用或增加必要的薄封装。

## 验证与限制

本次迁移采用串行和真实四工作线程测试：所有记录器选择、表顺序、连续 run、空闲工作线程、
完全空运行、列对应关系、错误输入、覆盖保护以及 C++/PyROOT 文件回读。
采样器测试检查取值范围和统计矩；既有几何、SD、谱系与物理配置回归测试继续执行。
最终结果记录在 history.log。软件检查通过不代表刻度谱、宇宙线或本底率已经完成物理验证。
历史生产结果不重写；新旧字符串分支和元数据布局不同，分析侧应按 schema 区分。
