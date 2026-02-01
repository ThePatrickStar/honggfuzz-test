# Honggfuzz Fuzzing Phases

Honggfuzz 有两条主路径：**Static 模式**和 **Dynamic（反馈驱动）模式**，后者包含三个阶段。

## 状态枚举

定义在 `honggfuzz.h:136-142`：

```
_HF_STATE_UNSET            // 初始未设置
_HF_STATE_STATIC           // 静态模式（无反馈）
_HF_STATE_DYNAMIC_DRY_RUN  // 动态模式 Phase 1
_HF_STATE_DYNAMIC_MAIN     // 动态模式 Phase 3
_HF_STATE_DYNAMIC_MINIMIZE // 动态模式 Phase 3（最小化变体）
```

## 入口判断 (`fuzz_threadsStart`, fuzz.c:709)

```
if socketFuzzer       → 直接进入 DYNAMIC_MAIN
elif dynFileMethod != NONE → 进入 DYNAMIC_DRY_RUN（Phase 1）
else                       → 进入 STATIC
```

## Static 模式

不使用 coverage 反馈。每次迭代从输入目录读取静态文件，施加变异后执行，不根据 coverage 结果调整种子选择。

## Dynamic 模式（三阶段）

### Phase 1/3: Dry Run (`_HF_STATE_DYNAMIC_DRY_RUN`)

- 逐个执行输入目录中的初始种子（**不做变异**，`mutationsPerRun=0`）
- 目的：收集每个初始种子的 coverage 基线，建立动态语料库
- 所有初始文件跑完后，等待全部线程就绪，进入下一阶段

### Phase 2/3: 切换过渡 (`fuzz_setDynamicMainState`, fuzz.c:113)

不是独立状态，而是从 Dry Run 到 Main 的过渡逻辑：
- 等待所有线程都完成 Dry Run（通过原子计数器同步）
- 如果语料库为空，插入一个空种子防止后续阶段失败
- 根据语料库中最大种子大小，动态调整 `maxInputSz`（加 25% 余量）
- 如果启用了 `--minimize`，则转入 MINIMIZE 而非 MAIN

### Phase 3/3: Dynamic Main (`_HF_STATE_DYNAMIC_MAIN`)

核心 fuzzing 阶段：
- `input_prepareDynamicInput()` 从动态语料库中选种子
- `power_calculateEnergy()` 计算能量，决定每个种子被 fuzz 的概率
- 对选中种子施加变异（`mangle_mangleContent`）
- 执行后通过 `fuzz_perfFeedback()` 收集 coverage，发现新覆盖则加入语料库

### Phase 3/3 变体: Corpus Minimization (`_HF_STATE_DYNAMIC_MINIMIZE`)

仅在 `--minimize` 模式下触发，替代 DYNAMIC_MAIN：
- 遍历输入目录，删除未被动态语料库保留的文件
- 完成后终止 fuzzing

## 每轮迭代流程 (`fuzz_fuzzLoop`, fuzz.c:508)

```
初始化 run 状态
  → fuzz_fetchInput()        // 根据当前 phase 获取/选择输入
    → subproc_Run()          // 执行目标程序
      → fuzz_perfFeedback()  // 收集 coverage 反馈，决定是否加入语料库
        → report_saveReport() // 保存崩溃报告
```

## Persistent 模式输入传递

### 架构

```
Fuzzer                                    Target
dynfile->data ─── 共享内存 (8MiB) ──────→ inputFile (mmap只读, FD 1021)
persistentSock ←─ Unix Socket ─────────→ FD 1023
```

### 流程

1. **初始化** (`fuzz_threadNew`): 创建共享内存，fork 时 `dup2` 到固定 FD
2. **目标启动**: `libhfuzz/fetch.c` 的 constructor 自动 mmap FD 1021
3. **每轮迭代**:
   - Target 发送 `'R'` 表示就绪
   - Fuzzer 写入种子到共享内存，通过 socket 发送大小 (uint64)
   - Target 直接从共享内存读取，执行 `LLVMFuzzerTestOneInput()`
   - 循环

**特点**: 零拷贝，socket 仅传就绪信号和大小；目标进程不退出，持续循环。
