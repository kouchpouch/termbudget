/*
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program. If not, see <https://www.gnu.org/licenses/>. 
 *
 * termbudget 2026
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#ifndef TUI_H
#define TUI_H

#include <ncurses.h>

#include "helper.h"
#include "parser.h"

/* Minimum length to display the full names of each field
 * on the X axis */

/* DATE: 12/31/2025___
 *       ^...........^ == 13 */
#define DATE_X 13

/* TRANSACTION TYPE: EXPENSE___
 *                   ^........^  == 10 */
#define TRNS_X 10

/* AMOUNT: Max 7 digits + 2 decimal precision + the literal point '.' +
 * currency symbol + a space. Result = 12 plus padding = 14.
 * E.G. $1234567.10_ */
#define AMNT_X 14

/* 11 'n 14 derived from the length of "CATEGORY" and 
 * "DESCRIPTION" + 3 */
#define CATG_X 11
#define DESC_X 14

#define MIN_COLUMNS (DATE_X + TRNS_X + AMNT_X + CATG_X + DESC_X)
#define SHORTEN_THRESH 15
#define SIDEBAR_COLUMNS 40
#define MIN_COLUMNS_SIDEBAR MIN_COLUMNS + SIDEBAR_COLUMNS
#define REVERSE_COLOR 69
#define MIN_ROWS 20
#define INPUT_WIN_ROWS 8
#define BOX_OFFSET 2
#define MAX_LEN_CATG 64
#define MAX_LEN_DESC 64
#define INPUT_MSG_Y_OFFSET 2
#define MAX_Y_CATG_SELECT 22 // Maximum of 20 categories on the screen

struct ColWidth {
	int date;
	int catg;
	int desc;
	int trns;
	int amnt;
};

enum menukeys {
	NO_SELECT = 0,
	ADD = 1,
	EDIT = 2,
	READ = 3,
	QUIT = 4,
	SORT = 5,
	OVERVIEW = 6,
	EDIT_CATG = 7,
	RESIZE = 13,
	NO_RCRD = 14
};

struct ReadWins {
	WINDOW *parent;
	WINDOW *sidebar_parent;
	WINDOW *sidebar_body;
	WINDOW *data;
};

void window_creation_fail(void);

/* Tests that the stdscr is big enough to handle this program */
int test_terminal_size(void);

/* Exits the window wptr and hangs the terminal until a key is pressed */
void nc_exit_window_key(WINDOW *wptr);

/* Exits window wptr */
void nc_exit_window(WINDOW *wptr);

/* Removes and refreshes wptr to remove error message.
 * For use with input subwindows. Dimensions are hard coded to be 4 above
 * the maximum y coordinate */
void clear_input_error_message(WINDOW *wptr);

/* 
 * Calculate column offsets for ncurses formatting, sets date width into date,
 * category width into catg, description width into desc, transaction type
 * width into trns, and amount into amnt.
 */
void calculate_columns(struct ColWidth *cw, int max_x);

void highlight(WINDOW *wptr, int y, int x, int n);

void unhighlight(WINDOW *wptr, int y, int x, int n);

/*
 * Print headers for the reading data to wptr, column width is calculated
 * by calculate_columns() automatically. Offsets calculation by an offset
 * of x_off
 */
void print_column_headers(WINDOW *wptr, int x_off);

void nc_print_record_vert(WINDOW *wptr, _transact_tokens_t *ld, int x_off);

/* Moves cursor in wptr to y-coord y for a string len
 * of len */
int mvwxctr(WINDOW *wptr, int y, int len);

/* Prints str centered on the X axis of wptr and y-coord
 * of y */
int mvwxcprintw(WINDOW *wptr, int y, char *str);

/* Same as mvwxcprintw but for a digit value */
int mvwxcprintw_digit(WINDOW *wptr, int y, int d);

/* Creates all three (or two, if the sidebar width check does not return true),
 * for the nc_read_setup function in main.c. If the sidebar window is not
 * created, it is closed with delwin() and the pointer is set to NULL. */
struct ReadWins *create_read_windows(void);

/* Creates the subwindow for the read function */
WINDOW *create_lines_subwindow(int max_y, int max_x, int y_off, int x_off);

/* Creates the parent window to select a category, with dimensions based on
 * n categories but not larger in the Y axis than MAX_Y_CATG_SELECT */
WINDOW *create_category_select_parent(int n);

/* Creates the subwindow inside of wptr_parent for holding data */
WINDOW *create_category_select_subwindow(WINDOW *wptr_parent);

/* Creates a subwindow for user input with dimensions based on parameter n,
 * this function ensures that the window can center the data in the window. 
 * n is the number of menu items the window should hold. The window will at
 * least be y INPUT_WIN_ROWS units long. */
WINDOW *create_input_subwindow_n_rows(int n);

/* Creates a subwindow with n rows, these rows are NOT verified and there is 
 * no maximum or minumum value */
WINDOW *create_input_subwindow_force_rows(int n);

/* Creates a subwindow for user input with dimensions based on stdscr */
WINDOW *create_input_subwindow(void);

/* Show a message str in an input subwindow which exits on key press */
void nc_message(char *str);

/* Returns the color pair for the index 'x' of each category */
int category_color(int x);

/* Initializes the stdscr */
WINDOW *nc_init_stdscr(void);

/* Prints the welcome splash screen */
void nc_print_welcome(WINDOW *wptr);

/* Prints the string "DEBUG" on the bottom right of wptr */
void nc_print_debug_flag(WINDOW *wptr);

///* Prints the footer with menu options */
//void nc_print_footer(WINDOW *wptr, struct Footer *pf);

/* Prints standard F1-F4 options on wptr */
void nc_print_main_menu_footer(WINDOW *wptr);

void nc_print_read_footer(WINDOW *wptr);

/* Prints the footer with extended options on wptr */
void nc_print_extended_footer(WINDOW *wptr);

/* Prints the footer with only the quit key on, every other key is set to
 * A_DIM and extended mode is set to FALSE */
void nc_print_quit_footer(WINDOW *wptr);

void nc_print_input_footer(WINDOW *wptr);

#endif
