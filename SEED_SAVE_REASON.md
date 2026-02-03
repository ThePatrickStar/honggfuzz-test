# 基于环境变量修改种子保留原因

通常对于Fuzzer，除了crash，有两个重要的种子保留原因，一个是发现了新的边，另外一个是边的运行次数发生了变化。

现在需要，当设置了环境变量DISABLE_COV_BIN_COUNT的时候，不再考虑边的运行次数，而是仅在发现新的边的时候，才认为种子是interesting的，保留到队列中。

## Honggfuzz的种子保留逻辑

### 核心决策点

种子保留的主要判断在 `fuzz.c:fuzz_perfFeedback()` 函数（约第 258-261 行）：

```c
if (run->hwCnts.newBBCnt > 0 || softNewPC > 0 || softNewEdge > 0 || softNewCmp > 0 ||
    softNewStackDepth || diff0 < 0 || diff1 < 0) {
    input_addDynamicInput(run);  // 保存种子
}
```

### 种子被保留的条件（满足任一即保留）

| 条件 | 含义 |
|------|------|
| `newBBCnt > 0` | 发现新的基本块（硬件计数器） |
| `softNewPC > 0` | 发现新的程序计数器位置（软件插桩） |
| `softNewEdge > 0` | 发现新的边（软件插桩） |
| `softNewCmp > 0` | 比较指令有新进展 |
| `softNewStackDepth` | 观察到更深的调用栈 |
| `diff0 < 0` | CPU 指令计数有变化 |
| `diff1 < 0` | CPU 分支计数有变化 |

### 覆盖率反馈的来源

1. **硬件反馈**（Linux）：
   - `linux/perf.c`: 通过 perf 计数器获取
   - `linux/pt.c`: Intel Processor Trace 获取新基本块

2. **软件插桩**（libhfuzz）：
   - 编译时插入覆盖率收集代码
   - 通过共享内存传递覆盖率 bitmap（FD 1022）
   - CMP 反馈通过 FD 1019 传递

### 执行流程

```
模糊测试执行 → 覆盖率收集 → 反馈处理 → 决策判断 → 保存种子
     ↓              ↓            ↓           ↓          ↓
subproc_Run()   bitmap更新   fuzz_perfFeedback()  条件判断  input_addDynamicInput()
```

### 关键源文件

| 文件 | 函数 | 作用 |
|------|------|------|
| `fuzz.c` | `fuzz_perfFeedback()` | **主决策逻辑** |
| `input.c` | `input_addDynamicInput()` | **种子保存逻辑** |
| `honggfuzz.h` | `struct _dynfile_t` | 种子元数据结构 |
| `linux/perf.c` | - | 硬件覆盖率收集 |

### 边的计数（bin count）相关

在软件插桩模式下，覆盖率 bitmap 不仅记录边是否被执行，还会记录执行次数的"桶"（bucket）。当边的执行次数跨越桶边界时（如从 1-3 次变为 4-7 次），也会被认为是有新发现。

#### 插桩代码中的 bin count 实现

**位置**: `libhfuzz/instrument.c`

**1. 桶映射表 `instrumentCntMap`（第 731-741 行）**

```c
static uint8_t const instrumentCntMap[256] = {
    [0]          = 0,         // 0次   → 0
    [1]          = 1U << 0,   // 1次   → 0b00000001
    [2]          = 1U << 1,   // 2次   → 0b00000010
    [3]          = 1U << 2,   // 3次   → 0b00000100
    [4 ... 5]    = 1U << 3,   // 4-5次 → 0b00001000
    [6 ... 10]   = 1U << 4,   // 6-10  → 0b00010000
    [11 ... 32]  = 1U << 5,   // 11-32 → 0b00100000
    [33 ... 64]  = 1U << 6,   // 33-64 → 0b01000000
    [65 ... 255] = 1U << 7,   // 65+   → 0b10000000
};
```

这是 AFL 经典的 hit count 桶化方案。边的原始执行次数 `v` 被映射为 8 个桶之一。

**2. 核心判断逻辑（第 803-821 行）**

```c
const uint8_t newval = instrumentCntMap[v];        // 当前执行次数 → 桶值
if (globalCovFeedback->pcGuardMap[guard] < newval) {  // 全局记录的桶值 < 新桶值？
    const uint8_t oldval = ATOMIC_POST_OR(globalCovFeedback->pcGuardMap[guard], newval);
    if (!oldval) {
        // oldval == 0 → 这条边从未被执行过 → 真正的新边
        ATOMIC_PRE_INC(globalCovFeedback->pidNewEdge[my_thread_no].val);
    } else if (oldval < newval) {
        // oldval != 0 但 < newval → 边已存在，但执行次数跨越了新的桶
        ATOMIC_PRE_INC(globalCovFeedback->pidNewCmp[my_thread_no].val);  // ← bin count 影响点！
    }
}
```

**3. 两种情况的区分**

| 场景 | `oldval` | `newval` | 递增计数器 | 含义 |
|------|----------|----------|-----------|------|
| **新边** | `0` | `>0` | `pidNewEdge` | 这条边之前从未被执行 |
| **bin count 变化** | `>0` 且 `< newval` | `> oldval` | `pidNewCmp` | 边已存在，但执行次数进入新桶 |

**4. bin count 变化触发种子保留的路径**

```
instrument.c: ATOMIC_PRE_INC(globalCovFeedback->pidNewCmp[my_thread_no].val)
       ↓
fuzz.c: softNewCmp = ATOMIC_GET(covFeedbackMap->pidNewCmp[run->fuzzNo].val)
       ↓
fuzz.c: if (... || softNewCmp > 0 || ...) → input_addDynamicInput()  // 保存种子
```

注：`pidNewCmp` 是按线程索引的数组，每个 fuzzing 线程只读取自己对应的计数器（`run->fuzzNo`），不是汇总所有线程

#### 实现 `DISABLE_COV_BIN_COUNT` 的方案

要禁用 bin count 对种子保留的影响，在 `fuzz.c` 的种子保留决策前清零 `softNewCmp`。

**修改位置**: `fuzz.c` 第 258 行附近（if 判断前）

**修改内容**:
```c
/* Any increase in coverage (edge, pc, cmp, hw, stack) counters forces adding input to the
 * corpus */
if (getenv("DISABLE_COV_BIN_COUNT")) {
    static bool warnPrinted = false;
    if (!warnPrinted) {
        LOG_I("DISABLE_COV_BIN_COUNT is set, ignoring bin count changes for seed retention");
        warnPrinted = true;
    }
    softNewCmp = 0;
}
if (run->hwCnts.newBBCnt > 0 || softNewPC > 0 || softNewEdge > 0 || softNewCmp > 0 ||
    ...
```

- 使用 `static bool warnPrinted` 确保提示信息只打印一次
- 在决策 if 判断前将 `softNewCmp` 清零，使 bin count 变化不再触发种子保留


