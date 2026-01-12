#ifndef TUI_H
#define TUI_H
#include <ncurses.h>

#define MIN_COL 40
#define MIN_ROW 20

struct ColumnWidth {
	int max_x;
	int date;
	int catg;
	int desc;
	int trns;
	int amnt;
};

/* 
 * Calculate column offsets for ncurses formatting, sets date width into date,
 * category width into catg, description width into desc, transaction type
 * width into trns, and amount into amnt.
 */
extern void calculate_columns(struct ColumnWidth *cw);

/*
 * Print headers for the reading data to wptr, column width is calculated
 * by calculate_columns() automatically. Offsets calculation by an offset
 * of x_off
 */
extern void print_column_headers(WINDOW *wptr, int x_off);

/* Exits the window wptr and hangs the terminal until a key is pressed */
extern void nc_exit_window_key(WINDOW *wptr);

/* Exits window wptr */
extern void nc_exit_window(WINDOW *wptr);

/* Prints the welcome splash screen */
extern void nc_print_welcome(WINDOW *wptr);

/* Prints the footer with menu options */
extern void nc_print_footer(WINDOW *wptr);

/* Initializes the stdscr */
extern WINDOW *nc_init_stdscr();


extern int test_terminal_size(int max_y, int max_x);

#endif
