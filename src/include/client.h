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
 * visible[i][j]:
 *   -2 unknown
 *   -1 marked mine (or KNOWN to be mine and queued)
 *   -3 pending safe (queued to be visited)
 *   0..8 visited non-mine with given number
 */
static const int kCMaxN = 35;
static int visible[kCMaxN][kCMaxN];
static int known_mines;   // number we've committed as mines (either marked or proven)
static std::vector<std::pair<int, int>> safe_queue;
static std::vector<std::pair<int, int>> mine_queue;

static inline bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void InitGame() {
  known_mines = 0;
  safe_queue.clear();
  mine_queue.clear();
  for (int i = 0; i < kCMaxN; ++i)
    for (int j = 0; j < kCMaxN; ++j) visible[i][j] = -2;
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
        // keep previous knowledge
      } else if (ch == '@') {
        visible[i][j] = -1;
      } else if (ch == 'X') {
        visible[i][j] = -1;
      } else if (ch >= '0' && ch <= '8') {
        visible[i][j] = ch - '0';
      }
    }
  }
}

static inline void Neighbors(int r, int c, std::vector<std::pair<int, int>> &out) {
  out.clear();
  for (int dr = -1; dr <= 1; ++dr)
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (CInBounds(nr, nc)) out.emplace_back(nr, nc);
    }
}

// --- Basic deduction ---
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
        for (auto &p : nbrs) {
          if (visible[p.first][p.second] == -2) {
            visible[p.first][p.second] = -1;
            mine_queue.emplace_back(p.first, p.second);
            changed = true;
          }
        }
      } else if (remaining == 0) {
        for (auto &p : nbrs) {
          if (visible[p.first][p.second] == -2) {
            safe_queue.emplace_back(p.first, p.second);
            visible[p.first][p.second] = -3;
            changed = true;
          }
        }
      }
    }
  }
  return changed;
}

// --- Subset deduction (Baseline2 style) ---
static bool SubsetDeduce() {
  struct Constraint {
    int r, c;
    std::vector<std::pair<int, int>> unknowns;
    int rem;
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
        if (v == -2) cc.unknowns.push_back(p);
        else if (v == -1) ++mine_cnt;
      }
      if (cc.unknowns.empty()) continue;
      cc.rem = num - mine_cnt;
      cons.push_back(std::move(cc));
    }
  }
  bool changed = false;
  int n = (int)cons.size();
  for (int a = 0; a < n; ++a) {
    for (int b = 0; b < n; ++b) {
      if (a == b) continue;
      if (cons[a].unknowns.size() >= cons[b].unknowns.size()) continue;
      if (std::abs(cons[a].r - cons[b].r) > 2 || std::abs(cons[a].c - cons[b].c) > 2) continue;
      bool is_subset = true;
      for (auto &p : cons[a].unknowns) {
        bool found = false;
        for (auto &q : cons[b].unknowns) if (p == q) { found = true; break; }
        if (!found) { is_subset = false; break; }
      }
      if (!is_subset) continue;
      std::vector<std::pair<int, int>> diff;
      for (auto &q : cons[b].unknowns) {
        bool in_a = false;
        for (auto &p : cons[a].unknowns) if (p == q) { in_a = true; break; }
        if (!in_a) diff.push_back(q);
      }
      int drem = cons[b].rem - cons[a].rem;
      if (drem < 0 || drem > (int)diff.size()) continue;
      if (drem == 0) {
        for (auto &p : diff) {
          if (visible[p.first][p.second] == -2) {
            visible[p.first][p.second] = -3;
            safe_queue.emplace_back(p.first, p.second);
            changed = true;
          }
        }
      } else if (drem == (int)diff.size()) {
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

// --- Enumeration deduction ---
// Enumerate assignments per component, then combine with global total-mines constraint
// to compute marginal probabilities.

static int frontier_cell_idx[kCMaxN][kCMaxN];  // global (i,j) -> frontier index or -1

struct EnumResult {
  int sz;
  std::vector<int> global_cells;          // indices into frontier array
  std::vector<std::vector<long long>> count_by_k;  // [k][local] = # valid assignments with k mines where local is mine
  std::vector<long long> total_by_k;      // total_by_k[k] = # valid assignments with k mines
};

static bool EnumDeduce(int &best_guess_r, int &best_guess_c, double &best_guess_prob) {
  best_guess_r = -1;
  best_guess_c = -1;
  best_guess_prob = 2.0;

  // Build frontier: unknown cells adjacent to a revealed number.
  std::vector<std::pair<int, int>> frontier;
  std::vector<std::pair<int, int>> nbrs;
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j) frontier_cell_idx[i][j] = -1;

  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] != -2) continue;
      bool adj_num = false;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) if (visible[p.first][p.second] >= 0) { adj_num = true; break; }
      if (adj_num) {
        frontier_cell_idx[i][j] = (int)frontier.size();
        frontier.emplace_back(i, j);
      }
    }
  }
  int F = (int)frontier.size();
  int non_frontier_unknowns = 0;
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j)
      if (visible[i][j] == -2 && frontier_cell_idx[i][j] == -1) ++non_frontier_unknowns;

  int remaining_mines = total_mines - known_mines;
  // Sanity
  if (remaining_mines < 0) remaining_mines = 0;

  if (F == 0 && non_frontier_unknowns == 0) return false;

  // Build constraints on frontier cells
  struct Con {
    std::vector<int> idx;  // frontier indices
    int rem;
  };
  std::vector<Con> cons;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (visible[i][j] < 0) continue;
      int mine_cnt = 0;
      Con c;
      Neighbors(i, j, nbrs);
      for (auto &p : nbrs) {
        int v = visible[p.first][p.second];
        if (v == -1) ++mine_cnt;
        else if (v == -2) c.idx.push_back(frontier_cell_idx[p.first][p.second]);
      }
      if (c.idx.empty()) continue;
      c.rem = visible[i][j] - mine_cnt;
      if (c.rem < 0 || c.rem > (int)c.idx.size()) return false;
      cons.push_back(std::move(c));
    }
  }

  // Connected components of frontier cells (via constraints).
  std::vector<int> cell_comp(F, -1);
  std::vector<std::vector<int>> cell_cons(F);
  for (int ci = 0; ci < (int)cons.size(); ++ci)
    for (int x : cons[ci].idx) cell_cons[x].push_back(ci);
  int num_comps = 0;
  for (int s = 0; s < F; ++s) {
    if (cell_comp[s] != -1) continue;
    std::vector<int> st{s};
    cell_comp[s] = num_comps;
    while (!st.empty()) {
      int u = st.back();
      st.pop_back();
      for (int ci : cell_cons[u]) {
        for (int v : cons[ci].idx) {
          if (cell_comp[v] == -1) {
            cell_comp[v] = num_comps;
            st.push_back(v);
          }
        }
      }
    }
    ++num_comps;
  }

  // For each component, collect cells & constraints
  std::vector<std::vector<int>> comp_cells(num_comps);
  std::vector<std::vector<int>> comp_cons(num_comps);
  for (int x = 0; x < F; ++x) comp_cells[cell_comp[x]].push_back(x);
  for (int ci = 0; ci < (int)cons.size(); ++ci) {
    if (cons[ci].idx.empty()) continue;
    comp_cons[cell_comp[cons[ci].idx[0]]].push_back(ci);
  }

  const int kMaxComp = 28;

  std::vector<EnumResult> results(num_comps);

  for (int cid = 0; cid < num_comps; ++cid) {
    int sz = (int)comp_cells[cid].size();
    results[cid].sz = sz;
    results[cid].global_cells = comp_cells[cid];
    results[cid].count_by_k.assign(sz + 1, std::vector<long long>(sz, 0));
    results[cid].total_by_k.assign(sz + 1, 0);

    if (sz == 0 || sz > kMaxComp) continue;

    // Map global -> local
    std::vector<int> local_idx(F, -1);
    for (int k = 0; k < sz; ++k) local_idx[comp_cells[cid][k]] = k;

    // Local constraints
    std::vector<std::vector<int>> lc_idx;
    std::vector<int> lc_rem_init;
    for (int ci : comp_cons[cid]) {
      std::vector<int> li;
      for (int x : cons[ci].idx) li.push_back(local_idx[x]);
      lc_idx.push_back(std::move(li));
      lc_rem_init.push_back(cons[ci].rem);
    }

    std::vector<std::vector<int>> cell_lcs(sz);
    for (int li = 0; li < (int)lc_idx.size(); ++li)
      for (int x : lc_idx[li]) cell_lcs[x].push_back(li);

    std::vector<int> con_rem = lc_rem_init;
    std::vector<int> con_unk(lc_idx.size());
    for (int li = 0; li < (int)lc_idx.size(); ++li) con_unk[li] = (int)lc_idx[li].size();

    std::vector<int> assign_vec(sz, 0);

    std::function<void(int, int)> rec = [&](int pos, int k) {
      if (pos == sz) {
        results[cid].total_by_k[k]++;
        for (int x = 0; x < sz; ++x) {
          if (assign_vec[x]) results[cid].count_by_k[k][x]++;
        }
        return;
      }
      // Try 0
      bool ok0 = true;
      for (int li : cell_lcs[pos]) {
        if (con_rem[li] > con_unk[li] - 1) { ok0 = false; break; }
      }
      if (ok0) {
        for (int li : cell_lcs[pos]) con_unk[li] -= 1;
        assign_vec[pos] = 0;
        rec(pos + 1, k);
        for (int li : cell_lcs[pos]) con_unk[li] += 1;
      }
      // Try 1
      if (k + 1 > remaining_mines) return;
      bool ok1 = true;
      for (int li : cell_lcs[pos]) {
        if (con_rem[li] < 1) { ok1 = false; break; }
      }
      if (ok1) {
        for (int li : cell_lcs[pos]) { con_rem[li] -= 1; con_unk[li] -= 1; }
        assign_vec[pos] = 1;
        rec(pos + 1, k + 1);
        for (int li : cell_lcs[pos]) { con_rem[li] += 1; con_unk[li] += 1; }
      }
    };
    rec(0, 0);
  }

  // --- Combine results with global total mine constraint ---
  // Let W[cid][k] = total_by_k (count of assignments using k mines in component cid).
  // Let T = total frontier mines across components + interior_mines = remaining_mines.
  // For interior: C(non_frontier_unknowns, interior_mines).
  // Weight for each (cid_mines_dist, interior_mines): product of W[cid][k_cid] * C(non_frontier_unknowns, interior_mines).

  // DP: combined[k] = sum over distributions with k total frontier mines.
  // Multiply by C(NF, T - k) for interior.
  // Use double (or long double) for probabilities to avoid overflow.

  std::vector<long double> combined(remaining_mines + 1, 0.0L);
  combined[0] = 1.0L;
  std::vector<long double> tmp;
  for (int cid = 0; cid < num_comps; ++cid) {
    int sz = results[cid].sz;
    if (sz == 0) continue;
    if (sz > kMaxComp) {
      // Skip: treat as no info (this limits us, but safe)
      continue;
    }
    tmp.assign(remaining_mines + 1, 0.0L);
    for (int tot = 0; tot <= remaining_mines; ++tot) {
      if (combined[tot] == 0.0L) continue;
      for (int k = 0; k <= sz; ++k) {
        if (results[cid].total_by_k[k] == 0) continue;
        int new_tot = tot + k;
        if (new_tot > remaining_mines) break;
        tmp[new_tot] += combined[tot] * (long double)results[cid].total_by_k[k];
      }
    }
    combined = tmp;
  }

  // Binomial coefficient table (NF choose r) for interior possibilities
  int NF = non_frontier_unknowns;
  std::vector<long double> binom(NF + 1, 0.0L);
  {
    // compute C(NF, r) as long double
    if (NF >= 0) {
      binom[0] = 1.0L;
      for (int r = 1; r <= NF; ++r) {
        binom[r] = binom[r - 1] * (long double)(NF - r + 1) / (long double)r;
      }
    }
  }

  // Total weight sum over all (frontier_mines K such that K<=remaining_mines and NF>=remaining_mines-K).
  long double grand_total = 0.0L;
  for (int K = 0; K <= remaining_mines; ++K) {
    if (combined[K] == 0.0L) continue;
    int I = remaining_mines - K;
    if (I < 0 || I > NF) continue;
    grand_total += combined[K] * binom[I];
  }
  if (grand_total <= 0.0L) {
    // No valid distribution found; fall back to simple logic
    return false;
  }

  // Per-cell mine probability on frontier:
  // For each cell in component cid with sz cells, its contribution to total mine-count
  // given K frontier mines is:
  //   sum over k_cid (count_by_k[k_cid][cell]) * (ways other components sum to K-k_cid).
  // This needs "combined without cid" style DP. Let's compute it.

  // Precompute combined totals per component (component-level distribution vector).
  // Then for each component, remove its contribution to get "others_combined", then reconstruct.
  // Simpler: forward DP storing combined arrays per component prefix, and backward for suffix.

  std::vector<std::vector<long double>> prefix_comb(num_comps + 1);
  prefix_comb[0].assign(remaining_mines + 1, 0.0L);
  prefix_comb[0][0] = 1.0L;
  for (int cid = 0; cid < num_comps; ++cid) {
    int sz = results[cid].sz;
    prefix_comb[cid + 1].assign(remaining_mines + 1, 0.0L);
    if (sz == 0 || sz > kMaxComp) {
      prefix_comb[cid + 1] = prefix_comb[cid];
      continue;
    }
    for (int tot = 0; tot <= remaining_mines; ++tot) {
      if (prefix_comb[cid][tot] == 0.0L) continue;
      for (int k = 0; k <= sz; ++k) {
        if (results[cid].total_by_k[k] == 0) continue;
        int nt = tot + k;
        if (nt > remaining_mines) break;
        prefix_comb[cid + 1][nt] += prefix_comb[cid][tot] * (long double)results[cid].total_by_k[k];
      }
    }
  }

  std::vector<std::vector<long double>> suffix_comb(num_comps + 2);
  suffix_comb[num_comps + 1].assign(remaining_mines + 1, 0.0L);
  suffix_comb[num_comps + 1][0] = 1.0L;
  // suffix_comb[cid] = distribution over total mines from components cid..num_comps-1
  suffix_comb[num_comps] = suffix_comb[num_comps + 1];
  // Better: build from end.
  suffix_comb[num_comps].assign(remaining_mines + 1, 0.0L);
  suffix_comb[num_comps][0] = 1.0L;
  for (int cid = num_comps - 1; cid >= 0; --cid) {
    int sz = results[cid].sz;
    suffix_comb[cid].assign(remaining_mines + 1, 0.0L);
    if (sz == 0 || sz > kMaxComp) {
      suffix_comb[cid] = suffix_comb[cid + 1];
      continue;
    }
    for (int tot = 0; tot <= remaining_mines; ++tot) {
      if (suffix_comb[cid + 1][tot] == 0.0L) continue;
      for (int k = 0; k <= sz; ++k) {
        if (results[cid].total_by_k[k] == 0) continue;
        int nt = tot + k;
        if (nt > remaining_mines) break;
        suffix_comb[cid][nt] += suffix_comb[cid + 1][tot] * (long double)results[cid].total_by_k[k];
      }
    }
  }

  bool changed = false;
  // Compute per-cell mine probability
  std::vector<double> mine_prob(F, -1.0);
  for (int cid = 0; cid < num_comps; ++cid) {
    int sz = results[cid].sz;
    if (sz == 0 || sz > kMaxComp) continue;
    for (int local = 0; local < sz; ++local) {
      int global = comp_cells[cid][local];
      auto &fp = frontier[global];
      if (visible[fp.first][fp.second] != -2) continue;
      long double numerator = 0.0L;
      // numerator = sum over K (count_by_k[k][local] * prefix(cid)[tot1] * suffix(cid+1)[...] * binom(NF, remaining - K))
      // Simpler: for this cid, iterate k (number of mines in this comp), convolve prefix[cid] and suffix[cid+1] for other total,
      // multiply count_by_k[k][local] * ways_other_total * binom(NF, remaining - k - other_total).
      for (int k = 0; k <= sz; ++k) {
        long long cn = results[cid].count_by_k[k][local];
        if (cn == 0) continue;
        // convolve prefix[cid] and suffix[cid+1] -> distribution over other comps' total mines
        // We want sum_{ot} prefix[cid][ot1] * suffix[cid+1][ot2] with ot1+ot2 = other_total
        // Actually prefix[cid] is distribution from comps 0..cid-1, suffix[cid+1] is from cid+1..num_comps-1.
        // Their convolution gives "all other comps combined".
        // We want to weight by binom(NF, remaining - k - other_total).
        for (int ot1 = 0; ot1 <= remaining_mines; ++ot1) {
          if (prefix_comb[cid][ot1] == 0.0L) continue;
          int remain_for_ot2_binom = remaining_mines - k - ot1;
          if (remain_for_ot2_binom < 0) break;
          for (int ot2 = 0; ot2 <= remain_for_ot2_binom; ++ot2) {
            if (suffix_comb[cid + 1][ot2] == 0.0L) continue;
            int I = remaining_mines - k - ot1 - ot2;
            if (I < 0 || I > NF) continue;
            long double w = prefix_comb[cid][ot1] * suffix_comb[cid + 1][ot2] * binom[I];
            numerator += (long double)cn * w;
          }
        }
      }
      double prob = (double)((long double)numerator / grand_total);
      mine_prob[global] = prob;
      // Determine certainty
      if (prob <= 1e-12) {
        // Safe
        visible[fp.first][fp.second] = -3;
        safe_queue.emplace_back(fp.first, fp.second);
        changed = true;
      } else if (prob >= 1.0 - 1e-12) {
        visible[fp.first][fp.second] = -1;
        mine_queue.emplace_back(fp.first, fp.second);
        changed = true;
      }
    }
  }

  // Check interior certainty too
  if (NF > 0) {
    // E[interior_mines] / NF should be 0 or 1 if certain
    long double exp_interior = 0.0L;
    for (int K = 0; K <= remaining_mines; ++K) {
      if (combined[K] == 0.0L) continue;
      int I = remaining_mines - K;
      if (I < 0 || I > NF) continue;
      exp_interior += combined[K] * binom[I] * (long double)I;
    }
    double iprob = (double)(exp_interior / grand_total / (long double)NF);
    if (iprob <= 1e-12) {
      // All interior safe
      for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
          if (visible[i][j] == -2 && frontier_cell_idx[i][j] == -1) {
            visible[i][j] = -3;
            safe_queue.emplace_back(i, j);
            changed = true;
          }
        }
      }
    } else if (iprob >= 1.0 - 1e-12) {
      // All interior are mines
      for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
          if (visible[i][j] == -2 && frontier_cell_idx[i][j] == -1) {
            visible[i][j] = -1;
            mine_queue.emplace_back(i, j);
            changed = true;
          }
        }
      }
    }
  }

  if (changed) return true;

  // Compute best guess probability
  for (int fi = 0; fi < F; ++fi) {
    auto &p = frontier[fi];
    if (visible[p.first][p.second] != -2) continue;
    double pr = mine_prob[fi];
    if (pr < 0) continue;  // skipped comp
    if (pr < best_guess_prob) {
      best_guess_prob = pr;
      best_guess_r = p.first;
      best_guess_c = p.second;
    }
  }

  // Interior cells: compute interior mine probability
  if (NF > 0) {
    // P(mine | interior) = E[interior_mines] / NF
    long double exp_interior = 0.0L;
    for (int K = 0; K <= remaining_mines; ++K) {
      if (combined[K] == 0.0L) continue;
      int I = remaining_mines - K;
      if (I < 0 || I > NF) continue;
      exp_interior += combined[K] * binom[I] * (long double)I;
    }
    double interior_prob = (double)(exp_interior / grand_total / (long double)NF);
    if (interior_prob < best_guess_prob) {
      best_guess_prob = interior_prob;
      // Pick some non-frontier unknown cell. Prefer center of unknown areas for better chance of 0-flood.
      // Actually prefer corner/edge (fewer neighbors = more predictable).
      // Simplest: pick any. Try to find one near edges.
      int best_nbr_count = 100;
      for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < columns; ++j) {
          if (visible[i][j] != -2 || frontier_cell_idx[i][j] != -1) continue;
          // count in-bounds neighbors
          int nc = 0;
          for (int dr = -1; dr <= 1; ++dr)
            for (int dc = -1; dc <= 1; ++dc) {
              if (dr == 0 && dc == 0) continue;
              int nr = i + dr, ncc = j + dc;
              if (CInBounds(nr, ncc)) ++nc;
            }
          if (nc < best_nbr_count) {
            best_nbr_count = nc;
            best_guess_r = i;
            best_guess_c = j;
          }
        }
      }
    }
  }

  return false;
}

static bool PickRandomUnknown(int &r, int &c) {
  std::vector<std::pair<int, int>> choices;
  // Prefer corners/edges
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j)
      if (visible[i][j] == -2) choices.emplace_back(i, j);
  if (choices.empty()) return false;
  // Sort by neighbor count ascending (corners first)
  std::sort(choices.begin(), choices.end(), [&](auto &a, auto &b) {
    auto neigh = [&](int i, int j) {
      int n = 0;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
          if (dr == 0 && dc == 0) continue;
          if (CInBounds(i + dr, j + dc)) ++n;
        }
      return n;
    };
    return neigh(a.first, a.second) < neigh(b.first, b.second);
  });
  r = choices[0].first;
  c = choices[0].second;
  return true;
}

void Decide() {
  // 1. Mark known mines
  while (!mine_queue.empty()) {
    auto p = mine_queue.back();
    mine_queue.pop_back();
    if (visible[p.first][p.second] != -1) continue;
    ++known_mines;
    Execute(p.first, p.second, 1);
    return;
  }
  // 2. Visit known safes
  while (!safe_queue.empty()) {
    auto p = safe_queue.back();
    safe_queue.pop_back();
    if (visible[p.first][p.second] != -3) continue;
    visible[p.first][p.second] = -2;
    Execute(p.first, p.second, 0);
    return;
  }

  // 3. Deduce
  if (BasicDeduce()) { Decide(); return; }
  if (SubsetDeduce()) { Decide(); return; }

  int gr = -1, gc = -1;
  double gp = 2.0;
  if (EnumDeduce(gr, gc, gp)) { Decide(); return; }

  if (gr != -1) {
    visible[gr][gc] = -2;
    Execute(gr, gc, 0);
    return;
  }

  int r, c;
  if (PickRandomUnknown(r, c)) {
    Execute(r, c, 0);
    return;
  }
  Execute(0, 0, 0);
}

#endif
