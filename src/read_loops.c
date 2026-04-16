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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ncurses.h>
#include <assert.h>
#include <limits.h>

#include "read_loops.h"
#include "read_init.h"
#include "main.h"
#include "edit_categories.h"
#include "helper.h"
#include "tui.h"
#include "tui_sidebar.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "flags.h"

struct ScrollCursor {
	/* Total size of the data that can be shown. */
	int total_rows; 

	/* Total number of records and/or categories displayed on the screen. */
	int displayed; 
	int select_idx;
	int cur_y;
	int catg_node;
	int catg_data;
	size_t sidebar_idx;
};

struct DispCursor {
	int first;
	int last;
};

static void print_debug_line(WINDOW *wptr, struct ScrollCursor *sc)
{
	mvwhline(wptr, getmaxy(wptr) - 1, 1, 0, getmaxx(wptr) - 2);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 55, "SELIDX: %d", sc->select_idx);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 40, "DATA: %d", sc->displayed);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 30, "DATA: %d", sc->catg_data);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 20, "NODE: %d", sc->catg_node);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 10, "CURS: %d", sc->cur_y);
	wrefresh(wptr);
}

static void print_catg_balances
(WINDOW *wptr, int tt, double planned, double exp, int width)
{
	// Safe cast, we know these strings aren't greater than INT_MAX
	char *full_inc_string = "Planned: $, Received: $";
	char *full_exp_string = "Planned: $, Remaining: $";
	char *short_inc_string = "Plan: $, Rcvd: $";
	char *short_exp_string = "Plan: $, Rem: $";
	char *abbreviated = "P$, R$";

	int full_inc_len = (int)strlen(full_inc_string);
	int full_exp_len = (int)strlen(full_exp_string);
	int short_inc_len = (int)strlen(short_inc_string);
	int short_exp_len = (int)strlen(short_exp_string);
	int abbreviated_len = (int)strlen(abbreviated);

	double remaining;

	if (exp > 0) {
		remaining = exp - planned;
	} else {
		remaining = planned + exp;
	}

	if (tt == TT_INCOME) {
		if (full_inc_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "Planned: $%.2f, Received: $%.2f", planned, exp);
		} else if (short_inc_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "Plan: $%.2f, Rcvd: $%.2f", planned, exp);
		} else if (abbreviated_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "P$%.2f, R$%.2f", planned, exp);
		}
	} else if (tt == TT_EXPENSE) {
		if (full_exp_len + finlen(planned) + finlen(remaining) < width) {
			//wprintw(wptr, "Plan: $%.2f, Rem: $%.2f, Exp: $%.2f", planned, remaining, exp);
			wprintw(wptr, "Planned: $%.2f, Remaining: $%.2f", planned, remaining);
		} else if (short_exp_len + finlen(planned) + finlen(remaining) < width) {
			wprintw(wptr, "Plan: $%.2f, Rem: $%.2f", planned, remaining);
		} else if (abbreviated_len + finlen(planned) + finlen(remaining) < width) {
			wprintw(wptr, "P$%.2f, R$%.2f", planned, remaining);
		}
	}
}

static bool shorten_string(WINDOW *wptr)
{
	if (getmaxx(wptr) + BOX_OFFSET < MIN_COLUMNS + SHORTEN_THRESH) {
		return true;
	} else {
		return false;
	}
}

/* 
 * Prints record from ld, formatting in columns from cw, to a window pointed
 * to by wptr, at a Y-coordinate of y. Truncates desc and category strings
 * if the window is too small.
 */
static void print_record_hr
(WINDOW *wptr, struct ColWidth *cw, struct LineData *ld, int y)
{
	char *etc = "..";
	int lenetc = (int)strlen(etc);
	int x = 0;
	bool shorten = shorten_string(wptr);
	wmove(wptr, y, x);
	if (shorten) {
		wprintw(wptr, "%d/%d", ld->month, ld->day);
	} else {
		wprintw(wptr, "%d/%d/%d", ld->month, ld->day, ld->year);
	}

	wmove(wptr, y, x += cw->date);
	if ((int)strlen(ld->category) > cw->catg - lenetc) {
		if (getmaxx(wptr) < MIN_COLUMNS) {
			wprintw(wptr, "%.*s%s", cw->catg - lenetc, ld->category, etc);
		} else {
			wprintw(wptr, "%.*s%s", cw->catg - lenetc, ld->category, etc);
		}
	} else {
		wprintw(wptr, "%s", ld->category);
	}

	wmove(wptr, y, x += cw->catg);
	if ((int)strlen(ld->desc) > cw->desc - lenetc) {
		wprintw(wptr, "%.*s%s", cw->desc - lenetc, ld->desc, etc);
	} else {
		wprintw(wptr, "%s", ld->desc);
	}

	wmove(wptr, y, x += cw->desc);
	if (shorten) {
		wprintw(wptr, "%s", ld->transtype == 0 ? "-" : "+");
	} else {
		wprintw(wptr, "%s", ld->transtype == 0 ? "Expense" : "Income");
	}

	wmove(wptr, y, x += cw->trns);
	wprintw(wptr, "$%.2f", ld->amount);
}

static void print_category_hr
(WINDOW *wptr, struct ColWidth *cw, struct BudgetTokens *bt, int y)
{
	char *etc = "..";
	int lenetc = (int)strlen(etc);
	int x = 0;
	int print_offset = 0;
	double e = get_expenditures_per_category(bt);
	wattron(wptr, A_REVERSE);

	/* Move cursor past the date columns */
	wmove(wptr, y, x += cw->date - print_offset);
	if ((int)strlen(bt->catg) > cw->catg - lenetc) {
		wprintw(wptr, "%.*s%s", cw->catg - lenetc, bt->catg, etc);
	} else {
		wprintw(wptr, "%s", bt->catg);
	}

	/* Move cursor past the category column */
	wmove(wptr, y, x += cw->catg - print_offset);
	print_catg_balances(wptr, bt->transtype, bt->amount, e, cw->desc);

	wmove(wptr, y, x += cw->desc - print_offset);

	if (shorten_string(wptr)) {
		wprintw(wptr, "%s", bt->transtype == 0 ? "-" : "+");
	} else {
		wprintw(wptr, "%s", bt->transtype == 0 ? "Expenses" : "Income");
	}

	wattroff(wptr, A_REVERSE);
}

/* Returns the total number of nodes in 'nodes' */
static int get_total_nodes(CategoryNode **nodes)
{
	int n = 1;
	for (int i = 0; nodes[i]->next != NULL; i++) {
		n++;
	}
	return n;
}

static void print_init_budget_loop
(WINDOW *wptr, struct ScrollCursor *sc, CategoryNode **nodes, 
 struct ColWidth *cw, FILE *fptr)
{
	int max_y = getmaxy(wptr);
	int total_nodes = get_total_nodes(nodes);
	char *line_str;
	char linebuff[LINE_BUFFER];
	struct LineData ld;

	/* 
	 * For each category print the budget line and the records that match 
	 * the category. Increment displayed to keep track of how many lines are
	 * displayed (this is to keep track of scrolling).
	 */
	for (int i = 0; sc->displayed < max_y && sc->displayed < sc->total_rows 
		 && i < total_nodes; i++) 
	{
		struct BudgetTokens *bt = tokenize_budget_fpi(nodes[i]->catg_fp);
		print_category_hr(wptr, cw, bt, sc->displayed);
		mvwchgat(wptr, sc->displayed, 0, -1, A_NORMAL, category_color(i), NULL); 
		sc->displayed++;

		for (size_t j = 0; sc->displayed < max_y && sc->displayed < sc->total_rows 
		     && j < nodes[i]->data->size; j++)
		{
			fseek(fptr, nodes[i]->data->data[j], SEEK_SET);
			line_str = fgets(linebuff, sizeof(linebuff), fptr);
			tokenize_record(&ld, &line_str);
			print_record_hr(wptr, cw, &ld, sc->displayed);
			free_tokenized_record_strings(&ld);
			sc->displayed++;
		}

		free_budget_tokens(bt);
	}

	wmove(wptr, 0, 0);
	mvwchgat(wptr, sc->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (debug_flag) {
		curs_set(1);
	}

	wrefresh(wptr);
}

/*
 * Returns the value of the total number of rows to display in 
 * nc_read_budget_loop() to handle scrolling.
 *
 * The return value is calculated by adding up add CategoryNodes and their
 * data member's size.
 */
static int get_total_displayed_rows(CategoryNode **nodes)
{
	int rows = 0;
	int i = 0;

	while (1) {
		if (nodes[i]->next == NULL) {
			rows += 1 + nodes[i]->data->size;
			break;
		} else {
			rows += 1 + nodes[i]->data->size;
		}
		i++;
	}

	return rows;
}

static void init_scroll_cursor(struct ScrollCursor *sc, CategoryNode **nodes) 
{
	sc->total_rows = get_total_displayed_rows(nodes);
	sc->displayed = 0;
	sc->select_idx = 0; // Tracks selection for indexing
	sc->cur_y = 0; // Tracks cursor position in window
	sc->catg_node = 0; // Tracks which node the cursor is on
	/* Tracks which member record the cursor is on, begins at -1 to mark that
	 * the first cursor position is a node. */
	sc->catg_data = -1;
	sc->sidebar_idx = 0;
}

static void print_balances_text(WINDOW *wptr, Vec *psc)
{
	struct Balances pb_, *pb = &pb_;
	calculate_balance(pb, psc);
	int total_len = intlen(pb->income) + intlen(pb->expense) + strlen("Expenses: $.00 Income: $.00");
	mvwprintw(wptr, 0, getmaxx(wptr) / 2 - total_len / 2, 
		   	  "Income: $%.2f Expenses: $%.2f", pb->income, pb->expense);
	wrefresh(wptr);
}

/* Draws a border around the window 'wptr' with "T" shaped characters to
 * mesh seamlessly with the window border of the sidebar. */
//static void draw_parent_box_with_sidebar(WINDOW *wptr) {
//	wborder(wptr, 0, 0, 0, 0, 0, ACS_TTEE, 0, ACS_BTEE);
//	wrefresh(wptr);
//}

/* Draws all of the window borders, then the border text on top. Call this
 * function any time the borders/text need to be updated. */
static void draw_read_window_borders_and_text
(struct ReadWins *wins, Vec *psc)
{
	// Draw borders in order for correct intersecting lines
	if (wins->sidebar_parent != NULL || wins->sidebar_body != NULL) {
		draw_sidebar_parent_border(wins->sidebar_parent);
		draw_body_border(wins->sidebar_body);
	} else {
		print_balances_text(wins->parent, psc);
	}
}

/*
 * Creates a sub window inside of wptr to display a line-by-line format of
 * the selected record at index i of pidx->data. Following the format style
 * from edit_transaction()
 */
static int show_detail_subwindow(char *line)
{
	WINDOW *wptr_detail = create_input_subwindow();
	int y = getmaxy(wptr_detail);

	box(wptr_detail, 0, 0);
	mvwxcprintw(wptr_detail, 0, "Details");

	struct LineData linedata_, *ld = &linedata_;
	tokenize_record(ld, &line);

	nc_print_record_vert(wptr_detail, ld, BOX_OFFSET);

	free_tokenized_record_strings(ld);
	nc_exit_window_key(wptr_detail);
	return y;
}

static void refresh_displayed_counter
(WINDOW *wptr, struct ScrollCursor *sc, struct DispCursor *dc)
{
	mvwhline(wptr, getmaxy(wptr) - 1, BOX_OFFSET, 0, getmaxx(wptr) - (BOX_OFFSET + 1));
	wrefresh(wptr);

	// Calc length of, for example, "32-50 of 80 records"
	char *disp = " -  of  records";
	int x_offset = 
		strlen(disp) + intlen(sc->displayed) + intlen(sc->total_rows) + BOX_OFFSET;

	if (sc->displayed < sc->total_rows) {
		mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - x_offset,
			"%d-%d of %d records", dc->first, dc->last, sc->total_rows);
		wrefresh(wptr);
	}
}

static void nc_scroll_prev
(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw, bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}

	if (catg) {
		struct BudgetTokens *bt = tokenize_budget_fpi(b);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		print_category_hr(wptr, cw, bt, 0);
		free_budget_tokens(bt);
	} else {
		struct LineData linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		print_record_hr(wptr, cw, ld, 0);
		free_tokenized_record_strings(ld);
	}
}

static void nc_scroll_next
(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw, bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str;
	line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}

	if (catg) {
		struct BudgetTokens *bt = tokenize_budget_fpi(b);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		print_category_hr(wptr, cw, bt, getmaxy(wptr) - 1);
		free_budget_tokens(bt);
	} else {
		struct LineData linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		print_record_hr(wptr, cw, ld, getmaxy(wptr) - 1);
		free_tokenized_record_strings(ld);
	}
}

/* Returns 1 if the text was scrolled down, 0 if a normal scroll occured */
static int nc_scroll_prev_read_loop
(WINDOW *wptr, struct ScrollCursor *sc, struct ColWidth *cw, FILE *fptr, 
 Vec *psc)
{
	int retval = 0;

	mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
	sc->cur_y--;
	sc->select_idx--;

	if (sc->cur_y < 0) {
		sc->cur_y = -1;
	}
	
	/* For safe cast */
	assert(sc->displayed >= 0);

	if (sc->select_idx >= 0 && (size_t)sc->displayed < psc->size && sc->cur_y == -1) {
		nc_scroll_prev(psc->data[sc->select_idx], fptr, wptr, cw, false);
		sc->cur_y = getcury(wptr);
		retval++;
	}
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured */
static int nc_scroll_next_read_loop
(WINDOW *wptr, struct ScrollCursor *sc, struct ColWidth *cw, FILE *fptr, 
 Vec *psc)
{
	int retval = 0;

	mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
	sc->cur_y++;
	sc->select_idx++;

	/* For safe cast */
	assert(sc->displayed >= 0);

	if ((size_t)sc->displayed < psc->size && sc->cur_y == getmaxy(wptr)) {
		nc_scroll_next(psc->data[sc->select_idx], fptr, wptr, cw, false);
		sc->cur_y = getcury(wptr);
		retval++;
	}
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured, -1
 * if no scroll occured. */
static int nc_scroll_prev_category
(WINDOW *wptr, CategoryNode **nodes, struct ScrollCursor *sc, 
 struct ColWidth *cw, FILE *rfptr, FILE *bfptr)
{
	int retval = -1;

	if (sc->catg_data < 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		sc->catg_node--;
		sc->catg_data = nodes[sc->catg_node]->data->size - 1;
		retval++;
	} else if (sc->catg_data >= 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data--;
		retval++;
	} else if (sc->catg_data == 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data = -1;
		sc->catg_node--;
		retval++;
	}

	sc->cur_y--;
	sc->select_idx--;
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (sc->select_idx >= 0 && sc->displayed < sc->total_rows && sc->cur_y < 0) {
		sc->cur_y = -1;
		if (sc->catg_data == -1) {
			nc_scroll_prev(nodes[sc->catg_node]->catg_fp, 
						   bfptr, wptr, cw, true);
//		} else if (sc->catg_data >= 0) {
		} else {
			nc_scroll_prev(nodes[sc->catg_node]->data->data[sc->catg_data], 
						   rfptr, wptr, cw, false);
		}
		retval++;
		sc->cur_y = getcury(wptr);
		mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	return retval;
}

/* Returns 1 if the text was scrolled down, 0 if a normal scroll occured, -1
 * if no scroll occured. */
static int nc_scroll_next_category
(WINDOW *wptr, CategoryNode **nodes, struct ScrollCursor *sc, 
 struct ColWidth *cw, FILE *rfptr, FILE *bfptr)
{
	assert(nodes[sc->catg_node]->data->size <= INT_MAX);

	int retval = -1;
	if (sc->catg_data < 0 && nodes[sc->catg_node]->data->size > 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		sc->catg_data = 0;
		retval++;

	/* Explicit cast of sc->catg_data to size_t is okay, the previous condition checks
	 * that it is a positive number.  vvvvvv */
	} else if (sc->catg_data < (int)nodes[sc->catg_node]->data->size - 1) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data++;
		retval++;
	} else if (sc->catg_data == (int)nodes[sc->catg_node]->data->size - 1) {
		if (nodes[sc->catg_node]->data->size == 0) {
			mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		} else {
			mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		}
		sc->catg_data = -1;
		sc->catg_node++;
		retval++;
	}

	sc->cur_y++;
	sc->select_idx++;
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (sc->displayed < sc->total_rows && sc->cur_y == getmaxy(wptr)) {
		if (sc->catg_data == -1) {
			nc_scroll_next(nodes[sc->catg_node]->catg_fp, 
						   bfptr, wptr, cw, true);
		} else {
			nc_scroll_next(nodes[sc->catg_node]->data->data[sc->catg_data],
						   rfptr, wptr, cw, false);
		}
		retval++;
		sc->cur_y = getcury(wptr);
		mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	return retval;
}

/*
 * After the detail sub window is closed, there is a gap of invisible lines
 * where the window was. This loops through each line and changes it attr
 * back to A_NORMAL and then re-reverse-video's the original line. Cursor
 * position is NOT changed.
 *
 * Refreshes n number of lines.
 */
static void refresh_on_detail_close_uniform
(WINDOW *wptr, WINDOW *wptr_parent, int n)
{
	int temp_y, temp_x;
	getyx(wptr, temp_y, temp_x);
	wmove(wptr, 0, 0);

	for (int i = 0; i < n; i++) {
		mvwchgat(wptr, i, 0, -1, A_NORMAL, 0, NULL);
	}

	wmove(wptr, temp_y, temp_x);
	wchgat(wptr, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	mvwvline(wptr_parent, 1, 0, 0, getmaxy(wptr_parent) - 2);
	mvwvline(wptr_parent, 1, getmaxx(wptr_parent) - 1, 0, getmaxy(wptr_parent) - 2);
	wrefresh(wptr_parent);
}

/* An optimized version of refreshing the screen. This function only refreshes
 * as many lines as is required to get the data back on the screen after a
 * subwindow closes. */
static void refresh_budget_loop
(WINDOW *data, CategoryNode **nodes, struct ScrollCursor *sc,
 struct ColWidth *cw, FILE *rfptr, FILE *bfptr, int subwin_y)
{
	int tmp_idx = sc->select_idx;
	int subw_y_upper = (getmaxy(stdscr) / 2) - (subwin_y / 2) - BOX_OFFSET;
	int subw_y_lower = (getmaxy(stdscr) / 2) + (subwin_y / 2) - BOX_OFFSET;

	if (sc->cur_y <= subw_y_upper) {
		while (sc->cur_y < subw_y_lower && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx > 0) {
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	} else if (sc->cur_y >= subw_y_lower) {
		while (sc->cur_y > subw_y_upper && sc->select_idx > 0) { 
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	} else {
		while (sc->cur_y > subw_y_upper && sc->select_idx > 0) { 
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->cur_y < subw_y_lower && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	}
}

/*
 * Main loop for the user to interact with when selecting the read menu option.
 * If sorted by anything other than 'Category', nc_read_loop will be used.
 *
 * Prints scrollable data to the window pointed to by wptr, sorted by Category.
 * Categories will have their own row in the data with a user-modifiable value
 * retrived from BUDGET_DIR.
 */
void nc_read_budget_loop
(struct ReadWins *wins, FILE *rfptr, FILE *bfptr, struct SelRecord *sr,
 Vec *psc, CategoryNode **nodes)
{
	struct ColWidth cw_, *cw = &cw_;
	struct ScrollCursor sc_, *sc = &sc_;
	struct DispCursor dc_, *dc = &dc_;
	dc->first = 1;

	char *line;
	char linebuff[LINE_BUFFER];
	int c = 0;
	int subwin_y;
	int scroll_ret;

	init_scroll_cursor(sc, nodes);
	calculate_columns(cw, getmaxx(wins->data) + BOX_OFFSET);

	if (debug_flag) {
//		debug_columns(wins->parent, cw);
	}

	nc_print_read_footer(stdscr);
	print_init_budget_loop(wins->data, sc, nodes, cw, rfptr);
	draw_read_window_borders_and_text(wins, psc);
	dc->last = sc->displayed;

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(wins->parent, sc, dc);

		if (debug_flag) {
			print_debug_line(wins->parent, sc);
		}
		c = wgetch(wins->data);

		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (sc->select_idx + 1 < sc->total_rows) {
				scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}

			break;
		case('k'):
		case(KEY_UP):
			if (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;
		case('K'):
		case(KEY_SHOME): // "SHIFT + HOME"
			if (sc->catg_data == -1 && sc->catg_node != 0) {
				mv_category_to_top(nodes, sc->catg_node);
				sr->flag = RESIZE;
				sr->index = 0;
				return;
			}
			break;
		case('?'):
			subwin_y = show_help_subwindow();
			init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			refresh_budget_loop(wins->data, nodes, sc, cw, rfptr, bfptr, subwin_y);
			c = 0;
			break;
		case('\n'):
		case('\r'):
			if (sc->catg_data >= 0) {
				fseek(rfptr, nodes[sc->catg_node]->data->data[sc->catg_data], SEEK_SET);
				line = fgets(linebuff, sizeof(linebuff), rfptr);
				subwin_y = show_detail_subwindow(line);
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
				refresh_budget_loop(wins->data, nodes, sc, cw, rfptr, bfptr, subwin_y);
				c = 0;
			} else {
				c = 0;
			}
			break;

		case(KEY_NPAGE): // PAGE DOWN
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx + 1 < sc->total_rows) {
					scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
					dc->first += scroll_ret;
					dc->last += scroll_ret;
				}
			}
			break;
		case(KEY_PPAGE): // PAGE UP
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx > 0) {
					scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
					dc->first -= scroll_ret;
					dc->last -= scroll_ret;
				}
			}
			break;

		case(KEY_EOL):
		case(KEY_END):
			while (sc->select_idx + 1 < sc->total_rows) {
				scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;
		case(KEY_BEG):
		case(KEY_HOME):
			while (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('['):
			if (sc->sidebar_idx > 0) {
				sc->sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case(']'):
			if (nodes[sc->sidebar_idx]->next != NULL) {
				sc->sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case('A'):
		case('a'):
		case(KEY_F(ADD)):
			sr->flag = ADD;
			return;
		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			if (sc->catg_data < 0) { 
				sr->flag = EDIT_CATG;
				sr->opt = nodes[sc->catg_node]->data->size;
				sr->index = sc->catg_node;
				//sr->index = nodes[sc->catg_node]->catg_fp;
			} else {
				sr->flag = EDIT;
				sr->index = nodes[sc->catg_node]->data->data[sc->catg_data];
			}
			return;
		case('R'):
		case('r'):
		case(KEY_F(READ)):
			sr->flag = READ;
			sr->index = 0;
			return;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			sr->flag = QUIT;
			sr->index = 0;
			return;
		case('S'):
		case('s'):
		case(KEY_F(SORT)):
			sr->flag = SORT;
			sr->index = 0;
			return;
		case('O'):
		case('o'):
		case(KEY_F(OVERVIEW)):
			sr->flag = OVERVIEW;
			sr->index = 0;
			return;
		case(KEY_RESIZE):
			sr->flag = RESIZE;
			sr->index = 0;
			return;
		}
	}

	sr->flag = NO_SELECT;
	sr->index = 0;
	return;
}

static void init_scroll_cursor_no_category(struct ScrollCursor *sc)
{
	sc->total_rows = 0;
	sc->displayed = 0;
	sc->select_idx = 0; // Tracks selection for indexing
	sc->cur_y = 0; // Tracks cursor position in window
	sc->catg_node = 0; // Tracks which node the cursor is on
	/* Tracks which member record the cursor is on, begins at -1 to mark that
	 * the first cursor position is a node. */
	sc->catg_data = -1;
	sc->sidebar_idx = 0;
}

/* Print initial lines based on screen size for nc_read_loop */
static void nc_print_initial_read_loop
(WINDOW *wptr, struct ScrollCursor *sc, struct ColWidth *cw, 
 FILE *fptr, Vec *psc)
{
	char *line_str;
	char linebuff[LINE_BUFFER];
	struct LineData ld;

	/* For safe cast */
	assert(sc->displayed >= 0);

	for (int i = 0; i < getmaxy(wptr) && (size_t)sc->displayed < psc->size; i++) {
		fseek(fptr, psc->data[i], SEEK_SET);
		line_str = fgets(linebuff, sizeof(linebuff), fptr);
		tokenize_record(&ld, &line_str);
		print_record_hr(wptr, cw, &ld, i);
		free_tokenized_record_strings(&ld);
		sc->displayed++;
	}

	wmove(wptr, 0, 0);
	mvwchgat(wptr, sc->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	if (debug_flag) {
		curs_set(1);
	}
	wrefresh(wptr);
}

/*
 * Main read loop. Populates member values in the struct pointed to 
 * by sr on a MenuKeys press. Prints lines by seeking FPI to the byte offset
 * of psc->data. Sort occurs before this function in nc_read_setup.
 */
void nc_read_loop
(struct ReadWins *wins, FILE *fptr, struct SelRecord *sr, Vec *psc,
 CategoryNode **nodes)
{
	struct ColWidth cw_, *cw = &cw_;
	struct ScrollCursor sc_, *sc = &sc_;
	struct DispCursor dc_, *dc = &dc_;
	dc->first = 1;
	char linebuff[LINE_BUFFER];
	int c = 0;
	int scroll_ret;

	init_scroll_cursor_no_category(sc);
	calculate_columns(cw, getmaxx(wins->data) + BOX_OFFSET);
	nc_print_read_footer(stdscr);
	nc_print_initial_read_loop(wins->data, sc, cw, fptr, psc);

	assert(psc->size <= INT_MAX);

	sc->total_rows = psc->size;
	dc->last = sc->displayed;
	draw_read_window_borders_and_text(wins, psc);

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(wins->parent, sc, dc);

		if (debug_flag) {
		}
		c = wgetch(wins->data);

		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (sc->select_idx + 1 < sc->total_rows) {
				scroll_ret = nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;

		case('k'):
		case(KEY_UP):
			if (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('\n'):
		case('\r'):
			fseek(fptr, psc->data[sc->select_idx], SEEK_SET);
			char *line = fgets(linebuff, sizeof(linebuff), fptr);
			show_detail_subwindow(line);
			refresh_on_detail_close_uniform(wins->data, wins->parent, sc->displayed);
			c = 0;
			break;

		case(KEY_NPAGE): // PAGE DOWN
			for(int i = 0; i < 10; i++) {
				if (sc->select_idx + 1 < sc->total_rows) {
					nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
					dc->first += scroll_ret;
					dc->last += scroll_ret;
				}
			}
			break;

		case(KEY_PPAGE): // PAGE UP
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx > 0) {
					nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
					dc->first -= scroll_ret;
					dc->last -= scroll_ret;
				}
			}
			break;

		case(KEY_END):
			while (sc->select_idx + 1 < sc->total_rows) {
				nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;

		case(KEY_HOME):
			while (sc->select_idx > 0) {
				nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('['):
			if (sc->sidebar_idx > 0) {
				sc->sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;

		case(']'):
			if (nodes[sc->sidebar_idx]->next != NULL) {
				sc->sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;

		case('A'):
		case('a'):
		case(KEY_F(ADD)):
			sr->flag = ADD;
			sr->index = psc->data[sc->select_idx];
			return;

		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			sr->flag = EDIT;
			sr->index = psc->data[sc->select_idx];
			return;

		case('R'):
		case('r'):
		case(KEY_F(READ)):
			sr->flag = READ;
			sr->index = 0;
			return;
				
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			sr->flag = QUIT;
			sr->index = 0;
			return;

		case('S'):
		case('s'):
		case(KEY_F(SORT)):
			sr->flag = SORT;
			sr->index = 0;
			return;

		case('O'):
		case('o'):
		case(KEY_F(OVERVIEW)):
			sr->flag = OVERVIEW;
			sr->index = 0;
			return;

		case(KEY_RESIZE):
			sr->flag = RESIZE;
			sr->index = 0;
			return;

		}
	}

	sr->flag = NO_SELECT;
	sr->index = 0;
	return;
}
