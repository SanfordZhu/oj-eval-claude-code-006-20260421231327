#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>

int rows;
int columns;
int total_mines;
int game_state;  // 0 continuing, 1 win, -1 lose

// Internal state
static const int kMaxN = 35;
// is_mine[i][j]: true if (i,j) has a mine
static bool is_mine[kMaxN][kMaxN];
// visited[i][j]: true if visited
static bool visited[kMaxN][kMaxN];
// marked[i][j]: true if marked as mine
static bool marked[kMaxN][kMaxN];
// mine_count[i][j]: number of adjacent mines
static int mine_count_arr[kMaxN][kMaxN];

static int visit_count;        // visited non-mine grids
static int marked_correct;     // correctly marked mines
static int non_mine_total;     // total number of non-mine grids
static int non_mine_visited;   // visited non-mine grids count
static bool lose_by_mark_wrong;  // true if lost by marking a non-mine
static int wrong_mark_r, wrong_mark_c;  // position of wrong mark
static bool lose_by_visit_mine;  // true if lost by visiting mine
static int visited_mine_r, visited_mine_c;

static inline bool InBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

static inline int CountAdjMines(int r, int c) {
  int cnt = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (InBounds(nr, nc) && is_mine[nr][nc]) ++cnt;
    }
  }
  return cnt;
}

void InitMap() {
  std::cin >> rows >> columns;
  total_mines = 0;
  game_state = 0;
  visit_count = 0;
  marked_correct = 0;
  non_mine_visited = 0;
  non_mine_total = 0;
  lose_by_mark_wrong = false;
  lose_by_visit_mine = false;
  wrong_mark_r = wrong_mark_c = -1;
  visited_mine_r = visited_mine_c = -1;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      is_mine[i][j] = false;
      visited[i][j] = false;
      marked[i][j] = false;
      mine_count_arr[i][j] = 0;
    }
  }
  for (int i = 0; i < rows; ++i) {
    std::string row_str;
    std::cin >> row_str;
    for (int j = 0; j < columns; ++j) {
      if (row_str[j] == 'X') {
        is_mine[i][j] = true;
        ++total_mines;
      }
    }
  }
  non_mine_total = rows * columns - total_mines;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (!is_mine[i][j]) {
        mine_count_arr[i][j] = CountAdjMines(i, j);
      }
    }
  }
}

static void VisitRecursive(int r, int c) {
  if (!InBounds(r, c)) return;
  if (visited[r][c] || marked[r][c]) return;
  visited[r][c] = true;
  if (!is_mine[r][c]) {
    ++non_mine_visited;
  }
  if (mine_count_arr[r][c] == 0 && !is_mine[r][c]) {
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        VisitRecursive(r + dr, c + dc);
      }
    }
  }
}

void VisitBlock(int r, int c) {
  if (!InBounds(r, c)) return;
  if (visited[r][c] || marked[r][c]) return;
  if (is_mine[r][c]) {
    visited[r][c] = true;
    lose_by_visit_mine = true;
    visited_mine_r = r;
    visited_mine_c = c;
    game_state = -1;
    return;
  }
  VisitRecursive(r, c);
  if (non_mine_visited == non_mine_total) {
    game_state = 1;
  }
}

void MarkMine(int r, int c) {
  if (!InBounds(r, c)) return;
  if (visited[r][c] || marked[r][c]) return;
  if (!is_mine[r][c]) {
    // Wrong mark -> lose
    lose_by_mark_wrong = true;
    wrong_mark_r = r;
    wrong_mark_c = c;
    game_state = -1;
    return;
  }
  marked[r][c] = true;
  ++marked_correct;
  // Marking doesn't win the game by itself per description
  // (victory is when all non-mine grids visited)
}

void AutoExplore(int r, int c) {
  if (!InBounds(r, c)) return;
  // Must be visited non-mine grid (showing a number)
  if (!visited[r][c] || is_mine[r][c]) return;
  // Count marked mines around
  int marked_around = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (InBounds(nr, nc) && marked[nr][nc]) ++marked_around;
    }
  }
  if (marked_around != mine_count_arr[r][c]) return;
  // Visit all non-marked, non-visited neighbors
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (!InBounds(nr, nc)) continue;
      if (visited[nr][nc] || marked[nr][nc]) continue;
      if (is_mine[nr][nc]) {
        // Visiting a mine -> lose
        visited[nr][nc] = true;
        lose_by_visit_mine = true;
        visited_mine_r = nr;
        visited_mine_c = nc;
        game_state = -1;
        return;
      }
      VisitRecursive(nr, nc);
    }
  }
  if (non_mine_visited == non_mine_total) {
    game_state = 1;
  }
}

void ExitGame() {
  if (game_state == 1) {
    std::cout << "YOU WIN!" << std::endl;
    std::cout << non_mine_visited << " " << total_mines << std::endl;
  } else {
    std::cout << "GAME OVER!" << std::endl;
    std::cout << non_mine_visited << " " << marked_correct << std::endl;
  }
  exit(0);
}

void PrintMap() {
  // If game is won, print all mines as '@' regardless of marked
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      char ch;
      if (game_state == 1) {
        if (is_mine[i][j]) {
          ch = '@';
        } else {
          ch = static_cast<char>('0' + mine_count_arr[i][j]);
        }
      } else {
        if (marked[i][j]) {
          // Marked: @ if mine, X if not (but marking non-mine also causes lose)
          if (is_mine[i][j]) {
            ch = '@';
          } else {
            ch = 'X';
          }
        } else if (visited[i][j]) {
          if (is_mine[i][j]) {
            ch = 'X';
          } else {
            ch = static_cast<char>('0' + mine_count_arr[i][j]);
          }
        } else {
          ch = '?';
        }
      }
      std::cout << ch;
    }
    std::cout << '\n';
  }
}

#endif
