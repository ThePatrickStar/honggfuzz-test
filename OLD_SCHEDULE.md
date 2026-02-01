# Honggfuzz Seed Scheduling Analysis

针对执行调度算法，增加更多配置。针对以下方面：执行速度，

## Overview

Honggfuzz uses a sophisticated seed scheduling mechanism that balances exploration and exploitation. The scheduler maintains a dynamic queue of seeds and selects them based on multiple factors including execution speed, coverage, age, and size.

## Core Data Structures

### dynfile_t (honggfuzz.h:147-159)
Represents a single seed/input file in the dynamic corpus:
- `size`: Size of the input in bytes
- `cov[4]`: Coverage metrics (4 different coverage dimensions)
- `idx`: Index/position in the queue
- `timeExecUSecs`: Execution time in microseconds
- `path`: File path
- `src`: Pointer to parent seed (for tracking lineage)
- `refs`: Reference count (how many times this seed was used as parent)
- `phase`: Current fuzzing phase
- `data`: Actual input data

### Dynamic Queue (honggfuzz.h:218)
The dynamic corpus is managed as a tail queue:
```c
TAILQ_HEAD(dyns_t, _dynfile_t) dynfileq;
```
- Seeds are stored in a doubly-linked list
- `dynfileqCurrent`: Pointer tracking current position for round-robin iteration
- `dynfileqCnt`: Total number of seeds in queue
- `dynfileqMaxSz`: Maximum queue size

## Seed Scheduling Algorithm

### Main Selection Logic: input_prepareDynamicInput() (input.c:537-586)

This is the core function that selects which seed to fuzz next. The algorithm works as follows:

1. **Round-robin iteration**: Maintains `dynfileqCurrent` pointer that moves through the queue sequentially
2. **Probabilistic skipping**: Each seed is evaluated with `input_skipFactor()` to determine if it should be skipped
3. **Try counter**: Seeds with negative skip_factor get multiple consecutive tries (`triesLeft`)

**Key logic** (input.c:558-566):
```c
int skip_factor = input_skipFactor(run, run->current, &speed_factor);
if (skip_factor <= 0) {
    run->triesLeft = -(skip_factor);  // Favored seeds get multiple tries
    break;
}

if ((util_rnd64() % skip_factor) == 0) {  // Probabilistic skip
    break;
}
```

**Interpretation**:
- If `skip_factor <= 0`: The seed is favored and gets `-skip_factor` consecutive executions
- If `skip_factor > 0`: The seed is selected with probability `1/skip_factor` (higher penalty = lower selection probability)

### Penalty Calculation: input_skipFactor() (input.c:470-535)

This function calculates a penalty score for each seed based on multiple factors. Higher penalty = lower selection probability.

#### Factor 1: Execution Speed (input.c:473-478)
```c
*speed_factor = HF_CAP(input_speedFactor(run, dynfile), -10, 5);
penalty += *speed_factor;
```
- **Purpose**: Favor faster-executing seeds to maximize throughput
- **Calculation**: Compares seed's execution time against average execution time
- **Range**: Capped between -10 (very fast) and +5 (very slow)
- **Effect**: Slower seeds get higher penalty and are selected less frequently

#### Factor 2: Age/Position in Queue (input.c:499-516)
```c
static const int scaleMap[200] = {
    [98 ... 199] = -20,  // Newest seeds (top 2%)
    [91 ... 97]  = -2,   // Very new seeds
    [81 ... 90]  = -1,   // New seeds
    [71 ... 80]  = 0,    // Neutral
    [41 ... 70]  = 1,    // Older seeds
    [0 ... 40]   = 2,    // Oldest seeds (bottom 40%)
};
const unsigned percentile = (dynfile->idx * 100) / run->global->io.dynfileqCnt;
penalty += scaleMap[percentile];
```
- **Purpose**: Favor newer seeds that were recently added to the corpus
- **Calculation**: Based on seed's position (idx) in the queue as a percentile
- **Range**: -20 (newest 2%) to +2 (oldest 40%)
- **Effect**: Older seeds are gradually deprioritized, newer seeds get more attention

#### Factor 3: Reference Count (input.c:518-523)
```c
penalty += HF_CAP((2 - (int)dynfile->refs), -10, 2);
```
- **Purpose**: Favor seeds that have successfully produced other interesting seeds
- **Calculation**: `2 - refs` (capped between -10 and 2)
- **Effect**:
  - Seeds with 0 refs: penalty = +2 (less likely to be selected)
  - Seeds with 1 ref: penalty = +1
  - Seeds with 2 refs: penalty = 0
  - Seeds with 3+ refs: penalty = -1 to -10 (more likely to be selected)
- **Rationale**: Seeds that have been productive parents should be explored more

#### Factor 4: Input Size (input.c:525-532)
```c
if (dynfile->size > 0) {
    penalty += HF_CAP(((int)util_Log2(dynfile->size) - 10), -5, 5);
}
```
- **Purpose**: Favor smaller inputs for faster execution and easier mutation
- **Calculation**: `log2(size) - 10` (capped between -5 and 5)
- **Effect**:
  - 1KB input (2^10): penalty = 0
  - 512B input (2^9): penalty = -1 (favored)
  - 2KB input (2^11): penalty = +1 (penalized)
  - 32KB input (2^15): penalty = +5 (heavily penalized)
- **Rationale**: Smaller inputs execute faster and are easier to mutate effectively

## Queue Management

### Adding Seeds: input_addDynamicInput() (input.c:400-410)

When a new seed is discovered (e.g., finds new coverage), it's added to the dynamic queue:

```c
dynfile_t* iter = NULL;
TAILQ_FOREACH_HF (iter, &run->global->io.dynfileq, pointers) {
    if (input_cmpCov(dynfile, iter)) {
        TAILQ_INSERT_BEFORE(iter, dynfile, pointers);
        break;
    }
}
if (iter == NULL) {
    TAILQ_INSERT_TAIL(&run->global->io.dynfileq, dynfile, pointers);
}
```

**Key behavior**:
- Seeds are **sorted by coverage** when inserted
- Better coverage seeds are placed earlier in the queue
- This ensures high-value seeds are encountered sooner during round-robin iteration

## Summary: Scheduling Strategy

Honggfuzz's seed scheduling combines multiple strategies:

### 1. Coverage-Ordered Queue
- Seeds are sorted by coverage when added
- Better coverage seeds appear earlier in round-robin iteration
- Ensures high-quality seeds are encountered more frequently

### 2. Multi-Factor Penalty System
The scheduler balances four competing objectives:
- **Throughput**: Favor fast-executing seeds (speed penalty)
- **Freshness**: Favor recently-added seeds (age penalty)
- **Productivity**: Favor seeds that have produced interesting children (reference penalty)
- **Efficiency**: Favor smaller seeds (size penalty)

### 3. Probabilistic Selection
- Instead of strict prioritization, uses probabilistic skipping
- Seeds with high penalty are skipped with higher probability
- Seeds with negative penalty get multiple consecutive tries
- This provides exploration (all seeds eventually tested) while biasing toward exploitation (good seeds tested more)

### 4. Key Characteristics

**Advantages of this approach**:
- **Adaptive**: Automatically adjusts to seed execution times
- **Balanced**: No single factor dominates; multiple objectives are considered
- **Exploration-friendly**: Even low-priority seeds eventually get tested
- **Throughput-optimized**: Strong bias toward fast-executing seeds

**Potential weaknesses**:
- Age penalty may cause older seeds to be under-explored even if they have potential
- Size penalty may discourage exploration of larger inputs that could reach deep code paths
- The probabilistic nature means some high-value seeds might be skipped by chance

## Implications for Fuzzer Comparison

When comparing Honggfuzz against other fuzzers, the scheduling strategy can be analyzed through:

### Metrics to Extract from Fuzzerlog
1. **Seed selection frequency**: How often each seed is chosen (from CHANCES records)
2. **Seed characteristics**: Size, execution time, coverage of selected seeds
3. **Temporal patterns**: When are high-coverage seeds selected vs low-coverage seeds
4. **Parent-child relationships**: Which seeds produce the most interesting children (from NEW_SEED records)

### Key Questions for Analysis
1. **Does the speed penalty help or hurt?**
   - Compare execution throughput between fuzzers
   - Check if fast seeds lead to coverage gains or just repeated exploration

2. **Does the age penalty cause missed opportunities?**
   - Analyze if critical basic blocks are covered by old seeds that get deprioritized
   - Check if newer fuzzers without age penalty explore old seeds more thoroughly

3. **Does the size penalty limit deep path exploration?**
   - Compare average seed sizes between fuzzers
   - Check if critical basic blocks require larger inputs that Honggfuzz deprioritizes

4. **Does the reference-based scheduling create productive lineages?**
   - Trace seed genealogy to see if high-ref seeds actually produce better children
   - Compare against fuzzers without reference-based scheduling

## Code References

For detailed implementation, refer to these key functions:

### Core Scheduling Functions
- `input_prepareDynamicInput()` - input.c:537-586
  - Main seed selection logic
  - Implements round-robin with probabilistic skipping

- `input_skipFactor()` - input.c:470-535
  - Calculates penalty score based on 4 factors
  - Returns negative value for favored seeds, positive for penalized seeds

- `input_speedFactor()` - input.c:452-468
  - Calculates speed penalty by comparing against average execution time

### Queue Management Functions
- `input_addDynamicInput()` - input.c:~400-438
  - Adds new seed to queue, sorted by coverage
  - Logs new seed to fuzzerlog

- `input_cmpCov()` - input.c (referenced but not shown in excerpts)
  - Compares coverage between two seeds for sorting

### Data Structures
- `dynfile_t` - honggfuzz.h:147-159
  - Seed representation with coverage, size, execution time, etc.

- `honggfuzz_t.io.dynfileq` - honggfuzz.h:218
  - Tail queue storing all dynamic seeds

