#ifndef TUI_H
#define TUI_H
#include <ncurses.h>

#define MIN_COL 40
#define MIN_ROW 20

extern void nc_print_welcome(WINDOW *wptr);
extern void nc_print_footer(WINDOW *wptr);
extern WINDOW *nc_new_win();
extern int test_terminal_size(int max_y, int max_x);

#endif
