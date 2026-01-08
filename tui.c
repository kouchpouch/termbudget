#include <ncurses.h>
#include <string.h>
#include "tui.h"

int test_terminal_size(int max_y, int max_x) {
	if (max_y < MIN_ROW || max_x < MIN_COL) {
			return -1;
		}
	return 0;
}

WINDOW *nc_new_win() {
	WINDOW *wptr = initscr(); 
	noecho(); cbreak(); keypad(stdscr, true);

	return wptr;
}

void nc_print_welcome(WINDOW *wptr) {
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	curs_set(0);

	char welcome[] = "Welcome to termBudget";
	char welcome2[] = "Made by TN";

	mvwprintw(wptr, max_y/2, max_x/2 - strlen(welcome)/2, "%s", welcome);
	mvwprintw(wptr, max_y/2 + 1, max_x/2 - strlen(welcome2)/2, "%s", welcome2);
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
