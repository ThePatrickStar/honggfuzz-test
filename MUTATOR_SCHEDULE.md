# Honggfuzz Mutator Schedule 策略分析

本文档描述当前 Honggfuzz 代码库中使用的 Mutator 调度策略。

## 1. Mutator 类型定义

### 1.1 Mutator 枚举 (mangle.c:105-143)

定义了 36 种 mutator 类型：

| 枚举值 | 名称 | 功能描述 |
|--------|------|----------|
| MANGLE_SHRINK | 收缩 | 删除输入的一部分 |
| MANGLE_EXPAND | 扩展 | 在输入中插入空间 |
| MANGLE_BIT | 位翻转 | 随机翻转一个比特 |
| MANGLE_INC_BYTE | 字节递增 | 随机位置字节+1 |
| MANGLE_DEC_BYTE | 字节递减 | 随机位置字节-1 |
| MANGLE_NEG_BYTE | 字节取反 | 随机位置字节按位取反 |
| MANGLE_ADD_SUB | 加减运算 | 对1/2/4/8字节整数加减 |
| MANGLE_ARITH8 | 8位算术 | 8位整数算术变异 |
| MANGLE_MEM_SET | 内存设置 | memset 填充随机值 |
| MANGLE_MEM_CLR | 内存清零 | memset 填充0 |
| MANGLE_MEM_SWAP | 内存交换 | 交换两个内存区域 |
| MANGLE_MEM_COPY | 内存复制 | 复制内存区域 |
| MANGLE_BLOCK_MOVE | 块移动 | 移动数据块 |
| MANGLE_BLOCK_REPEAT | 块重复 | 重复数据块 |
| MANGLE_BLOCK_SWAP | 块交换 | 交换两个数据块 |
| MANGLE_CHUNK_SHUFFLE | 块打乱 | 打乱固定大小的块 |
| MANGLE_BYTES | 随机字节 | 插入1-2个随机字节 |
| MANGLE_BYTE_REPEAT | 字节重复 | 重复某个字节 |
| MANGLE_RANDOM_BUF | 随机缓冲区 | 填充随机数据 |
| MANGLE_INTERESTING_VALUES | 有趣值 | 插入边界值(0,1,0x7f,0x80等) |
| MANGLE_ASCII_NUM | ASCII数字 | 插入ASCII数字字符串 |
| MANGLE_ASCII_NUM_CHANGE | ASCII数字变化 | 修改现有ASCII数字 |
| MANGLE_MAGIC | 魔数 | 插入魔数值 |
| MANGLE_STATIC_DICT | 静态字典 | 使用用户提供的字典 |
| MANGLE_CONST_FEEDBACK_DICT | 动态字典 | 使用CMP反馈的常量 |
| MANGLE_CMP_SOLVE | CMP求解 | 基于比较反馈求解 |
| MANGLE_SPLICE | 拼接 | 从其他种子拼接数据 |
| MANGLE_CROSS_OVER | 交叉 | 与其他种子交叉 |
| MANGLE_SPECIAL_STRINGS | 特殊字符串 | 插入特殊字符串(SQL注入等) |
| MANGLE_TLV_MUTATE | TLV变异 | 变异长度字段 |
| MANGLE_TOKEN_SHUFFLE | 令牌打乱 | 打乱以分隔符分割的令牌 |
| MANGLE_GRADIENT_CMP | 梯度CMP | 梯度引导的比较求解 |
| MANGLE_ARITH_CONST | 算术常量 | 对CMP常量进行算术变异 |
| MANGLE_DICT_INSERT | 字典插入 | 组合字典词条插入 |
| MANGLE_PUNCTUATION | 标点符号 | 插入标点符号 |
| MANGLE_HAVOC | 混沌模式 | 一次应用16-128个随机变异 |

### 1.2 Mutator 类别 (mangle.c:46-58)

Mutator 被分为 10 个类别，每个类别可以通过环境变量禁用：

| 类别 | 环境变量 | 包含的 Mutator |
|------|----------|----------------|
| MUT_CAT_BITFLIP | HF_DISABLE_MUT_BITFLIP | MANGLE_BIT, MANGLE_NEG_BYTE |
| MUT_CAT_ARITH | HF_DISABLE_MUT_ARITH | MANGLE_INC_BYTE, MANGLE_DEC_BYTE, MANGLE_ADD_SUB, MANGLE_ARITH8, MANGLE_ARITH_CONST |
| MUT_CAT_INTERESTING | HF_DISABLE_MUT_INTERESTING | MANGLE_MAGIC, MANGLE_INTERESTING_VALUES, MANGLE_SPECIAL_STRINGS, MANGLE_PUNCTUATION |
| MUT_CAT_DICT_EXTRA | HF_DISABLE_MUT_DICT_EXTRA | MANGLE_STATIC_DICT, MANGLE_DICT_INSERT |
| MUT_CAT_DYN_DICT | HF_DISABLE_MUT_DYN_DICT | MANGLE_CONST_FEEDBACK_DICT |
| MUT_CAT_RANDOM_BYTES | HF_DISABLE_MUT_RANDOM_BYTES | MANGLE_BYTES, MANGLE_BYTE_REPEAT, MANGLE_MEM_SET, MANGLE_MEM_CLR, MANGLE_RANDOM_BUF |
| MUT_CAT_STRUCTURAL_BYTES | HF_DISABLE_MUT_STRUCTURAL_BYTES | MANGLE_SHRINK, MANGLE_EXPAND, MANGLE_MEM_SWAP, MANGLE_MEM_COPY, MANGLE_BLOCK_MOVE, MANGLE_BLOCK_REPEAT, MANGLE_BLOCK_SWAP, MANGLE_CHUNK_SHUFFLE, MANGLE_TLV_MUTATE, MANGLE_TOKEN_SHUFFLE |
| MUT_CAT_ASCII_NUM | HF_DISABLE_MUT_ASCII_NUM | MANGLE_ASCII_NUM, MANGLE_ASCII_NUM_CHANGE |
| MUT_CAT_SPLICE | HF_DISABLE_MUT_SPLICE | MANGLE_SPLICE, MANGLE_CROSS_OVER |
| MUT_CAT_CMPLOG | HF_DISABLE_MUT_CMPLOG | MANGLE_CMP_SOLVE, MANGLE_GRADIENT_CMP |

## 2. Mutator 调度策略 (MOpt)

### 2.1 分层调度 (Tier-based Scheduling)

核心调度函数 `mangle_pickWeighted` (mangle.c:1932-1984) 实现了分层的 mutator 选择策略：

```
┌─────────────────────────────────────────────────────────────────┐
│                    Mutator 分层架构                              │
├─────────────────────────────────────────────────────────────────┤
│  TIER_DATA (40%)    │ 数据导向变异：字典、魔数、CMP求解          │
│  TIER_ARITH (25%)   │ 算术变异：位翻转、字节加减、算术运算        │
│  TIER_SPLICE (20%)  │ 拼接变异：splice、crossover               │
│  TIER_OTHER (15%)   │ 其他变异：随机选择任意 mutator             │
└─────────────────────────────────────────────────────────────────┘
```

**各层包含的 Mutator (mangle.c:1865-1896)：**

- **tierData**: INTERESTING_VALUES, MAGIC, STATIC_DICT, CONST_FEEDBACK_DICT, CMP_SOLVE, SPECIAL_STRINGS, GRADIENT_CMP, ARITH_CONST, DICT_INSERT, PUNCTUATION
- **tierArith**: BIT, INC_BYTE, DEC_BYTE, NEG_BYTE, ADD_SUB, ARITH8
- **tierSplice**: SPLICE, CROSS_OVER
- **tierStructure**: CHUNK_SHUFFLE, BLOCK_REPEAT, BLOCK_SWAP, BLOCK_MOVE, TLV_MUTATE, TOKEN_SHUFFLE

### 2.2 自适应权重调整 (Adaptive Weighting)

权重会根据各层的成功率动态调整 (mangle.c:1937-1959)：

```c
// 基础权重
uint8_t w[4] = {40, 25, 20, 15};  // DATA, ARITH, SPLICE, OTHER

// 自适应调整逻辑
for (int i = 0; i < 4; i++) {
    uint64_t tries = run->global->mutate.stats[i].tries;
    if (tries < 500) continue;  // 样本不足，跳过

    uint64_t hits = run->global->mutate.stats[i].successes;
    uint64_t rate = (hits * 10000) / tries;  // 万分比

    if (rate > 50)        // > 0.5% 成功率 - 非常好
        w[i] = MIN(w[i] + 15, 90);
    else if (rate > 10)   // > 0.1% 成功率
        w[i] = MIN(w[i] + 5, 70);
    else if (rate < 1)    // < 0.01% 成功率
        w[i] = MAX(w[i] / 2, 5);
}
```

**成功率追踪 (honggfuzz.h:305-308)：**

```c
struct {
    uint64_t tries;     // 该层被使用的次数
    uint64_t successes; // 该层导致新覆盖的次数
} stats[4];             // 0=data, 1=arith, 2=splice, 3=other
```

### 2.3 停滞应对策略 (Stagnation Handling)

当覆盖率停滞时，调度器会采取不同策略 (mangle.c:2047-2097)：

| 停滞时间 | 阈值 | 策略 |
|----------|------|------|
| timeStagnated | 10秒 | 33% 概率执行 CMP_SOLVE，50% 概率执行 SPLICE，25% 概率执行 GRADIENT_CMP |
| timeStuck | 60秒 | 33% 概率执行 CROSS_OVER |
| timeGivenUp | 300秒 | 12.5% 概率执行结构性变异 |
| 2×timeGivenUp | 600秒 | 6.25% 概率进入 HAVOC 模式 |

**停滞时的变异次数调整：**

```c
// 基础变异次数为 mutationsPerRun
uint8_t mult = 1, cap = 16, min = 1;

if (stagnation > 300s) {      // timeGivenUp
    mult = 4; cap = 64; min = 2;
} else if (stagnation > 60s) { // timeStuck
    mult = 2; cap = 32;
}

uint64_t count = rndGet(min, MIN(base * mult, cap));
```

**停滞时的数据层强化 (mangle.c:2108-2111)：**

当停滞超过30秒时，25% 的概率强制选择 tierData 层的 mutator。

## 3. Power Schedule (能量调度)

Power Schedule 控制每个种子被选中的频率，定义在 `power.c` 中。

### 3.1 基础能量

```c
#define POWER_BASE_ENERGY 256  // 基础能量，能量为256时种子被选中1次
```

### 3.2 能量影响因素

| 因素 | 调整规则 | 可配置环境变量 |
|------|----------|----------------|
| **新覆盖边** | 每个新边 boost 2倍，最多8个新边 | HFUZZ_PREFER_HIGHER_COVERAGE |
| **覆盖密度** | 密度>50%: 1.5倍; >200%: 2倍 | HFUZZ_PREFER_HIGHER_COVERAGE |
| **执行速度** | 根据相对速度调整 1/16 到 16 倍 | HFUZZ_PREFER_FASTER_SEEDS |
| **生育率** | 有子代的种子获得 log2(refs) 的 boost | - |
| **新鲜度** | <60秒: 4倍; <5分钟: 2倍; >1小时无子代: 0.5倍 | - |
| **种子大小** | >1KB: 按 log2(size) 降低能量 | HFUZZ_PREFER_SHORTER_SEEDS |
| **堆栈深度** | >16KB: 按深度 boost | - |
| **路径多样性** | 有唯一路径: 1.25倍 | - |
| **CMP进度** | 有进度: 最多1.25倍 | - |
| **稀有边** | 命中稀有边: 最多2倍 | HFUZZ_PREFER_HIGHER_COVERAGE |
| **选中次数** | >100次: 按 log2 降低 | - |
| **变异深度** | >8代: 按 log2 降低 | - |
| **熵值** | 高熵/低熵: 0.5倍; 中等熵: 1.5倍 | - |
| **超时** | 超时种子: 1/32倍 | - |

### 3.3 阶段感知

在 dry-run 阶段，小种子 (<256字节) 获得 1.5 倍 boost 以加速初始探索。

## 4. Mutator Sanitization — `mangle_sanitize` 函数 (mangle.c:1902-1930)

### 4.1 作用

`mangle_sanitize` 是调度管线中的**安全阀**。它在 mutator 被选中之后、实际执行之前介入，确保所选 mutator 的运行时依赖已满足。如果依赖缺失，则将其替换为功能相近但无外部依赖的回退 mutator，避免空操作或无意义的变异。

该函数在两个关键路径上被调用：
1. `mangle_pickWeighted` 的返回值经过 sanitize（mangle.c:2000）—— 主循环中每次选择 mutator 都会经过
2. `mangle_dispatch` 内部调用 sanitize（mangle.c:2044）—— 停滞应对策略中直接指定的 mutator 也会经过

### 4.2 依赖检查机制

函数内部维护了一个静态的需求-回退映射表，使用位标志 `needs` 编码三种依赖：

| 位标志 | 含义 | 检查条件 |
|--------|------|----------|
| bit 0 (0x1) | 需要用户字典 | `run->global->mutate.dictionaryCnt == 0` 时不满足 |
| bit 1 (0x2) | 需要 CMP 反馈 | `!run->global->feedback.cmpFeedback` 时不满足 |
| bit 2 (0x4) | 需要动态文件方法 | `dynFileMethod == _HF_DYNFILE_NONE` 时不满足 |

对于 `needs == 0` 的 mutator（大多数），函数直接返回原值，零开销。

### 4.3 回退映射表

| Mutator | needs | 依赖 | 回退 Mutator | 回退逻辑 |
|---------|-------|------|--------------|----------|
| MANGLE_STATIC_DICT | 0x1 | 字典 | MANGLE_MAGIC | 无字典时用魔数值替代 |
| MANGLE_CONST_FEEDBACK_DICT | 0x2 | CMP反馈 | MANGLE_MAGIC | 无CMP反馈时用魔数值替代 |
| MANGLE_CMP_SOLVE | 0x2 | CMP反馈 | MANGLE_MAGIC | 无CMP反馈时用魔数值替代 |
| MANGLE_GRADIENT_CMP | 0x2 | CMP反馈 | MANGLE_MAGIC | 无CMP反馈时用魔数值替代 |
| MANGLE_ARITH_CONST | 0x2 | CMP反馈 | MANGLE_ADD_SUB | 无CMP反馈时用普通算术替代 |
| MANGLE_SPLICE | 0x4 | 动态文件 | MANGLE_RANDOM_BUF | 无语料库时用随机数据替代 |
| MANGLE_CROSS_OVER | 0x4 | 动态文件 | MANGLE_BYTES | 无语料库时用随机字节替代 |
| MANGLE_DICT_INSERT | 0x1 | 字典 | MANGLE_PUNCTUATION | 无字典时用标点符号替代 |

### 4.4 与禁用 MOpt 的关系

当 `HF_DISABLE_MOPT=1` 禁用 MOpt 自适应调度时，`mangle_pickWeighted` 使用均匀随机选择 mutator。此时 `mangle_sanitize` 的作用更加关键——因为均匀随机可能选中任何 mutator（包括需要字典或 CMP 反馈的），sanitize 确保这些选择在运行时条件不满足时能安全回退，不会产生无效变异。

### 4.5 额外的越界保护

函数开头还有一层防御性检查：如果传入的枚举值超出 `MANGLE_COUNT` 范围，会随机选择一个合法的 mutator 而非触发未定义行为：

```c
if ((unsigned)m >= MANGLE_COUNT) {
    return (mangle_t)util_rndGet(0, MANGLE_COUNT - 1);
}
```

## 5. HAVOC 模式 (mangle.c:1778-1856)

HAVOC 模式是极端停滞时的逃逸策略，一次执行 16-128 个随机变异。

包含的 mutator（均匀随机选择）：
- 基础变异：BIT, INC_BYTE, DEC_BYTE, NEG_BYTE, BYTES
- 魔数变异：MAGIC
- 算术变异：ADD_SUB, ARITH8
- 内存变异：MEM_SET, MEM_SWAP, MEM_COPY
- 结构变异：EXPAND, SHRINK, BLOCK_MOVE
- 字节变异：BYTE_REPEAT, RANDOM_BUF
- 字典变异：STATIC_DICT, CONST_FEEDBACK_DICT, DICT_INSERT
- 拼接变异：SPLICE, CROSS_OVER
- CMP变异：CMP_SOLVE

## 6. 关键代码位置

| 文件 | 行号 | 功能 |
|------|------|------|
| mangle.c | 46-58 | Mutator 类别定义 |
| mangle.c | 60-98 | 类别禁用检查 |
| mangle.c | 105-143 | Mutator 枚举定义 |
| mangle.c | 1862-1896 | 分层数组定义 |
| mangle.c | 1932-1984 | mangle_pickWeighted (MOpt核心) |
| mangle.c | 1987-2028 | Mutator 分发表和执行 |
| mangle.c | 2030-2117 | mangle_mangleContent (主入口) |
| mangle.c | 1778-1856 | HAVOC 模式 |
| power.c | 88-288 | power_calculateEnergy (Power Schedule) |
| honggfuzz.h | 305-308 | 变异统计结构 |

## 7. 可能的修改方案

### 7.1 禁用 MOpt 策略

可以添加环境变量 `HF_DISABLE_MOPT` 来禁用自适应权重调整，回退到均匀随机选择：

```c
// 在 mangle_pickWeighted 中添加检查
static bool moptDisabled = false;
static bool moptChecked = false;
if (!moptChecked) {
    moptChecked = true;
    if (getenv("HF_DISABLE_MOPT")) {
        moptDisabled = true;
        LOG_I("MOpt adaptive scheduling disabled via HF_DISABLE_MOPT");
    }
}

if (moptDisabled) {
    // 使用均匀随机选择
    return (mangle_t)util_rndGet(0, MANGLE_COUNT - 1);
}
```

### 7.2 配置调度层权重

可以添加环境变量配置各层的基础权重：

- `HF_MOPT_WEIGHT_DATA` - 数据层权重 (默认40)
- `HF_MOPT_WEIGHT_ARITH` - 算术层权重 (默认25)
- `HF_MOPT_WEIGHT_SPLICE` - 拼接层权重 (默认20)
- `HF_MOPT_WEIGHT_OTHER` - 其他层权重 (默认15)

### 7.3 禁用停滞应对策略

添加 `HF_DISABLE_STAGNATION_BOOST` 禁用停滞时的额外变异。
