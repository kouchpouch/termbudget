#include <ncurses.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include "tui.h"

int test_terminal_size(int max_y, int max_x) {
	if (max_y < MIN_ROW || max_x < MIN_COLUMNS) {
			return -1;
		}
	return 0;
}

void nc_exit_window_key(WINDOW *wptr) {
	wrefresh(wptr);
	wgetch(wptr);
	wclear(wptr);
	wrefresh(wptr);
	delwin(wptr);
}

void nc_exit_window(WINDOW *wptr) {
	wclear(wptr);
	wrefresh(wptr);
	delwin(wptr);
}

void calculate_columns(struct ColumnWidth *cw) {

	/* DATE: 12/31/2025___
	 *       ^...........^ == 13 */
	cw->date = 13; 

	/* TRANSACTION TYPE: EXPENSE___
	 *                   ^........^  == 10 */
	cw->trns = 10;

	/* AMOUNT: Max 9 digits plus decimal point, plus 1 space, 11 */
	cw->amnt = 11;

	int static_columns = cw->date + cw->trns + cw->amnt;

	/* 11 'n 14 derived from the length of "CATEGORY" and "DESCRIPTION" + 3 */
	int minimum_cols = static_columns + 11 + 14;

	if (cw->max_x < minimum_cols) {
		cw->catg = 0;	
		cw->desc = 0;	
	} else if ((cw->max_x - static_columns) / 2 < 64) {
		cw->catg = (cw->max_x - static_columns) / 4;
		cw->desc = (cw->max_x - static_columns) / 2 + cw->catg;
	} else {
		cw->desc = 64;
		cw->catg = 64;
	}
}

void print_column_headers(WINDOW *wptr, int x_off) {
	struct ColumnWidth column_width, *cw = &column_width;
	cw->max_x = getmaxx(wptr);
	cw->max_x -= x_off;
	
	calculate_columns(cw);

	int cur = x_off;

	mvwprintw(wptr, 1, cur, "DATE");
	mvwprintw(wptr, 1, cur += cw->date, "CATEGORY");
	mvwprintw(wptr, 1, cur += cw->catg, "DESCRIPTION");
	mvwprintw(wptr, 1, cur += cw->desc, "TYPE");
	mvwprintw(wptr, 1, cur += cw->trns, "AMOUNT");
	mvwchgat(wptr, 1, x_off, cw->max_x - 2, A_REVERSE, 0, NULL);

	wrefresh(wptr);
}

int mvwxctr(WINDOW *wptr, int y, int len) {
	int ret = wmove(wptr, y, getmaxx(wptr) / 2 - len / 2);
	return ret;
}

int mvwxcprintw(WINDOW *wptr, int y, char *str) {
	if (mvwxctr(wptr, y, strlen(str)) < 0) return -1;
	int ret = wprintw(wptr, "%s", str);
	return ret;
}

WINDOW *create_input_subwindow() {
	int max_y, max_x;
	getmaxyx(stdscr, max_y, max_x);
	int win_y, win_x;
	
	win_y = 8;

	if (max_x >= MIN_COLUMNS + 20) {
		win_x = MIN_COLUMNS + 20;
	} else {
		win_x = max_x;
	}

	WINDOW *wptr = newwin(win_y, win_x, (max_y / 2) - win_y / 2, (max_x / 2) - win_x / 2);
	box(wptr, 0, 0);
	return wptr;
}

WINDOW *nc_init_stdscr() {
	WINDOW *wptr = initscr(); 
	if (wptr == NULL) {
		return NULL;
	}
	noecho(); cbreak(); keypad(stdscr, true);
	return wptr;
}

void nc_print_welcome(WINDOW *wptr) {
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	curs_set(0);

	char welcome[] = "Welcome to termBudget";
	char welcome2[] = "Made by TN";

	mvwxcprintw(wptr, max_y/2, "Welcome to termBudget");
	mvwxcprintw(wptr, max_y/2 + 1, "Made by TN");
}

void nc_print_footer(WINDOW *wptr) {
	int max_y, max_x, cur;
	getmaxyx(wptr, max_y, max_x);

	cur = 0;
	curs_set(0);

	char add_key[] = "F1 ";
	char add_text[] = "Add";
	char edit_key[] = " F2 ";
	char edit_text[] = "Edit";
	char read_key[] = " F3 ";
	char read_text[] = "Read";
	char quit_key[] = " F4 ";
	char quit_text[] = "Quit";

	mvwprintw(wptr, max_y - 1, cur, "%s", add_key);
	wattron(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(add_key), "%s", add_text);
	wattroff(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(add_text), "%s", edit_key);
	wattron(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(edit_key), "%s", edit_text);
	wattroff(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(edit_text), "%s", read_key);
	wattron(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(read_key), "%s", read_text);
	wattroff(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(read_text), "%s", quit_key);
	wattron(wptr, A_REVERSE);
	mvwprintw(wptr, max_y - 1, cur += strlen(quit_key), "%s", quit_text);
	wattroff(wptr, A_REVERSE);
}
