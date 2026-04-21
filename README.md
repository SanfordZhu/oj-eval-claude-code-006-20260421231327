# Minesweeper-2025 Assignment

**ACMOJ Problem IDs**: 2876 (Basic), 2877 (Advanced)

## Table of Contents

- [Minesweeper-2025 Assignment](#minesweeper-2025-assignment)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
    - [Background](#background)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Terminology](#terminology)
    - [Tasks](#tasks)
      - [Basic Task (Server)](#basic-task-server)
      - [Advanced Task (Client)](#advanced-task-client)
    - [Data Constraints](#data-constraints)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
    - [2876 - Basic](#2876---basic)
    - [2877 - Advanced](#2877---advanced)
  - [Implementation Guidelines](#implementation-guidelines)
    - [Repository Structure](#repository-structure)
    - [Basic Task Implementation](#basic-task-implementation)
    - [Advanced Task Implementation](#advanced-task-implementation)
  - [Submission Requirements](#submission-requirements)
    - [Submission Guidelines](#submission-guidelines)
    - [Advanced Task Grading Criteria](#advanced-task-grading-criteria)
    - [Evaluation Notes](#evaluation-notes)
    - [Academic Integrity](#academic-integrity)


## Introduction

### Background

Minesweeper is a classic lightweight puzzle game that was built into older versions of Windows operating systems. For many people, it holds memories of entertainment on old computers during student days, idle time in computer classes, or relaxation during competition downtime.

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| Pass **2876** (Basic) | 45% |
| **2877** (Advanced) Performance | 35% |
| Code Review | 20% |

**Advanced Task Score Breakdown:**

| Achievement | Points |
| :--: | :--: |
| Rubbish to Baseline1 | 10% |
| Baseline1 to Baseline2 | 20% |
| Beat Baseline2 | 5% |

- Local sample data are provided to help with development and debugging. Passing local samples does not guarantee passing OJ tests.

- In the Code Review, we will **strictly examine your code style and repository organization structure, etc.**

## Assignment Requirements

In this assignment, you need to implement a Minesweeper game. If you're unfamiliar with Minesweeper, refer to [Minesweeper on Wikipedia](https://en.wikipedia.org/wiki/Minesweeper_(video_game)).

### Terminology

- **Grid and Map**: Each small square is a grid; all grids together form the map
- **Mine, Non-mine grid, Mine count**: For a non-mine grid, its mine count equals the number of adjacent mines in the 8 surrounding directions
- **Visit**: Changes a grid's state to visited
  - If already visited or marked, no effect occurs
  - **Like traditional Minesweeper, when you visit a grid with mine count 0, all surrounding grids are automatically visited**
  - This process continues recursively for any auto-visited grids that also have mine count 0
- **Mark**: Mark a grid as containing a mine
  - If already visited or marked, no effect occurs
  - **Unlike traditional Minesweeper, marking an already marked grid does NOT unmark it**
  - **Important: Marking a non-mine grid causes immediate game failure**
- **Auto Explore**: Similar to double-clicking in traditional Minesweeper
  - Can only target **visited non-mine grids** (grids showing numbers)
  - If the target grid is not a visited non-mine grid, the operation has no effect
  - If the number of marked mines around the target equals its mine count, all non-mine grids among the 8 surrounding grids are visited
- **Game End Conditions**:
  - Visiting a mine causes **immediate** failure
  - **Unlike traditional Minesweeper, marking a non-mine grid as a mine also causes immediate failure**
  - Victory occurs when all non-mine grids have been visited (**not required to mark all mines**)

### Tasks

#### Basic Task (Server)

Your basic task is to simulate a command-line interactive Minesweeper game (implementing the game server).

**Input Format:**
- First line: two integers `n` and `m` (map has `n` rows and `m` columns)
- Next `n` lines: each contains `m` characters (excluding newline)
  - Row `i`, column `j` (0-indexed) represents the grid at that position
  - `.` represents a non-mine grid
  - `X` represents a mine grid

**Example Input:**
```
3 3
.X.
...
..X
```
This represents a 3×3 map with mines at position (0,1) and (2,2).

**Initial State:**
All grids start as unvisited. After initialization, output the map (format described below).

**Operations:**
You will receive multiple lines of input, each containing three integers `x y type`:
- `x`, `y`: coordinates within ranges [0, n-1] and [0, m-1]
- `type`: operation type
  - `0` = Visit grid
  - `1` = Mark mine
  - `2` = Auto explore

**Output Format:**

After each operation, print the map with `n` rows and `m` columns, followed by a newline:

- **Unvisited grid**: `?`
- **Visited grid**: 
  - If mine: `X`
  - If non-mine: digit `k` (its mine count)
- **Marked grid**:
  - If mine: `@`
  - If non-mine: `X` (marking non-mine causes failure)

**Example:**

Initial state output:
```
???
???
???
```

After visiting (2,0), then (2,2):
```
???
12?
01X
```
(Game fails because a mine was visited)

After visiting (2,0), then marking (2,2) and (0,0):
```
X??
12?
01@
```
(Game fails because (0,0) is not a mine)

**Complete Example:**

After visiting (2,0):
```
???
12?
01?
```

If marking (1,2):
```
???
12X
01?
```
(Game fails - marked non-mine)

If marking (2,2) instead:
```
???
12?
01@
```

Then auto-exploring (2,1):
```
???
122
01@
```

**Victory Output:**
When all non-mine grids are visited (final output before victory), output `@` for **all mine grids regardless of marked status**:
```
1@1
122
01@
```

**Game End:**
When the game ends, output:
1. First line: `"YOU WIN!"` or `"GAME OVER!"`
2. Second line: `visit_count marked_mine_count` (space-separated)
   - `visit_count`: number of visited non-mine grids
   - `marked_mine_count`: number of correctly identified mines
     - If victory: total number of mines
     - If failure: number of correctly marked mines
3. Exit with return value 0

Example victory output:
```
YOU WIN!
7 2
```

#### Advanced Task (Client)

Your advanced task is to play the Minesweeper game you just designed (implementing the game client). This is similar to interactive problems in competitive programming, but uses your own game program for interaction instead of standard I/O.

**Important: You may only use cin/cout for input/output in this section.**

The game program is called the server code (`server.h`), and the playing program is the client code (`client.h`).

**Workflow:**
1. Map is read as input to the server code
2. Client code only knows map dimensions (rows and columns)
3. Client repeatedly issues `Execute(row, column, type)` commands
   - First command is provided by input data to prevent hitting a mine initially
   - Each `Execute` performs an operation and reads the resulting map

**Important: You can only call Execute once per Decide call**

The `Execute` function structure:
```cpp
Execute(row, column, type):
  VisitBlock(row, column) or MarkMine(row, column) or AutoExplore(row, column)
  ReadMap() from result of PrintMap()
```

You need to:
1. Read limited map information in the `ReadMap` function
2. Store it in global variables
3. Make next decision based on this information
4. Issue decision via `Execute`

Refer to `src/advanced.cpp` and `src/include/client.h` for code structure.

**Example Interaction:**

Input:
```
3 3
.X.
...
..X
2 0
```
(Last line `2 0` is the first decision made for you)

Interaction:
```
Decision          Result of PrintMap
2 0 0             ???
                  12?
                  01?
1 2 0             ???
                  122
                  01?
0 1 0             ?X?
                  122
                  01?
                  GAME OVER!
                  5 0
```

**Call Flow:**
1. `main()` calls `InitMap()` to initialize
2. Until game ends, repeat:
   - Call `Decide()` for your program to decide grid and operation
   - `Decide()` calls `Execute(row, column, type)`
   - `Execute` uses your server to refresh map and calls `ReadMap()`
   - `ReadMap()` reads new map from standard input

### Data Constraints

- Map dimensions: `1 < n, m ≤ 30`
- Mine count: `1 ≤ x < n*m`
- **Basic task**: operation sequence length ≤ 1000, all operations guaranteed valid
- **Advanced task**: first operation guaranteed not to hit a mine; all input guaranteed valid

## Test Data

Public samples for local testing are provided at:
- `/workspace/data/006/basic`
- `/workspace/data/006/advanced`

## Per-Testcase Resource Limits

### 2876 - Basic

- **Time Limit (per test case)**: 1000 ms
- **Memory Limit (per test case)**: 244 MiB
- **Disk Usage**: Disk access is not permitted

### 2877 - Advanced

- **Time Limit (per test case)**: 5000 ms (min), 30000 ms (max)
- **Memory Limit (per test case)**: 256 MiB
- **Disk Usage**: Disk access is not permitted

## Implementation Guidelines

### Repository Structure

```
├── CMakeLists.txt
├── README.md
├── src
│   ├── CMakeLists.txt
│   ├── advanced.cpp
│   ├── basic.cpp
│   └── include
│       ├── client.h
│       ├── generator.h
│       └── server.h
├── submit_acmoj/
│   └── acmoj_client.py
└── /workspace/data/006
    ├── advanced
    └── basic
```

### Basic Task Implementation

- Main file: `basic.cpp` (**do not modify**)
- Read and understand how it calls functions in `server.h`
- Implement functions marked `TODO` in `server.h`
- Test by running `basic.cpp`
- Public test cases in `/workspace/data/006/basic`:
  - Tests 11-13: old 2023 tests (visit-only operations)
  - Tests 1-2: all operation types
  - Test 7: robustness test
- Example: input `1.in` should produce output matching `1.out`

#### Advanced Task Implementation

- Main file: `advanced.cpp` (**do not modify**)
- Read and understand how it calls functions in `server.h` and `client.h`
- After completing basic task, implement functions marked `TODO` in `client.h`
- Test by running `advanced.cpp`
- Public test cases in `/workspace/data/006/advanced` (5 test points)
- **Important**: In `InitGame()`, initialize ALL custom global variables since `TestBatch()` calls it multiple times

## Submission Requirements

### Submission Guidelines

- **Basic task**: Submit `server.h` to ACMOJ problem 2876
- **Advanced task**: Submit `client.h` to ACMOJ problem 2877
- Programs must read from standard input and write to standard output
- Ensure your implementation meets time and memory limits
- Use C++

### Advanced Task Grading Criteria

**Scoring Formula:**

If at game end you found `p` mines and visited `q` grids out of `n×m` total grids:
```
Score = (p + q) / (n × m) × 100%
```

**Testing Method:**
- Generate 50 random maps per test group
- Test using your `client.h` with standard `server.h`
- Drop lowest 10% of scores
- Average remaining scores as final score for that test group
- **Warning: Do NOT spam identical random code submissions** - submissions are monitored

**Baseline Performance:**

| Baseline | Score | Strategy | Assignment Grade |
| :--: | :--: | :-- | :--: |
| **Rubbish** | 39,625 | Random clicking only | Minimum threshold |
| **Baseline1** | 100,449 | Basic reasoning: auto-explore obvious safe grids, mark obvious mines | 10% (beat Rubbish) |
| **Baseline2** | 136,481 | Strong reasoning: deduce all logically solvable situations | 30% (beat Baseline1) |
| **Beat Baseline2** | > 136,481 | Advanced strategies | 35% (full credit) |

**Score Calculation:**
- Beat a baseline → earn full points for that tier
- Between baselines → **at least linear interpolation**, with order preservation

**Example:**
- Baseline1: 70,000 points
- Baseline2: 80,000 points
- Your score: 75,000 points
- You earn: 10% (beat Baseline1) + 10% × (75,000 - 70,000) / (80,000 - 70,000) = 15%

**Baseline Strategies:**

1. **Rubbish Baseline** (39,625 points):
   - Weakest client, serves as minimum threshold
   - Below this = **0 points**
   
2. **Baseline1** (100,449 points):
   - Basic reasoning ability
   - Automatically auto-explores obvious safe grids
   - If a visited non-mine grid has `k` unknown neighbors and its number is `k`, automatically marks all neighbors as mines
   - **Weak reasoning** for situations requiring logic

3. **Baseline2** (136,481 points):
   - Strong reasoning ability
   - Can deduce all obvious situations
   - **Strong reasoning** for situations requiring logical deduction

**Provided Materials:**
- Map generator
- Partial test parameters (random seeds & map parameters)
- Some individual test cases for debugging

### Evaluation Notes

- The evaluator will test your program with private OJ data in addition to local samples
- Output must match the required format exactly
- Exceeding time or memory limits will result in the corresponding verdict
- For advanced task, focus on algorithmic correctness and strategic thinking

### Academic Integrity

If any violations are found during evaluation or code review (including but not limited to using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.