# Honggfuzz Seed Scheduling Analysis

## Overview

Honggfuzz uses a sophisticated power scheduling algorithm to prioritize which seeds (test inputs) to select for mutation. The scheduling system combines multiple factors to calculate an "energy" score for each seed, which determines how frequently it will be selected.

## Core Architecture

### Data Structures

**Seed Queue (`dynfileq`)**:
- Implemented as a tail queue (TAILQ) sorted by coverage
- Seeds with better coverage are placed earlier in the queue
- Multiple queue pointers for different selection strategies:
  - `dynfileqCurrent`: Main selection pointer
  - `dynfileq2Current`: Random selection pointer (for splicing)
  - `dynfileqDiverseCurrent`: Diversity-based selection pointer

**Seed Metadata (`dynfile_t`)**:
Each seed tracks:
- `size`: Input size in bytes
- `cov[4]`: Coverage metrics (4 dimensions)
- `timeExecUSecs`: Execution time in microseconds
- `timeAdded`: Timestamp when added to corpus
- `newEdges`: Number of new edges discovered
- `depth`: Derivation depth (how many mutations from original)
- `stackDepth`: Maximum stack depth during execution
- `pathHash`: Unique execution path identifier
- `cmpProgress`: Progress on comparison instructions
- `rareEdgeCnt`: Count of rarely-hit edges
- `selectCnt`: Number of times selected for fuzzing
- `refs`: Reference count (number of children produced)
- `src`: Pointer to parent seed
- `timedout`: Whether this seed caused a timeout 
- `imported`: Whether this is an imported seed (not yet measured)

## Energy Calculation Algorithm

The `power_calculateEnergy()` function in `power.c:85` calculates an energy score for each seed. This score determines how many times the seed will be selected for mutation.

### Base Energy and Conversion

- **Base Energy**: `POWER_BASE_ENERGY` (constant, likely 256 or similar)
- **Energy Range**: Capped between 1 and 32768 (`energyMax`)
- **Selection Logic**:
  - If `energy >= POWER_BASE_ENERGY`: seed is selected `energy / POWER_BASE_ENERGY` times (capped at 256)
  - If `energy < POWER_BASE_ENERGY`: probabilistic skipping with factor `POWER_BASE_ENERGY / energy` (capped at 64)

### Energy Factors (in order of application)

#### 1. Phase-Aware Energy (power.c:94-101)
- **Dry-run phase**: Favors smaller inputs for quick exploration
  - Seeds < 256 bytes get 1.5x boost
- **Main phase**: No phase-specific adjustment

#### 2. Novelty - New Edge Discovery (power.c:103-114)
- Seeds that discovered new edges get exponential boost
- **Boost**: `2^(min(newEdges, 8) - decay)`
- **Decay**: Time-based decay after 10 minutes
  - `decay = min(age_minutes / 10, 6)`
- **Rationale**: Recent discoveries indicate promising search space

#### 3. Density - Coverage Efficiency (power.c:116-123)
- Measures coverage per byte: `(cov[0] * 100) / size`
- **Thresholds**:
  - `density > 50`: 1.5x boost (efficient inputs)
  - `density > 200`: 2x boost (very dense loops/code)
- **Rationale**: Compact inputs with high coverage are valuable

#### 4. Speed - Execution Time (power.c:125-135)
- Compares seed's execution time to global average
- **Formula**: `energy = (energy * speed_ratio) / 16`
  - `speed_ratio = (avg_usecs * 16) / exec_usecs` (capped 1-256)
- **Effect**: Faster seeds get up to 16x boost
- **Rationale**: Faster execution allows more mutations per second

#### 5. Fertility - Child Production (power.c:137-142)
- Seeds that produced children (via `refs` count) get logarithmic boost
- **Formula**: `energy = (energy * (8 + min(log2(refs+1), 8))) / 8`
- **Effect**: Up to 2x boost for fertile seeds
- **Rationale**: Successful parents indicate promising regions

#### 6. Freshness - Time-Based Priority (power.c:144-152)
- **Fresh** (< 60s): 4x boost
- **Recent** (< 5 min): 2x boost
- **Stale** (> 60 min, no children): 0.5x penalty
- **Rationale**: New seeds haven't been fully explored yet

#### 7. Size Penalty (power.c:154-158)
- Large inputs (> 1KB) get penalized
- **Formula**: `energy >>= min(log2(size) - 10, 4)`
- **Effect**: Up to 16x penalty for very large inputs
- **Rationale**: Smaller inputs are faster and easier to mutate

#### 8. Stack Depth - Complex Execution (power.c:160-170)
- Deep stack usage suggests complex logic or recursion
- **Threshold**: > 16KB stack depth
- **Formula**: `energy = (energy * min(log2(stackDepth/1024) - 2, 8)) / 2`
- **Effect**: Up to 4x boost for deep stacks
- **Rationale**: Deep execution paths may reach complex code

#### 9. Path Diversity (power.c:172-179)
- Seeds with unique execution paths get boost
- **Condition**: Early exploration (< 1000 unique paths)
- **Effect**: 1.25x boost
- **Rationale**: Diverse paths explore different program behaviors

#### 10. CMP Progress - Comparison Solving (power.c:181-187)
- Seeds making progress on comparison instructions
- **Formula**: `energy = (energy * (4 + min(cmpProgress/8, 4))) / 4`
- **Effect**: Up to 2x boost
- **Rationale**: Solving comparisons unlocks new code paths

#### 11. Rare Edge Bonus (power.c:189-193)
- Seeds hitting rarely-executed edges
- **Formula**: `energy = (energy * (8 + min(rareEdgeCnt, 8))) / 8`
- **Effect**: Up to 2x boost
- **Rationale**: Rare edges are harder to reach, valuable to explore

#### 12. Diminishing Returns (power.c:195-200)
- Seeds selected many times yield less value
- **Threshold**: > 100 selections
- **Formula**: `energy >>= min(log2(selectCnt/100), 3)`
- **Effect**: Up to 8x penalty for over-selected seeds
- **Rationale**: Avoid getting stuck on same seeds

#### 13. Depth Penalty - Over-Specialization (power.c:202-209)
- Deeply derived seeds may be over-specialized
- **Threshold**: > 8 derivation levels
- **Formula**: `energy >>= min(log2(depth - 7), 3)`
- **Effect**: Up to 8x penalty for deep derivations
- **Rationale**: Balance exploration vs exploitation

#### 14. Stagnation Handling (power.c:211-222)
- When no new coverage for > 60s, focus on best seeds
- **High coverage** (≥80% of max): 4x boost
- **Low coverage** (<10% of max): 4x penalty
- **Rationale**: Intensify search in promising areas when stuck

#### 15. Entropy - Data Structure Quality (power.c:224-234)
- Analyzes byte distribution using Shannon entropy approximation
- **High entropy** (>93): 2x penalty (random/compressed data)
- **Very low entropy** (<25): 2x penalty (sparse/zeros)
- **Medium entropy** (<62): 1.5x boost (structured/text data)
- **Rationale**: Structured data is easier to mutate effectively

#### 16. Timeout Penalty (power.c:236-239)
- Seeds that caused timeouts get severe penalty
- **Effect**: 32x penalty (`energy >>= 5`)
- **Rationale**: Avoid wasting time on slow/hanging inputs

## Seed Selection Logic

The `input_prepareDynamicInput()` function in `input.c:471` implements the actual seed selection algorithm.

### Selection Algorithm (input.c:482-527)

The selection process follows these steps:

1. **Queue Traversal**: Iterate through the sorted corpus queue
2. **Skip Imported Seeds**: Imported seeds (not yet measured) are always selected
3. **Energy Calculation**: Calculate energy for each candidate seed
4. **Lineage Bonus**: Apply 25% bonus if parent was fertile (refs > 2)
5. **Selection Decision**:
   - **High energy** (≥ BASE_ENERGY): Select and repeat `energy/BASE_ENERGY` times (capped at 256)
   - **Low energy** (< BASE_ENERGY): Probabilistic skip with factor `BASE_ENERGY/energy` (capped at 64)

### Key Implementation Details

**Repeat Mechanism** (input.c:487-514):
- `triesLeft` counter tracks remaining repeats for current seed
- High-energy seeds are selected multiple times before moving to next seed
- Ensures valuable seeds get more mutation attempts

**Probabilistic Skipping** (input.c:517-526):
- Low-energy seeds have `1/skip_factor` chance of selection
- Uses random number: `(util_rnd64() % skip_factor) == 0`
- Maximum skip factor of 64 prevents complete starvation

**Selection Count Tracking** (input.c:533-535):
- Each selection increments `selectCnt` for the seed
- Used by diminishing returns factor in energy calculation
- Prevents over-exploitation of same seeds

## Coverage-Based Sorting

### Insertion Algorithm (input.c:418-428)

When a new seed is added via `input_addDynamicInput()`:

1. **Coverage Comparison**: Uses `input_cmpCov()` to compare 4-dimensional coverage vectors
2. **Sorted Insertion**: Seeds with better coverage are inserted earlier in queue
3. **Lexicographic Ordering**: Compares `cov[0]`, then `cov[1]`, then `cov[2]`, then `cov[3]`

### Coverage Comparison Function (input.c:360-371)

```c
static bool input_cmpCov(dynfile_t* item1, dynfile_t* item2) {
    for (size_t j = 0; j < 4; j++) {
        if (item1->cov[j] > item2->cov[j]) return true;
        if (item1->cov[j] < item2->cov[j]) return false;
    }
    return false; // Equal coverage
}
```

**Effect**: Seeds are naturally ordered by coverage quality, with best seeds at the front.

## Special Selection Strategies

### Random Selection for Splicing (input.c:727-757)

The `input_getRandomInputAsBuf()` function provides a separate selection mechanism for crossover/splicing operations:

- Uses separate queue pointer: `dynfileq2Current`
- Simple round-robin traversal (no energy calculation)
- Returns random seed from corpus for splicing with current seed

### Diverse Selection for Crossover (input.c:763-825)

The `input_getDiverseInputAsBuf()` function selects seeds that are maximally different from the current seed:

**Diversity Metrics**:
1. **Coverage Difference**: `|iter->cov[0] - current_cov|`
2. **Lineage Difference**: 25% bonus if different parent lineage

**Algorithm**:
- Examines window of 16 seeds starting from `dynfileqDiverseCurrent`
- Selects seed with maximum diversity score
- Wraps around to beginning when reaching end of queue

**Rationale**: Crossover between diverse seeds explores new combinations.

### Imported Seed Handling (input.c:537-564)

Imported seeds (from external sources, not yet measured) receive special treatment:

- **Always selected**: No energy calculation, immediate selection
- **Removed after use**: Deleted from queue after first execution
- **No tracking**: `selectCnt` not incremented, `src` set to NULL
- **Queue pointer updates**: All three queue pointers updated to skip removed seed

**Rationale**: Quickly evaluate external inputs without biasing corpus statistics.

## Summary and Key Insights

### Multi-Factor Scheduling Strategy

Honggfuzz's scheduling algorithm combines **16 different factors** to calculate seed priority:

**Positive Factors (Boost Energy)**:
1. Novelty (new edges discovered)
2. Density (coverage per byte)
3. Speed (fast execution)
4. Fertility (produced children)
5. Freshness (recently added)
6. Stack depth (deep execution)
7. Path diversity (unique paths)
8. CMP progress (comparison solving)
9. Rare edges (hard-to-reach code)
10. Structured entropy (medium entropy)
11. High coverage during stagnation

**Negative Factors (Reduce Energy)**:
1. Large size (> 1KB)
2. Over-selection (diminishing returns)
3. Deep derivation (over-specialization)
4. Low coverage during stagnation
5. Extreme entropy (too high or too low)
6. Timeout (severe penalty)

