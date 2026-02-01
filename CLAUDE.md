# Honggfuzz 特性基于环境变量的开关

当前的目标是,基于Honggfuzz的种子调度,Mutator等代码逻辑,设置环境变量开关部分逻辑。比如设置了某个特殊环境变量后，将会关闭这一因素在种子调度中的考虑，即不再基于这个逻辑考虑。

1. 针对每个特性的修改要单独放在一个commit中。
2. 必须在代码逻辑被禁用的时候，打印出信息提示。并且使用static的bool变量保证这个只打印一次，防止淹没输出。

关注的特性：

种子调度方面的三种特性：
  - 是否倾向于更短的种子
  - 是否倾向于coverage更多的种子
  - 是否倾向于执行时间更快的种子

### Honggfuzz代码文件及其作用

#### 核心种子调度文件

**power.c** (245行)
- 实现 `power_calculateEnergy()` 函数，计算每个种子的能量/优先级
- 综合考虑15+个因素：新颖性、密度、速度、生育力、新鲜度、大小、栈深度、路径多样性、CMP进度、稀有边、选择次数、深度、停滞、熵、超时状态等
- 能量值决定种子被fuzz的次数（skip factor）
- **关键点**：这里包含了三个目标特性的计算逻辑
  - 种子大小偏好（size因素）
  - coverage偏好（novelty、density、rareEdgeCnt等因素）
  - 执行速度偏好（speed因素）

**input.c** (982行)
- `input_prepareDynamicInput()` (line 471) - 主要的种子选择函数
  - 遍历动态语料库队列
  - 调用 `power_calculateEnergy()` 计算每个种子的能量
  - 基于能量值进行概率性跳过（skip）
  - 跟踪 `triesLeft` 以重复fuzz高能量种子
- `input_getRandomInputAsBuf()` (line 727) - 随机种子选择（用于splicing）
- `input_getDiverseInputAsBuf()` (line 763) - 多样化种子选择（用于crossover）

**fuzz.c** (808行)
- 主fuzzing循环和工作线程逻辑
- 管理fuzzing阶段（dry-run、main、minimize）
- 调用种子选择和变异函数
- 处理执行后的coverage反馈（lines 232-341）

#### 变异/Mutation文件

**mangle.c** (2022行 - 最大的文件)
- 实现33+种变异策略（enum `mangle_t`）
- 变异类型包括：
  - 数据变异：SHRINK、EXPAND、BIT、BYTE操作
  - 算术运算：INC_BYTE、DEC_BYTE、NEG_BYTE、ADD_SUB、ARITH8
  - 内存操作：MEM_SET、MEM_CLR、MEM_SWAP、MEM_COPY
  - 块操作：BLOCK_MOVE、BLOCK_REPEAT、BLOCK_SWAP
  - 高级变异：SPLICE、CROSS_OVER、CMP_SOLVE、GRADIENT_CMP
  - 字典变异：STATIC_DICT、CONST_FEEDBACK_DICT、DICT_INSERT
  - 特殊变异：HAVOC、SPECIAL_STRINGS、TLV_MUTATE、TOKEN_SHUFFLE

**mangle.h** (31行)
- 变异函数的头文件
- 主入口点：`mangle_mangleContent(run_t* run)`

#### Coverage追踪相关

**honggfuzz.h**
- **dynfile_t结构体** (lines 150-172)：存储每个种子的coverage数据
  - `uint64_t cov[4]` - Coverage指标数组
    - `cov[0]` = edges + PCs + basic blocks
    - `cov[1]` = comparison feedback
    - `cov[2]` = CPU instruction/branch counts
    - `cov[3]` = size-based metric
  - `uint32_t newEdges` - 添加时发现的新边数
  - `uint16_t rareEdgeCnt` - 该输入命中的稀有边数量
  - `uint64_t pathHash` - 执行路径哈希（用于多样性）
  - `uint32_t cmpProgress` - 比较进度分数
- **feedback_t结构体** (lines 199-219)：每线程coverage计数器
  - 新发现的PC、Edge、CMP计数
  - 总计数器
  - 执行路径哈希
  - `edgeHitCnt[65536]` - 全局边频率追踪

**libhfuzz/instrument.c** 和 **.h**
- 编译器插桩回调函数
- 在目标执行期间收集coverage数据
- 函数：`instrument8BitCountersCount()`, `instrumentUpdateCmpMap()`, `instrumentCheckStackDepth()`

#### 执行时间追踪相关

**honggfuzz.h**
- **dynfile_t结构体**：
  - `uint64_t timeExecUSecs` - 该种子的执行时间（微秒）
- **run_t结构体** (lines 403-436)：
  - `int64_t timeStartedUSecs` - 当前执行开始时间
- **honggfuzz_t结构体** (lines 285-293)：
  - `int64_t timeOfLongestUnitUSecs` - 观察到的最长执行时间

**subproc.c** (577行)
- `subproc_Run()` - 执行目标进程
- 测量执行时间（lines 447, 524）
- 计算 `diffUSecs = util_timeNowUSecs() - run->timeStartedUSecs`

**libhfuzz/performance.c** 和 **.h**
- 性能监控函数
- `performanceCheck()` - 检查执行性能

#### 支持性基础设施文件

**libhfcommon/util.c** (33,156字节)
- 工具函数：`util_rnd64()`, `util_Log2()`, `util_timeNowUSecs()`
- 随机数生成（用于概率性种子选择）

**libhfcommon/log.c** 和 **.h**
- 日志基础设施

**display.c** (465行)
- 统计信息显示和报告

**report.c** (164行)
- 崩溃报告

**dict.c** (181行)
- 字典管理（用于变异）

#### 种子调度数据流

1. **选择阶段** (`input_prepareDynamicInput()` in input.c)
   - 遍历动态语料库队列
   - 为每个种子调用 `power_calculateEnergy()`
   - 使用能量值确定skip factor
   - 基于能量进行概率性选择

2. **能量计算** (`power_calculateEnergy()` in power.c)
   - 综合考虑15+个因素，包括：
     - **Size（大小）** - 更小的种子更好
     - **Speed（速度）** - 执行时间更快的种子更好
     - **Novelty（新颖性）** - 发现新边的种子
     - **Density（密度）** - 每字节的coverage
     - **RareEdges（稀有边）** - 命中稀有边的种子
     - 其他因素：fertility、freshness、stack depth、path diversity、CMP progress等

3. **变异阶段** (`mangle_mangleContent()` in mangle.c)
   - 应用选定的变异策略
   - 33+种不同的变异类型

4. **反馈收集** (fuzz.c lines 232-341)
   - 收集coverage指标
   - 更新种子元数据
   - 追踪执行时间

#### 实现环境变量开关的关键位置

基于以上分析，实现三个特性的环境变量开关应该在 **power.c** 的 `power_calculateEnergy()` 函数中：

- **更短种子偏好**：控制size因素的权重
- **更多coverage偏好**：控制novelty、density、rareEdgeCnt等因素的权重
- **更快执行时间偏好**：控制speed因素的权重

