#ifndef TUI_H
#define TUI_H
#include <ncurses.h>

/* Minimum length to display the full names of each field
 * on the X axis */

/* DATE: 12/31/2025___
 *       ^...........^ == 13 */
#define DATE_X 13

/* TRANSACTION TYPE: EXPENSE___
 *                   ^........^  == 10 */
#define TRNS_X 10

/* AMOUNT: Max 7 digits + 2 decimal precision + the literal point '.' +
 * currency symbol + a space. Result = 12.
 * E.G. $1234567.10_ */
#define AMNT_X 12

/* 11 'n 14 derived from the length of "CATEGORY" and 
 * "DESCRIPTION" + 3 */
#define CATG_X 11
#define DESC_X 14

#define MIN_COLUMNS (DATE_X + TRNS_X + AMNT_X + CATG_X + DESC_X)
#define MIN_ROWS 20
#define INPUT_WIN_ROWS 8
#define BOX_OFFSET 2

typedef struct ColumnWidth {
	int max_x;
	int date;
	int catg;
	int desc;
	int trns;
	int amnt;
} ColumnWidth;

/* Tests that the stdscr is big enough to handle this program */
extern int test_terminal_size(void);

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

/* Removes and refreshes wptr to remove error message.
 * For use with input subwindows. Dimensions are hard coded to be 4 above
 * the maximum y coordinate */
extern void clear_input_error_message(WINDOW *wptr);

/* Moves cursor in wptr to y-coord y for a string len
 * of len */
extern int mvwxctr(WINDOW *wptr, int y, int len);

/* Prints str centered on the X axis of wptr and y-coord
 * of y */
extern int mvwxcprintw(WINDOW *wptr, int y, char *str);

/* Creates the subwindow for the read function */
extern WINDOW *create_lines_subwindow(int max_y, int max_x, int y_off, int x_off);

/* Creates a subwindow for user input with dimensions based on stdscr */
extern WINDOW *create_input_subwindow(void);

/* Show a message str in an input subwindow which exits on key press */
extern void nc_message(char *str);

/* Initializes the stdscr */
extern WINDOW *nc_init_stdscr(void);

/* Prints the welcome splash screen */
extern void nc_print_welcome(WINDOW *wptr);

/* Prints the string "DEBUG" on the bottom right of wptr */
extern void nc_print_debug_flag(WINDOW *wptr);

/* Prints the footer with menu options */
extern void nc_print_footer(WINDOW *wptr);

#endif
