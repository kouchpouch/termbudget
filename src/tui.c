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

#include "parser.h"
#include "tui_sidebar.h"

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

void window_creation_fail(void)
{
	perror("Failed to create ncurses window");
	exit(1);
}

int test_terminal_size(void)
{
	if (getmaxy(stdscr) < MIN_ROWS || getmaxx(stdscr) < MIN_COLUMNS) {
		mvprintw(0, 0, "Terminal is too small");
		refresh();
		return -1;
	}
	return 0;
}

void nc_exit_window_key(WINDOW *wptr)
{
	wrefresh(wptr);
	wgetch(wptr);
	wclear(wptr);
	wrefresh(wptr);
	delwin(wptr);
}

void nc_exit_window(WINDOW *wptr)
{
	wclear(wptr);
	wrefresh(wptr);
	delwin(wptr);
}

void clear_input_error_message(WINDOW *wptr)
{
	wmove(wptr, getmaxy(wptr) - 4, 0);
	wclrtobot(wptr);
	box(wptr, 0, 0);
	wrefresh(wptr);
}

void calculate_columns(struct ColWidth *cw, int max_x)
{
	int static_columns;
	int small_scr;

	cw->amnt = AMNT_X;

	if (max_x < MIN_COLUMNS) {
		cw->trns = strlen("+- ");
		cw->date = strlen("00/00 ");
	} else {
		cw->trns = TRNS_X;
		cw->date = DATE_X;
	}

	static_columns = cw->date + cw->trns + cw->amnt;

	if (max_x < MIN_COLUMNS + SHORTEN_THRESH) {
		cw->date = strlen("00/00 ");
		cw->trns = strlen("+- ");
		small_scr = cw->date + cw->trns + cw->amnt;
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

void print_column_headers(WINDOW *wptr, int x_off)
{
	struct ColWidth column_width, *cw = &column_width;
	int cur = x_off;
	int y = 1;

	calculate_columns(cw, getmaxx(wptr) - x_off);

	mvwprintw(wptr, y, cur, "DATE");

	if (cw->catg < CATG_X * 2) {
		mvwprintw(wptr, y, cur += cw->date, "CATG");
	} else {
		mvwprintw(wptr, y, cur += cw->date, "CATEGORY");
	}

	if (cw->desc < DESC_X * 2) {
		mvwprintw(wptr, y, cur += cw->catg, "DESC");
	} else {
		mvwprintw(wptr, y, cur += cw->catg, "DESCRIPTION");
	}

	if (cw->trns < TRNS_X) {
		mvwprintw(wptr, y, cur += cw->desc, "+-");
	} else {
		mvwprintw(wptr, y, cur += cw->desc, "TYPE");
	}

	mvwprintw(wptr, y, cur += cw->trns, "AMOUNT");

	mvwchgat(wptr, y, x_off, getmaxx(wptr) - x_off * 2, A_REVERSE, 0, NULL);

	wrefresh(wptr);
}

/* Prints record from ld, in vertical format, 5 rows. */
void nc_print_record_vert(WINDOW *wptr, _transact_tokens_t *ld, int x_off)
{
	int y = 1; /* Y-coord to print on, to start. */
	mvwprintw(wptr, y++, x_off, "Date--> %d/%d/%d", ld->month, ld->day, ld->year);
	mvwprintw(wptr, y++, x_off, "Cat.--> %s", ld->category);
	mvwprintw(wptr, y++, x_off, "Desc--> %s", ld->desc);
	mvwprintw(wptr, y++, x_off, "Type--> %s", ld->transtype == 0 ? "Expense" : "Income");
	mvwprintw(wptr, y++, x_off, "Amt.--> %.2f", ld->amount);
}

int mvwxctr(WINDOW *wptr, int y, int len)
{
	int ret = wmove(wptr, y, getmaxx(wptr) / 2 - len / 2);
	return ret;
}

int mvwxcprintw(WINDOW *wptr, int y, char *str)
{
	if (mvwxctr(wptr, y, strlen(str)) < 0) return -1;
	int ret = wprintw(wptr, "%s", str);
	return ret;
}

int mvwxcprintw_digit(WINDOW *wptr, int y, int d)
{
	if (mvwxctr(wptr, y, intlen(d)) < 0) return -1;
	int ret = wprintw(wptr, "%d", d);
	return ret;
}

static bool verify_sidebar_width(WINDOW *wptr)
{
	if (getmaxx(wptr) >= MIN_COLUMNS_SIDEBAR) {
		return true;
	}
	return false;
}

WINDOW *create_lines_subwindow(int max_y, int max_x, int y_off, int x_off)
{
	WINDOW *wptr = newwin(max_y - y_off * 2, max_x - x_off * 2, y_off + 1, x_off);
	if (wptr == NULL) {
		window_creation_fail();
	}
	keypad(wptr, true);
	return wptr;
}

struct ReadWins *create_read_windows(void)
{
	struct ReadWins *wins = malloc(sizeof(*wins));
	int parent_y, parent_x;
	int y, x;
	getmaxyx(stdscr, y, x);
	bool width = verify_sidebar_width(stdscr);
	int y_off = 1;
	int x_off = BOX_OFFSET;

	if (width) {
		wins->parent = newwin(y - 1, x - SIDEBAR_COLUMNS, 0, 0);
		if (wins->parent == NULL) {
			window_creation_fail();
		}
	} else {
		wins->parent = newwin(y - 1, 0, 0, 0);
	}

	getmaxyx(wins->parent, parent_y, parent_x);

	if (width) {
		wins->sidebar_parent = create_sidebar_parent(wins->parent, y, x);
		wins->sidebar_body = create_sidebar_body(wins->parent, wins->sidebar_parent);
	} else {
		wins->sidebar_parent = NULL;
		wins->sidebar_body = NULL;
	}

	wins->data = newwin(parent_y - 1 - y_off * 2, parent_x - x_off * 2, y_off + 1, x_off);
	if (wins->data == NULL) {
		window_creation_fail();
	}

	keypad(wins->parent, true);
	keypad(wins->data, true);

	return wins;
}

WINDOW *create_category_select_parent(int n)
{
	int win_y, win_x;
	int begin_y, begin_x;
	WINDOW *wptr;

	if (n + BOX_OFFSET > getmaxy(stdscr) 
		&& n + BOX_OFFSET <= MAX_Y_CATG_SELECT) {
		win_y = getmaxy(stdscr) - BOX_OFFSET;
	} else if (n + BOX_OFFSET > MAX_Y_CATG_SELECT) {
		win_y = MAX_Y_CATG_SELECT;
	} else {
		win_y = n + BOX_OFFSET;
	}

	if (getmaxx(stdscr) >= MAX_LEN_CATG) {
		win_x = MAX_LEN_CATG + BOX_OFFSET;
	} else {
		win_x = getmaxx(stdscr);
	}

	begin_y = getmaxy(stdscr) / 2 - win_y / 2;
	begin_x = getmaxx(stdscr) / 2 - win_x / 2;
	wptr = newwin(win_y, win_x, begin_y, begin_x);
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

WINDOW *create_category_select_subwindow(WINDOW *wptr_parent)
{
	WINDOW *wptr = newwin(getmaxy(wptr_parent) - BOX_OFFSET,
					   	  getmaxx(wptr_parent) - BOX_OFFSET,
					   	  getmaxy(stdscr) / 2 - getmaxy(wptr_parent) / 2 + 1, 
					   	  getmaxx(stdscr) / 2 - getmaxx(wptr_parent) / 2 + 1);
	if (wptr == NULL) {
		window_creation_fail();
	}

	return wptr;
}

WINDOW *create_input_subwindow_n_rows(int n)
{
	int max_y, max_x;
	int win_y, win_x;
	WINDOW *wptr;
	
	getmaxyx(stdscr, max_y, max_x);

	if (n <= INPUT_WIN_ROWS + BOX_OFFSET) {
		if (n % 2 == 0) {
			win_y = INPUT_WIN_ROWS;
		} else {
			win_y = INPUT_WIN_ROWS + 1;
		}
	} else {
		if (n % 2 == 0) {
			win_y = n + BOX_OFFSET;
		} else {
			win_y = n + BOX_OFFSET + 1;
		}
	}

	if (max_x >= MIN_COLUMNS + 20) {
		win_x = MIN_COLUMNS + 20;
	} else {
		win_x = max_x;
	}

	wptr = newwin(win_y, win_x, (max_y / 2) - win_y / 2, (max_x / 2) - win_x / 2);

	if (wptr == NULL) {
		window_creation_fail();
	}

	box(wptr, 0, 0);
	keypad(wptr, true);
	return wptr;
}

WINDOW *create_input_subwindow(void)
{
	int max_y, max_x;
	int win_y, win_x;
	WINDOW *wptr;
	
	getmaxyx(stdscr, max_y, max_x);
	win_y = INPUT_WIN_ROWS;

	if (max_x >= MIN_COLUMNS + 20) {
		win_x = MIN_COLUMNS + 20;
	} else {
		win_x = max_x;
	}

	wptr = newwin(win_y, win_x, (max_y / 2) - win_y / 2, (max_x / 2) - win_x / 2);

	if (wptr == NULL) {
		window_creation_fail();
	}

	box(wptr, 0, 0);
	keypad(wptr, true);
	return wptr;
}

void nc_message(char *str)
{
	WINDOW *wptr_msg = create_input_subwindow();
	mvwxcprintw(wptr_msg, 3, str);
	wrefresh(wptr_msg);
	nc_exit_window_key(wptr_msg);
}

int category_color(int x)
{
	return x % 10 + 11;
}

static void init_color_palette(void)
{
	// These colors were picked by a clanker, the only thing in the entire
	// program that used the devil.
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);
	init_pair(3, COLOR_YELLOW, -1);
	init_pair(11, 75, -1);  // soft blue
	init_pair(12, 69, -1);  // muted indigo
	init_pair(13, 111, -1); // lavender
	init_pair(14, 147, -1); // soft violet
	init_pair(15, 121, -1); // mint
	init_pair(16, 79, -1);  // seafoam
	init_pair(17, 108, -1); // sage
	init_pair(18, 216, -1); // peach
	init_pair(19, 215, -1); // soft orange
	init_pair(20, 222, -1); // light gold
	init_pair(REVERSE_COLOR, 251, -1);
}

WINDOW *nc_init_stdscr(void)
{
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
	init_color_palette();
	noecho(); cbreak(); keypad(stdscr, true);
	return stdscr;
}

static int print_logo(WINDOW *wptr)
{
	char *logo[8];
	int start_y = 7;
	int print_y;
	int print_x;
	int offset;
	int ln = 0;
	logo[ln++] = "   ||   \n";
	logo[ln++] = " s$||$$ \n";
	logo[ln++] = "$  ||  $\n";
	logo[ln++] = " *$||   \n";
	logo[ln++] = "   ||$s \n";
	logo[ln++] = "$  ||  $\n";
	logo[ln++] = " $$||$* \n";
	logo[ln++] = "   ||   \n";

	offset = strlen(logo[0]);

	for (int i = 0; i < ln; i++, start_y--) {
		print_y = (getmaxy(wptr) / 2) - start_y;
		print_x = getmaxx(wptr) / 2;

		/* Print ascii art */
		mvwprintw(wptr, print_y, print_x - offset / 2, "%s", logo[i]);
		/* Color the left 3 chars green */
		mvwchgat(wptr, print_y, print_x - offset / 2, 3, A_NORMAL, 2, NULL);
		/* Color the middle 2 chars yellow */
		mvwchgat(wptr, print_y, print_x - 1, 2, A_NORMAL, 3, NULL);
		/* Color the right 3 chars green */
		mvwchgat(wptr, print_y, print_x + 1, 3, A_NORMAL, 2, NULL);
	}

	wrefresh(wptr);

	return getcury(wptr);
}

void nc_print_welcome(WINDOW *wptr)
{
	int y = print_logo(wptr);
	y++;
	mvwxcprintw(wptr, ++y, "termBudget");
	mvwxcprintw(wptr, ++y, "github.com/kouchpouch");
	curs_set(0);
}

void nc_print_debug_flag(WINDOW *wptr)
{
	char debug_text[] = "DEBUG";
	wmove(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - (int)strlen(debug_text) - 1);
	wattron(wptr, COLOR_PAIR(1));
	wprintw(wptr, "%s", debug_text);
	wattroff(wptr, COLOR_PAIR(1));
}

static void nc_print_footer(WINDOW *wptr, struct Footer *pf)
{
	int max_y, cur;
	max_y = getmaxy(wptr);
	//mvwchgat(wptr, max_y - 1, 0, getmaxx(wptr), A_INVIS, 0, NULL);

	/* Clear the footer line of any old menu items */
	wmove(wptr, max_y - 1, 0);
	wclrtoeol(wptr);

	cur = 0;
	curs_set(0);

	char *add_key = "F1 ";
	char *add_text = "(A)dd";
	char *edit_key = " F2 ";
	char *edit_text = "(E)dit";
	char *read_key = " F3 ";
	char *read_text = "(R)ead";
	char *quit_key = " F4 ";
	char *quit_text = "(Q)uit";
	/* Extended */
	char *sort_key = " F5 ";
	char *sort_text = "(S)ort";
	char *overview_key = " F6 ";
	char *overview_text = "(O)verview";

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

void nc_print_main_menu_footer(WINDOW *wptr)
{
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = ON;
	pf->edit = DIM;
	pf->read = ON;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_read_footer(WINDOW *wptr)
{
	struct Footer f, *pf = &f;
	pf->extended = true;
	pf->add = ON;
	pf->edit = ON;
	pf->read = ON;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_quit_footer(WINDOW *wptr)
{
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = DIM;
	pf->edit = DIM;
	pf->read = DIM;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}

void nc_print_input_footer(WINDOW *wptr)
{
	struct Footer f, *pf = &f;
	pf->extended = false;
	pf->add = DIM;
	pf->edit = DIM;
	pf->read = DIM;
	pf->quit = ON;
	nc_print_footer(wptr, pf);
}
