# Honggfuzz Mutator按类别禁用

当前的目标是,设置环境变量开关一类mutator。比如设置了某个特殊环境变量后，将会关闭这一类mutator的使用。

1. 针对每个特性的修改要单独放在一个commit中。
2. 必须在代码逻辑被禁用的时候，打印出信息提示。并且使用static的bool变量保证这个只打印一次，防止淹没输出。

### mutator分类

```
{
    # --- Bitflip ---
    "rnd_bitflip": "bitflip",
    "byte_flip": "bitflip",
    # --- Arithmetic ---
    "byte_inc": "arith",
    "byte_dec": "arith",
    "add_sub_with_range": "arith",
    "arith8": "arith",
    "arith_const": "arith",
    "add_sub_2": "arith",
    "add_sub_8": "arith",
    "add_sub_4": "arith",
    "add_sub_1": "arith",
    # --- Interesting / Magic values ---
    "magic_values": "interesting",
    "interesting_values": "interesting",
    "special_strings": "interesting",
    "punctuation": "interesting",
    # --- Dictionary / Extra ---
    "static_dict": "dict_extra",
    "dictionary_insert": "dict_extra",
    # --- Dynamic Dictionary (feedback) ---
    "dyn_dict": "dyn_dict",
    # --- Random bytes ---
    "rnd_byte": "random_bytes",
    "rnd_bytes": "random_bytes",
    "pure_rnd_bytes": "random_bytes",
    "rnd_memset": "random_bytes",
    "rnd_memclr": "random_bytes",
    # --- Structural byte-level mutations ---
    "rnd_memswap": "structural_bytes",
    "rnd_memmove": "structural_bytes",
    "rnd_memcopy": "structural_bytes",
    "byte_repeat": "structural_bytes",
    "block_repeat": "structural_bytes",
    "block_swap": "structural_bytes",
    "expand": "structural_bytes",
    "shrink": "structural_bytes",
    "chunk_shuffle": "structural_bytes",
    "tlv_mutate": "structural_bytes",
    "token_shuffle": "structural_bytes",
    # --- ASCII numbers ---
    "ascii_num": "ascii_num",
    "ascii_num_change": "ascii_num",
    # --- Splice ---
    "splice": "splice",
    "crossover": "splice",
    # cmplog
    "gradient_cmp": "cmplog",
}
```

### 实施方案

#### 1. 环境变量命名

禁用某类mutator的环境变量格式: `HF_DISABLE_MUT_<CATEGORY>`

| 类别 | 环境变量 |
|------|----------|
| bitflip | `HF_DISABLE_MUT_BITFLIP` |
| arith | `HF_DISABLE_MUT_ARITH` |
| interesting | `HF_DISABLE_MUT_INTERESTING` |
| dict_extra | `HF_DISABLE_MUT_DICT_EXTRA` |
| dyn_dict | `HF_DISABLE_MUT_DYN_DICT` |
| random_bytes | `HF_DISABLE_MUT_RANDOM_BYTES` |
| structural_bytes | `HF_DISABLE_MUT_STRUCTURAL_BYTES` |
| ascii_num | `HF_DISABLE_MUT_ASCII_NUM` |
| splice | `HF_DISABLE_MUT_SPLICE` |
| cmplog | `HF_DISABLE_MUT_CMPLOG` |

#### 2. 实现方式

在`mangle.c`中定义了:
- `mutator_category_t`枚举: 10个类别
- `mutCatEnvVars[]`: 环境变量名映射
- `mutCatDisabled[]`: 禁用状态数组
- `mangle_checkDisabledCategories()`: 检测函数(只执行一次)
- `CHECK_MUT_DISABLED(cat)`: 检测宏

#### 3. 使用示例

```c
static void mangle_Bit(run_t* run, bool printable) {
    CHECK_MUT_DISABLED(MUT_CAT_BITFLIP);
    // 原有逻辑...
}
```

#### 4. 测试

```bash
# 禁用bitflip类mutator
HF_DISABLE_MUT_BITFLIP=1 ./honggfuzz ...

# 禁用多个类别
HF_DISABLE_MUT_ARITH=1 HF_DISABLE_MUT_SPLICE=1 ./honggfuzz ...
```

启动时会打印: `Mutator category 'bitflip' disabled via HF_DISABLE_MUT_BITFLIP`
