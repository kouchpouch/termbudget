#include <ncurses.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include "tui.h"

int test_terminal_size(int max_y, int max_x) {
	if (max_y < MIN_ROWS || max_x < MIN_COLUMNS) {
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

void clear_input_error_message(WINDOW *wptr) {
	wmove(wptr, getmaxy(wptr) - 4, 0);
	wclrtobot(wptr);
	box(wptr, 0, 0);
	wrefresh(wptr);
}

void calculate_columns(struct ColumnWidth *cw) {

	cw->date = DATE_X;
	cw->trns = TRNS_X;
	cw->amnt = AMNT_X;

	int static_columns = cw->date + cw->trns + cw->amnt;

	if (cw->max_x < MIN_COLUMNS) {
		cw->date = 6;
		cw->trns = 5;
		int small_scr = cw->date + cw->trns + cw->amnt;
		cw->catg = (cw->max_x - small_scr) / 3;
		cw->desc = (cw->max_x - small_scr) / 3 + cw->catg;
	} else if ((cw->max_x - static_columns) / 2 < 64) {
		cw->catg = (cw->max_x - static_columns) / 3;
		cw->desc = (cw->max_x - static_columns) / 3 + cw->catg;
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

	if (cw->catg <= CATG_X) {
		mvwprintw(wptr, 1, cur += cw->date, "CAT.");
	} else {
		mvwprintw(wptr, 1, cur += cw->date, "CATEGORY");
	}
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

WINDOW *create_lines_subwindow(int max_y, int max_x, int y_off, int x_off) {
	WINDOW *wptr = newwin(max_y - y_off * 2, max_x - x_off * 2, y_off + 1, x_off);
	if (wptr == NULL) {
		perror("Failed to create ncurses window");
	}
	keypad(wptr, true);
	return wptr;
}

WINDOW *create_input_subwindow(void) {
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

	if (wptr == NULL) {
		perror("Failed to create ncurses window");
		return NULL;
	}

	box(wptr, 0, 0);
	return wptr;
}

void nc_message(char *str) {
	WINDOW *wptr_msg = create_input_subwindow();
	mvwxcprintw(wptr_msg, 3, str);
	wrefresh(wptr_msg);
	nc_exit_window_key(wptr_msg);
}

WINDOW *nc_init_stdscr(void) {
	stdscr = initscr(); 
	if (stdscr == NULL) {
		return NULL;
	}
	noecho(); cbreak(); keypad(stdscr, true);
	return stdscr;
}

void nc_print_welcome(WINDOW *wptr) {
	curs_set(0);
	mvwxcprintw(wptr, getmaxy(wptr) / 2, "Welcome to termBudget");
	mvwxcprintw(wptr, getmaxy(wptr) / 2 + 1, "Made by TN");
}

void nc_print_debug_flag(WINDOW *wptr) {
	char debug_text[] = "DEBUG";
	wmove(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - (int)strlen(debug_text) - 1);
	wprintw(wptr, "%s", debug_text);
}

void nc_print_footer(WINDOW *wptr) {
	int max_y, cur;
	max_y = getmaxy(wptr);

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
