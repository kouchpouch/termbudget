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
 */
#include <ncurses.h>
#include <string.h>
#include <string.h>
#include <stdlib.h>
#include "tui.h"

enum FooterAttr {
	ON,
	DIM,
	OFF,
} footerattr;

struct Footer {
	bool extended;
	unsigned short add;
	unsigned short edit;
	unsigned short read;
	unsigned short quit;
};

static void window_creation_fail(void) {
	perror("Failed to create ncurses window");
	exit(1);
}

int test_terminal_size(void) {
	if (getmaxy(stdscr) < MIN_ROWS || getmaxx(stdscr) < MIN_COLUMNS) {
		mvprintw(0, 0, "Terminal is too small");
		refresh();
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

int calculate_overview_columns(WINDOW *wptr) {
	/* 
	 * There will be an expense and an income bar, each 1 column wide. =2
	 * There will also be text with the abbreviated month. =3
	 * Then there's the dollar amounts, which can vary but these can be offset
	 * vertically to not interfere with eachother. There also needs to be
	 * spacing between every month's bar graph. This should be dynamic but at
	 * least 2 columns.
	 *
	 * Therefore the minimum width to display the overview is:
	 * Minimum 7 per month to center the abbreviated month.
	 * 7 * 12 = 84 + 2 spaces on either side for the wptr_parent box.
	 * Total Minimum Columns = 86 on stdscr, 84 on wptr_data.
	 */

	int space = 0;
	if (getmaxx(wptr) >= 84) {
		space = getmaxx(wptr) / 12;
		if (space % 2 == 0) {
			if (((space + 1) * 12) < getmaxx(wptr)) {
				space += 1;
			} else {
				space -= 1;
			}
		}
	} else {
		return -1;
	}
	return space > 20 ? 20 : space;
}

void calculate_columns(struct ColWidth *cw, int max_x) {

	cw->date = DATE_X;
	cw->trns = TRNS_X;
	cw->amnt = AMNT_X;

	int static_columns = cw->date + cw->trns + cw->amnt;

	if (max_x < MIN_COLUMNS) {
		cw->date = 6;
		cw->trns = 5;
		int small_scr = cw->date + cw->trns + cw->amnt;
		cw->catg = (max_x - small_scr) / 3;
		cw->desc = (max_x - small_scr) / 3 + cw->catg;
	} else if ((max_x - static_columns) / 2 < 64) {
		cw->catg = (max_x - static_columns) / 3;
		cw->desc = (max_x - static_columns) / 3 + cw->catg;
	} else {
		cw->desc = 64;
		cw->catg = 64;
	}
}

void print_column_headers(WINDOW *wptr, int x_off) {
	struct ColWidth column_width, *cw = &column_width;
	

	int cur = x_off;

	calculate_columns(cw, getmaxx(wptr) - x_off);

	mvwprintw(wptr, 1, cur, "DATE");

	if (cw->catg <= CATG_X) {
		mvwprintw(wptr, 1, cur += cw->date, "CAT.");
	} else {
		mvwprintw(wptr, 1, cur += cw->date, "CATEGORY");
	}
	mvwprintw(wptr, 1, cur += cw->catg, "DESCRIPTION");
	mvwprintw(wptr, 1, cur += cw->desc, "TYPE");
	mvwprintw(wptr, 1, cur += cw->trns, "AMOUNT");
	mvwchgat(wptr, 1, x_off, getmaxx(wptr) - x_off * 2, A_REVERSE, 0, NULL);

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
		window_creation_fail();
	}
	keypad(wptr, true);
	return wptr;
}

WINDOW *create_category_select_parent(int n) {
	int win_y;
	if (n + BOX_OFFSET > getmaxy(stdscr) 
		&& n + BOX_OFFSET <= MAX_Y_CATG_SELECT) {
		win_y = getmaxy(stdscr) - BOX_OFFSET;
	} else if (n + BOX_OFFSET > MAX_Y_CATG_SELECT) {
		win_y = MAX_Y_CATG_SELECT;
	} else {
		win_y = n + BOX_OFFSET;
	}

	int win_x;
	if (getmaxx(stdscr) >= MAX_LEN_CATG) {
		win_x = MAX_LEN_CATG + BOX_OFFSET;
	} else {
		win_x = getmaxx(stdscr);
	}

	int begin_y = getmaxy(stdscr) / 2 - win_y / 2;
	int begin_x = getmaxx(stdscr) / 2 - win_x / 2;
	WINDOW *wptr = newwin(win_y, win_x, begin_y, begin_x);
	if (wptr == NULL) {
		window_creation_fail();
	}
	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Category");
	mvwxcprintw(wptr, getmaxy(wptr) - 1, 
				"Press 'c' to Create a Category");
	wrefresh(wptr);
	return wptr;
}

WINDOW *create_category_select_subwindow(WINDOW *wptr_parent) {
	WINDOW *wptr = newwin(getmaxy(wptr_parent) - BOX_OFFSET,
					   	  getmaxx(wptr_parent) - BOX_OFFSET,
					   	  getmaxy(stdscr) / 2 - getmaxy(wptr_parent) / 2 + 1, 
					   	  getmaxx(stdscr) / 2 - getmaxx(wptr_parent) / 2 + 1);
	if (wptr == NULL) {
		window_creation_fail();
	}

	return wptr;
}

WINDOW *create_input_subwindow(void) {
	int max_y, max_x;
	getmaxyx(stdscr, max_y, max_x);
	int win_y, win_x;
	
	win_y = INPUT_WIN_ROWS;

	if (max_x >= MIN_COLUMNS + 20) {
		win_x = MIN_COLUMNS + 20;
	} else {
		win_x = max_x;
	}

	WINDOW *wptr = newwin(win_y, win_x, 
					   	 (max_y / 2) - win_y / 2, 
					   	 (max_x / 2) - win_x / 2);

	if (wptr == NULL) {
		window_creation_fail();
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
	if (!has_colors()) {
		printw("This terminal does not support colors, press any key to quit");
		getch();
		endwin();
		exit(1);
	}
	start_color();
	use_default_colors();
	noecho(); cbreak(); keypad(stdscr, true);
	return stdscr;
}

void nc_print_welcome(WINDOW *wptr) {
	curs_set(0);
	mvwxcprintw(wptr, getmaxy(wptr) / 2, "Welcome to termBudget");
	mvwxcprintw(wptr, getmaxy(wptr) / 2 + 1, "github.com/kouchpouch");
}

void nc_print_debug_flag(WINDOW *wptr) {
	char debug_text[] = "DEBUG";
	wmove(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - (int)strlen(debug_text) - 1);
	init_pair(1, COLOR_RED, -1);
	wattron(wptr, COLOR_PAIR(1));
	wprintw(wptr, "%s", debug_text);
	wattroff(wptr, COLOR_PAIR(1));
}

static void nc_print_footer(WINDOW *wptr, struct Footer *pf) {
	int max_y, cur;
	max_y = getmaxy(wptr);
	//mvwchgat(wptr, max_y - 1, 0, getmaxx(wptr), A_INVIS, 0, NULL);

	/* Clear the footer line of any old menu items */
	mvwhline(wptr, max_y - 1, 0, A_INVIS, getmaxx(wptr) - 1);

	cur = 0;
	curs_set(0);

	char add_key[] = "F1 ";
	char add_text[] = "(A)dd";
	char edit_key[] = " F2 ";
	char edit_text[] = "(E)dit";
	char read_key[] = " F3 ";
	char read_text[] = "(R)ead";
	char quit_key[] = " F4 ";
	char quit_text[] = "(Q)uit";
	/* Extended */
	char sort_key[] = " F5 ";
	char sort_text[] = "(S)ort";
	char overview_key[] = " F6 ";
	char overview_text[] = "(O)verview";

	if (pf->add == OFF) {
		wattron(wptr, A_INVIS);
	}
	mvwprintw(wptr, max_y - 1, cur, "%s", add_key);
	if (pf->add == DIM) {
		wattron(wptr, A_DIM);
		wattron(wptr, A_REVERSE);
	} else if (pf->add == ON) {
		wattron(wptr, A_REVERSE);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(add_key), "%s", add_text);
	wattroff(wptr, A_REVERSE);
	wattroff(wptr, A_DIM);
	wattroff(wptr, A_INVIS);

	if (pf->edit == OFF) {
		wattron(wptr, A_INVIS);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(add_text), "%s", edit_key);
	if (pf->edit == DIM) {
		wattron(wptr, A_DIM);
		wattron(wptr, A_REVERSE);
	} else if (pf->edit == ON) {
		wattron(wptr, A_REVERSE);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(edit_key), "%s", edit_text);
	wattroff(wptr, A_REVERSE);
	wattroff(wptr, A_DIM);
	wattroff(wptr, A_INVIS);

	if (pf->read == OFF) {
		wattron(wptr, A_INVIS);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(edit_text), "%s", read_key);
	if (pf->read == DIM) {
		wattron(wptr, A_DIM);
		wattron(wptr, A_REVERSE);
	} else if (pf->read == ON) {
		wattron(wptr, A_REVERSE);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(read_key), "%s", read_text);
	wattroff(wptr, A_REVERSE);
	wattroff(wptr, A_DIM);
	wattroff(wptr, A_INVIS);

	if (pf->quit == OFF) {
		wattron(wptr, A_INVIS);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(read_text), "%s", quit_key);
	if (pf->quit == DIM) {
		wattron(wptr, A_DIM);
		wattron(wptr, A_REVERSE);
	} else if (pf->quit == ON) {
		wattron(wptr, A_REVERSE);
	}
	mvwprintw(wptr, max_y - 1, cur += strlen(quit_key), "%s", quit_text);
	wattroff(wptr, A_REVERSE);
	wattroff(wptr, A_DIM);
	wattroff(wptr, A_INVIS);

	if (pf->extended) {
		mvwprintw(wptr, max_y - 1, cur += strlen(quit_text), "%s", sort_key);
		wattron(wptr, A_REVERSE);
		mvwprintw(wptr, max_y - 1, cur += strlen(sort_key), "%s", sort_text);
		wattroff(wptr, A_REVERSE);

		mvwprintw(wptr, max_y - 1, cur += strlen(sort_text), "%s", overview_key);
		wattron(wptr, A_REVERSE);
		mvwprintw(wptr, max_y - 1, cur += strlen(overview_key), "%s", overview_text);
		wattroff(wptr, A_REVERSE);
	}
	wrefresh(wptr);
}

void nc_print_main_menu_footer(WINDOW *wptr) {
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = ON;
	pf->edit = DIM;
	pf->read = ON;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_read_footer(WINDOW *wptr) {
	struct Footer f, *pf = &f;
	pf->extended = true;
	pf->add = ON;
	pf->edit = ON;
	pf->read = ON;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_quit_footer(WINDOW *wptr) {
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = DIM;
	pf->edit = DIM;
	pf->read = DIM;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_input_footer(WINDOW *wptr) {
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = DIM;
	pf->edit = DIM;
	pf->read = DIM;
	pf->quit = DIM;
	nc_print_footer(wptr, pf);
}
