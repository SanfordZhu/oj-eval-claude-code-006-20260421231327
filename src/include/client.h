#ifndef CLIENT_H
#define CLIENT_H

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

extern int rows;
extern int columns;
extern int total_mines;

void Execute(int r, int c, int type);

/*
 * Client state
 * visible[i][j]:
 *   -2 unknown
 *   -1 marked mine
 *   0..8 visited non-mine with given number
 */
static const int kCMaxN = 35;
static int visible[kCMaxN][kCMaxN];
static int known_mines;       // number of cells we've marked as mine
static int known_safe_visited;  // number of non-mine cells already visited
static int pending_rows, pending_cols;

// Queues / flags for next decision
static std::vector<std::pair<int, int>> safe_queue;   // cells known to be safe but not yet visited
static std::vector<std::pair<int, int>> mine_queue;   // cells known to be mines but not yet marked

static inline bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void InitGame() {
  known_mines = 0;
  known_safe_visited = 0;
  safe_queue.clear();
  mine_queue.clear();
  for (int i = 0; i < kCMaxN; ++i) {
    for (int j = 0; j < kCMaxN; ++j) {
      visible[i][j] = -2;
    }
  }
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

void ReadMap() {
  for (int i = 0; i < rows; ++i) {
    std::string s;
    std::cin >> s;
    for (int j = 0; j < columns; ++j) {
      char ch = s[j];
      if (ch == '?') {
        // remains unknown (or was previously known?)
        // Keep previous knowledge (shouldn't revert)
        // but if we haven't yet set it, leave as -2
        if (visible[i][j] == -2) {
          // still unknown
        }
      } else if (ch == '@') {
        visible[i][j] = -1;
      } else if (ch == 'X') {
        // This means a mine was visited or non-mine was marked -- loss state.
        // We'll just mark it as -1 to be safe.
        visible[i][j] = -1;
      } else if (ch >= '0' && ch <= '8') {
        visible[i][j] = ch - '0';
      }
    }
  }
}

// Get neighbors
static inline void Neighbors(int r, int c, std::vector<std::pair<int, int>> &out) {
  out.clear();
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (CInBounds(nr, nc)) out.emplace_back(nr, nc);
    }
  }
}

// Basic deduction: for each visited number cell, look at neighbors.
// - If (number - #marked_mines_around) == #unknown_around => all unknowns are mines.
// - If (number - #marked_mines_around) == 0 and unknowns > 0 => all unknowns are safe.
// Returns true if any new info was derived.
static bool BasicDeduce() {
  bool changed = false;
  std::vector<std::pair<int, int>> nbrs;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] < 0) continue;
      int num = visible[i][j];
      int unknown_cnt = 0, mine_cnt = 0;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) {
        if (visible[p.first][p.second] == -2) ++unknown_cnt;
        else if (visible[p.first][p.second] == -1) ++mine_cnt;
      }
      if (unknown_cnt == 0) continue;
      int remaining = num - mine_cnt;
      if (remaining == unknown_cnt) {
        // all unknowns are mines
        for (auto &p : nbrs) {
          if (visible[p.first][p.second] == -2) {
            visible[p.first][p.second] = -1;  // predicted mine
            mine_queue.emplace_back(p.first, p.second);
            changed = true;
          }
        }
      } else if (remaining == 0) {
        // all unknowns are safe
        for (auto &p : nbrs) {
          if (visible[p.first][p.second] == -2) {
            // mark as safe by adding to safe queue, but keep visible as -2
            // so we don't double-count. We use a separate tracker.
            // Actually we should avoid adding duplicates. Use a flag.
            // Simpler: set visible to a sentinel after pushing? No, we need
            // to keep it -2 until actually visited. Use a visited-in-queue set.
            // Instead, mark as -3 temporarily? That breaks the invariant.
            // Let's use a parallel "safe_marked" array.
            safe_queue.emplace_back(p.first, p.second);
            // mark visible as -3 to indicate it's pending visit
            visible[p.first][p.second] = -3;
            changed = true;
          }
        }
      }
    }
  }
  return changed;
}

// Subset deduction (Baseline2 style).
// For two cells A, B with numbers, consider U_A = unknown neighbors of A, U_B = unknown neighbors of B.
// Let remA = numA - markedA_around, remB = numB - markedB_around.
// If U_A is a subset of U_B, then the U_B \ U_A must contain exactly remB - remA mines.
// Special cases:
//   - if remB - remA == |U_B \ U_A|: all diff are mines, U_A \ U_B? No, only meaningful for B's diff.
//   - if remB - remA == 0: all U_B \ U_A are safe.
static bool SubsetDeduce() {
  struct Constraint {
    int r, c;
    std::vector<std::pair<int, int>> unknowns;
    int rem;  // mines remaining in unknowns
  };
  std::vector<Constraint> cons;
  std::vector<std::pair<int, int>> nbrs;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] < 0) continue;
      int num = visible[i][j];
      int mine_cnt = 0;
      Constraint cc;
      cc.r = i;
      cc.c = j;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) {
        int v = visible[p.first][p.second];
        if (v == -2) cc.unknowns.push_back(p);  // still truly unknown
        else if (v == -1) ++mine_cnt;
      }
      if (cc.unknowns.empty()) continue;
      cc.rem = num - mine_cnt;
      cons.push_back(std::move(cc));
    }
  }
  bool changed = false;
  int n = static_cast<int>(cons.size());
  for (int a = 0; a < n; ++a) {
    for (int b = 0; b < n; ++b) {
      if (a == b) continue;
      // Check if cons[a].unknowns is subset of cons[b].unknowns
      if (cons[a].unknowns.size() >= cons[b].unknowns.size()) continue;
      // quick distance filter (both cells must be near): actually neighbors of adjacent cells are within Chebyshev 3
      if (std::abs(cons[a].r - cons[b].r) > 2 || std::abs(cons[a].c - cons[b].c) > 2) continue;
      bool is_subset = true;
      for (auto &p : cons[a].unknowns) {
        bool found = false;
        for (auto &q : cons[b].unknowns) {
          if (p == q) {
            found = true;
            break;
          }
        }
        if (!found) {
          is_subset = false;
          break;
        }
      }
      if (!is_subset) continue;
      // diff = B \ A
      std::vector<std::pair<int, int>> diff;
      for (auto &q : cons[b].unknowns) {
        bool in_a = false;
        for (auto &p : cons[a].unknowns) {
          if (p == q) {
            in_a = true;
            break;
          }
        }
        if (!in_a) diff.push_back(q);
      }
      int drem = cons[b].rem - cons[a].rem;
      if (drem < 0 || drem > static_cast<int>(diff.size())) continue;
      if (drem == 0) {
        // all diff are safe
        for (auto &p : diff) {
          if (visible[p.first][p.second] == -2) {
            visible[p.first][p.second] = -3;
            safe_queue.emplace_back(p.first, p.second);
            changed = true;
          }
        }
      } else if (drem == static_cast<int>(diff.size())) {
        for (auto &p : diff) {
          if (visible[p.first][p.second] == -2) {
            visible[p.first][p.second] = -1;
            mine_queue.emplace_back(p.first, p.second);
            changed = true;
          }
        }
      }
    }
  }
  return changed;
}

// Enumeration deduction for deep cases (optional).
// For all unknown cells adjacent to any number clue, enumerate consistent mine assignments.
// Cells that are mines in ALL valid assignments => definitely mines.
// Cells that are safe in ALL valid assignments => definitely safe.
// Also, probabilities can guide risky guesses.

static std::vector<std::pair<int, int>> frontier;  // unknown cells adjacent to numbers
static int mines_in_valid[kCMaxN][kCMaxN];
static long long valid_assignments;

// Structure of constraints for enumeration
struct EnumConstraint {
  std::vector<int> idx;  // indices into frontier
  int rem;
};

static std::vector<EnumConstraint> enum_cons;
static std::vector<int> frontier_assign;  // 0 or 1

// For faster enumeration: group frontier cells by connected components of constraints.
static int comp_id_of_cell[kCMaxN][kCMaxN];

static void EnumRecurse(int i, int total_mines_left, int total_unknown_left,
                        const std::vector<int> &cell_indices,
                        const std::vector<int> &con_indices,
                        std::vector<int> &con_rem,
                        std::vector<int> &con_unk) {
  if (i == static_cast<int>(cell_indices.size())) {
    // check all constraints have rem == 0 and unk == 0
    for (int ci : con_indices) {
      if (con_rem[ci] != 0) return;
    }
    // Check total mines constraint isn't violated (if we're tracking)
    ++valid_assignments;
    for (int k = 0; k < static_cast<int>(cell_indices.size()); ++k) {
      if (frontier_assign[cell_indices[k]]) {
        auto &p = frontier[cell_indices[k]];
        mines_in_valid[p.first][p.second]++;
      }
    }
    return;
  }
  int idx = cell_indices[i];
  // Find which constraints this cell participates in
  // Precomputed: cell_cons[idx] -> list of constraint indices
  // We'll do linear scan via con_indices
  // Try 0 (safe)
  bool ok0 = true;
  for (int ci : con_indices) {
    // Check if this cell is in this constraint
    bool in_this = false;
    for (int c2 : enum_cons[ci].idx) {
      if (c2 == idx) {
        in_this = true;
        break;
      }
    }
    if (!in_this) continue;
    // reducing unknown by 1, rem unchanged
    if (con_rem[ci] > con_unk[ci] - 1) {
      ok0 = false;
      break;
    }
  }
  if (ok0) {
    std::vector<int> affected;
    for (int ci : con_indices) {
      bool in_this = false;
      for (int c2 : enum_cons[ci].idx) {
        if (c2 == idx) {
          in_this = true;
          break;
        }
      }
      if (in_this) {
        con_unk[ci] -= 1;
        affected.push_back(ci);
      }
    }
    frontier_assign[idx] = 0;
    EnumRecurse(i + 1, total_mines_left, total_unknown_left - 1,
                cell_indices, con_indices, con_rem, con_unk);
    for (int ci : affected) con_unk[ci] += 1;
  }

  // Try 1 (mine)
  if (total_mines_left > 0) {
    bool ok1 = true;
    for (int ci : con_indices) {
      bool in_this = false;
      for (int c2 : enum_cons[ci].idx) {
        if (c2 == idx) {
          in_this = true;
          break;
        }
      }
      if (!in_this) continue;
      if (con_rem[ci] - 1 < 0) {
        ok1 = false;
        break;
      }
    }
    if (ok1) {
      std::vector<int> affected;
      for (int ci : con_indices) {
        bool in_this = false;
        for (int c2 : enum_cons[ci].idx) {
          if (c2 == idx) {
            in_this = true;
            break;
          }
        }
        if (in_this) {
          con_rem[ci] -= 1;
          con_unk[ci] -= 1;
          affected.push_back(ci);
        }
      }
      frontier_assign[idx] = 1;
      EnumRecurse(i + 1, total_mines_left - 1, total_unknown_left - 1,
                  cell_indices, con_indices, con_rem, con_unk);
      for (int ci : affected) {
        con_rem[ci] += 1;
        con_unk[ci] += 1;
      }
    }
  }
}

// Full enumeration deduction: only invoked when simpler methods fail.
// Returns true if new info derived.
static bool EnumDeduce(int &best_guess_r, int &best_guess_c, double &best_guess_prob) {
  best_guess_r = -1;
  best_guess_c = -1;
  best_guess_prob = 2.0;  // will store probability of being mine (lower is better)

  // Build frontier: all truly-unknown cells adjacent to at least one revealed number.
  frontier.clear();
  std::vector<std::pair<int, int>> nbrs;
  int cell_idx[kCMaxN][kCMaxN];
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j) cell_idx[i][j] = -1;

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] != -2) continue;
      // adjacent to a number?
      bool adj_num = false;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) {
        if (visible[p.first][p.second] >= 0) {
          adj_num = true;
          break;
        }
      }
      if (adj_num) {
        cell_idx[i][j] = static_cast<int>(frontier.size());
        frontier.emplace_back(i, j);
      }
    }
  }
  int F = static_cast<int>(frontier.size());
  if (F == 0) return false;

  // Build constraints
  enum_cons.clear();
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] < 0) continue;
      int mine_cnt = 0;
      EnumConstraint c;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) {
        int v = visible[p.first][p.second];
        if (v == -1) ++mine_cnt;
        else if (v == -2) {
          c.idx.push_back(cell_idx[p.first][p.second]);
        }
      }
      if (c.idx.empty()) continue;
      c.rem = visible[i][j] - mine_cnt;
      if (c.rem < 0 || c.rem > static_cast<int>(c.idx.size())) {
        // inconsistent; shouldn't happen
        return false;
      }
      enum_cons.push_back(std::move(c));
    }
  }

  // Find connected components of frontier cells (via shared constraints)
  std::vector<int> cell_comp(F, -1);
  std::vector<std::vector<int>> cell_cons(F);
  for (int ci = 0; ci < static_cast<int>(enum_cons.size()); ++ci) {
    for (int x : enum_cons[ci].idx) {
      cell_cons[x].push_back(ci);
    }
  }
  int num_comps = 0;
  for (int s = 0; s < F; ++s) {
    if (cell_comp[s] != -1) continue;
    // BFS
    std::vector<int> stack{s};
    cell_comp[s] = num_comps;
    while (!stack.empty()) {
      int u = stack.back();
      stack.pop_back();
      for (int ci : cell_cons[u]) {
        for (int v : enum_cons[ci].idx) {
          if (cell_comp[v] == -1) {
            cell_comp[v] = num_comps;
            stack.push_back(v);
          }
        }
      }
    }
    ++num_comps;
  }

  // For each component, enumerate assignments
  frontier_assign.assign(F, 0);
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j) mines_in_valid[i][j] = 0;

  // total remaining mines globally
  int remaining_mines = total_mines - known_mines;
  int non_frontier_unknowns = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] == -2 && cell_idx[i][j] == -1) {
        ++non_frontier_unknowns;
      }
    }
  }

  // If enumeration is too large, skip and just return false
  // We enumerate per-component independently for tractability.
  // Sum of frontier sizes might be large; cap total search by checking each component.
  // For simplicity, do per-component enumeration but skip if too big.

  // For per-component enumeration, collect cells and constraints belonging to it.
  std::vector<std::vector<std::vector<int>>> comp_assignments(num_comps);  // list of mine-count-by-cell summaries per assignment count
  // For each component: enumerate all valid assignments, count valid_assignments and mines_in_valid increments.
  // We'll also store the distribution over number of mines used.

  // Per-component data
  struct CompData {
    std::vector<int> cells;               // cell indices in frontier
    std::vector<int> cons;                // constraint indices
    long long total_valid = 0;            // total valid assignments
    std::vector<long long> count_by_k;    // count_by_k[k] = number of assignments with k mines
    std::vector<std::vector<long long>> mines_by_k;  // mines_by_k[k][local_cell_idx] = times that cell is mine
  };
  std::vector<CompData> comp_data(num_comps);
  for (int x = 0; x < F; ++x) comp_data[cell_comp[x]].cells.push_back(x);
  for (int ci = 0; ci < static_cast<int>(enum_cons.size()); ++ci) {
    if (enum_cons[ci].idx.empty()) continue;
    int c0 = cell_comp[enum_cons[ci].idx[0]];
    comp_data[c0].cons.push_back(ci);
  }

  // Cap: skip enumeration if component is too large
  const int kMaxComp = 22;
  bool any_skipped = false;
  for (int cid = 0; cid < num_comps; ++cid) {
    if (static_cast<int>(comp_data[cid].cells.size()) > kMaxComp) {
      any_skipped = true;
    }
  }

  // Enumerate each component
  for (int cid = 0; cid < num_comps; ++cid) {
    auto &cd = comp_data[cid];
    int sz = static_cast<int>(cd.cells.size());
    if (sz > kMaxComp) continue;  // skip too-large

    cd.count_by_k.assign(sz + 1, 0);
    cd.mines_by_k.assign(sz + 1, std::vector<long long>(sz, 0));

    // Map global cell idx to local index
    std::vector<int> local_idx(F, -1);
    for (int k = 0; k < sz; ++k) local_idx[cd.cells[k]] = k;

    // Build local constraints
    struct LocalCon {
      std::vector<int> local;
      int rem;
    };
    std::vector<LocalCon> lcs;
    for (int ci : cd.cons) {
      LocalCon lc;
      lc.rem = enum_cons[ci].rem;
      for (int x : enum_cons[ci].idx) lc.local.push_back(local_idx[x]);
      lcs.push_back(std::move(lc));
    }

    // Each cell's list of local-constraint indices
    std::vector<std::vector<int>> cell_lcs(sz);
    for (int li = 0; li < static_cast<int>(lcs.size()); ++li)
      for (int x : lcs[li].local) cell_lcs[x].push_back(li);

    std::vector<int> con_rem(lcs.size()), con_unk(lcs.size());
    for (int li = 0; li < static_cast<int>(lcs.size()); ++li) {
      con_rem[li] = lcs[li].rem;
      con_unk[li] = static_cast<int>(lcs[li].local.size());
    }

    std::vector<int> assign(sz, 0);

    // Recursive enumeration
    std::function<void(int, int)> rec = [&](int pos, int mine_cnt_so_far) {
      if (pos == sz) {
        // verify all constraints satisfied (rem should be 0 where unk==0)
        for (auto &lc_rem : con_rem) {
          // if unk for this constraint is 0 and rem!=0 -> invalid
          // But we check on the fly below.
        }
        // All should satisfy by construction at unk==0.
        cd.count_by_k[mine_cnt_so_far]++;
        for (int k = 0; k < sz; ++k) {
          if (assign[k]) cd.mines_by_k[mine_cnt_so_far][k]++;
        }
        return;
      }
      // Try 0
      bool ok0 = true;
      for (int li : cell_lcs[pos]) {
        if (con_rem[li] > con_unk[li] - 1) {
          ok0 = false;
          break;
        }
      }
      if (ok0) {
        for (int li : cell_lcs[pos]) con_unk[li] -= 1;
        assign[pos] = 0;
        rec(pos + 1, mine_cnt_so_far);
        for (int li : cell_lcs[pos]) con_unk[li] += 1;
      }
      // Try 1 (only if we can still have enough mines globally)
      if (mine_cnt_so_far + 1 > remaining_mines) return;
      bool ok1 = true;
      for (int li : cell_lcs[pos]) {
        if (con_rem[li] - 1 < 0) {
          ok1 = false;
          break;
        }
      }
      if (ok1) {
        for (int li : cell_lcs[pos]) {
          con_rem[li] -= 1;
          con_unk[li] -= 1;
        }
        assign[pos] = 1;
        rec(pos + 1, mine_cnt_so_far + 1);
        for (int li : cell_lcs[pos]) {
          con_rem[li] += 1;
          con_unk[li] += 1;
        }
      }
    };
    rec(0, 0);
  }

  // Now combine components. If we know the global number of mines, we'd need to sum
  // mines across components s.t. total_mines_in_frontier + mines_in_non_frontier == remaining_mines.
  // For simplicity (and correctness of deductions), we treat components independently
  // for determining 0/1 certainties: a cell is certain if, within its own component,
  // it's 0 or 1 in all valid assignments of that component (ignoring total-mine constraint).
  // This gives correct certainties (a stronger one with the global constraint could find more,
  // but this alone is very useful).

  bool changed = false;
  for (int cid = 0; cid < num_comps; ++cid) {
    auto &cd = comp_data[cid];
    int sz = static_cast<int>(cd.cells.size());
    if (sz == 0 || sz > kMaxComp) continue;
    long long total = 0;
    for (long long v : cd.count_by_k) total += v;
    if (total == 0) continue;
    for (int k = 0; k < sz; ++k) {
      long long mine_cnt = 0;
      for (int m = 0; m <= sz; ++m) mine_cnt += cd.mines_by_k[m][k];
      auto &p = frontier[cd.cells[k]];
      if (visible[p.first][p.second] != -2) continue;  // already decided
      if (mine_cnt == 0) {
        // always safe
        visible[p.first][p.second] = -3;
        safe_queue.emplace_back(p.first, p.second);
        changed = true;
      } else if (mine_cnt == total) {
        visible[p.first][p.second] = -1;
        mine_queue.emplace_back(p.first, p.second);
        changed = true;
      }
    }
  }

  if (changed) return true;

  // No certainties -> compute best guess probability (minimum mine prob)
  // For frontier cells (that weren't skipped):
  for (int cid = 0; cid < num_comps; ++cid) {
    auto &cd = comp_data[cid];
    int sz = static_cast<int>(cd.cells.size());
    if (sz == 0 || sz > kMaxComp) continue;
    long long total = 0;
    for (long long v : cd.count_by_k) total += v;
    if (total == 0) continue;
    for (int k = 0; k < sz; ++k) {
      auto &p = frontier[cd.cells[k]];
      if (visible[p.first][p.second] != -2) continue;
      long long mine_cnt = 0;
      for (int m = 0; m <= sz; ++m) mine_cnt += cd.mines_by_k[m][k];
      double prob = static_cast<double>(mine_cnt) / static_cast<double>(total);
      if (prob < best_guess_prob) {
        best_guess_prob = prob;
        best_guess_r = p.first;
        best_guess_c = p.second;
      }
    }
  }

  // Also consider non-frontier unknowns: their prob ~ remaining_mines_in_interior / non_frontier_unknowns
  // But we don't know exact share. Approximate: (remaining_mines - E[frontier_mines]) / non_frontier_unknowns
  if (non_frontier_unknowns > 0) {
    // Estimate expected mines in frontier using per-component expectations (ignoring global constraint)
    double exp_frontier_mines = 0.0;
    for (int cid = 0; cid < num_comps; ++cid) {
      auto &cd = comp_data[cid];
      int sz = static_cast<int>(cd.cells.size());
      if (sz == 0 || sz > kMaxComp) continue;
      long long total = 0;
      for (long long v : cd.count_by_k) total += v;
      if (total == 0) continue;
      for (int k = 0; k < sz; ++k) {
        long long mine_cnt = 0;
        for (int m = 0; m <= sz; ++m) mine_cnt += cd.mines_by_k[m][k];
        exp_frontier_mines += static_cast<double>(mine_cnt) / static_cast<double>(total);
      }
    }
    double remaining_for_interior = static_cast<double>(remaining_mines) - exp_frontier_mines;
    if (remaining_for_interior < 0) remaining_for_interior = 0;
    double interior_prob = remaining_for_interior / static_cast<double>(non_frontier_unknowns);
    if (interior_prob < best_guess_prob) {
      best_guess_prob = interior_prob;
      // pick some non-frontier unknown cell
      for (int i = 0; i < rows && best_guess_r == -1; ++i) {
        for (int j = 0; j < columns; ++j) {
          if (visible[i][j] == -2 && cell_idx[i][j] == -1) {
            best_guess_r = i;
            best_guess_c = j;
            break;
          }
        }
        if (best_guess_r != -1 && cell_idx[best_guess_r][best_guess_c] != -1) {
          // it's on frontier, skip
          best_guess_r = -1;
        }
      }
      // Re-search properly
      if (best_guess_r == -1) {
        for (int i = 0; i < rows; ++i) {
          for (int j = 0; j < columns; ++j) {
            if (visible[i][j] == -2 && cell_idx[i][j] == -1) {
              best_guess_r = i;
              best_guess_c = j;
              goto done_pick;
            }
          }
        }
      }
    done_pick:;
    }
  }

  return false;
}

// Count any unknown cells remaining
static bool AnyUnknownLeft() {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] == -2 || visible[i][j] == -3) return true;
    }
  }
  return false;
}

// Pick a random unknown cell (fallback guess).
static bool PickRandomUnknown(int &r, int &c) {
  std::vector<std::pair<int, int>> choices;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] == -2) choices.emplace_back(i, j);
    }
  }
  if (choices.empty()) return false;
  static std::mt19937 rng(12345);
  int k = static_cast<int>(rng() % choices.size());
  r = choices[k].first;
  c = choices[k].second;
  return true;
}

void Decide() {
  // Strategy:
  // 1. If any mine is known to be mine (mine_queue), mark it.
  // 2. If any safe cell is queued, visit it.
  // 3. Otherwise, try to deduce: basic, then subset, then enumeration.
  // 4. If still no certainty, pick best guess (lowest mine probability).

  // Drop any queued items that are already visited/marked
  auto prune = [&](std::vector<std::pair<int, int>> &q) {
    std::vector<std::pair<int, int>> nq;
    for (auto &p : q) {
      // for safe queue: visible[p]==-3 means pending; if -2 or ==number -> already done
      // for mine queue: visible[p]==-1 means already marked
      // We'll just keep all for now
      nq.push_back(p);
    }
    q = nq;
  };
  prune(mine_queue);
  prune(safe_queue);

  // Step 1: mark mines
  while (!mine_queue.empty()) {
    auto p = mine_queue.back();
    mine_queue.pop_back();
    if (visible[p.first][p.second] != -1) continue;
    // Mark it. visible stays -1 after marking (server shows @)
    ++known_mines;
    Execute(p.first, p.second, 1);
    return;
  }

  // Step 2: visit safes
  while (!safe_queue.empty()) {
    auto p = safe_queue.back();
    safe_queue.pop_back();
    if (visible[p.first][p.second] != -3) continue;
    // Visit; server will update. Restore visible temporarily for reading.
    visible[p.first][p.second] = -2;
    Execute(p.first, p.second, 0);
    return;
  }

  // Step 3: deductions
  bool progress = BasicDeduce();
  if (progress) {
    // Recurse
    Decide();
    return;
  }
  progress = SubsetDeduce();
  if (progress) {
    Decide();
    return;
  }

  // Step 4: enumeration + guessing
  int gr = -1, gc = -1;
  double gp = 2.0;
  bool enum_progress = EnumDeduce(gr, gc, gp);
  if (enum_progress) {
    Decide();
    return;
  }

  // Step 5: guess
  if (gr != -1) {
    // Visit best guess
    visible[gr][gc] = -2;  // ensure it's unknown
    Execute(gr, gc, 0);
    return;
  }

  // Last resort: random unknown
  int r, c;
  if (PickRandomUnknown(r, c)) {
    Execute(r, c, 0);
    return;
  }
  // Shouldn't happen, but if nothing to do, just visit (0,0)
  Execute(0, 0, 0);
}

#endif
