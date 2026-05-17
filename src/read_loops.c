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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ncurses.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

#include "read_loops.h"
#include "read_init.h"
#include "main.h"
#include "edit_transaction.h"
#include "edit_categories.h"
#include "helper.h"
#include "tui.h"
#include "tui_sidebar.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "flags.h"

#include "benchmark.h"

#define NUM_BUFFER_SZ 3 /* for vim-like number buffer */

struct visible_range {
	int first;
	int last;
};

/* Holds the variables used for scrolling and/or selecting transactions and/or 
 * categories */
struct scroll_vars {
	struct column_width *cw;
	struct visible_range *vr;
	WINDOW *wptr_data;
	WINDOW *wptr_parent;
	size_t sidebar_idx;
	/* Total size of the data that can be shown. */
	int total_rows; 
	/* Total number of records and/or categories displayed on the screen. */
	int displayed; 
	int select_idx;
	int cur_y;
	int catg_node;
	int catg_data;
};

struct num_buffer {
	int result;
	int buffer[NUM_BUFFER_SZ];
};

static void print_debug_line(struct scroll_vars *sv)
{
	int max_y, max_x, print_y;
	getmaxyx(sv->wptr_parent, max_y, max_x);
	print_y = max_y - 1;
	mvwhline(sv->wptr_parent, print_y, 1, 0, max_x - 2);
	mvwprintw(sv->wptr_parent, print_y, max_x - 55, "SELIDX: %d", sv->select_idx);
	mvwprintw(sv->wptr_parent, print_y, max_x - 40, "DATA: %d", sv->displayed);
	mvwprintw(sv->wptr_parent, print_y, max_x - 30, "DATA: %d", sv->catg_data);
	mvwprintw(sv->wptr_parent, print_y, max_x - 20, "NODE: %d", sv->catg_node);
	mvwprintw(sv->wptr_parent, print_y, max_x - 10, "CURS: %d", sv->cur_y);
	wrefresh(sv->wptr_parent);
}

static void print_catg_balances(WINDOW *wptr,
								int tt,
								double planned,
								double exp,
								int width)
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
static void print_record_hr(WINDOW *wptr,
							struct column_width *cw,
							struct transaction_tokens *ld,
							int y)
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

static void print_category_hr(WINDOW *wptr,
							  struct column_width *cw,
							  struct budget_tokens *bt,
							  struct catg_node *node,
							  int y)
{
	char *etc = "..";
	int lenetc = (int)strlen(etc);
	int x = 0;
	int print_offset = 0;
	double e = get_expenditures_per_category_fast(node);
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

static void print_init_budget_loop(struct scroll_vars *sv,
								   struct catg_node *head,
								   FILE *fptr)
{
	struct transaction_tokens ld;
	struct catg_node *curr = head;
	char *line_str;
	int max_y = getmaxy(sv->wptr_data);
	int total_nodes = get_total_nodes(head);
	char linebuff[LINE_BUFFER] = { 0 };

	/* 
	 * For each category print the budget line and the records that match 
	 * the category. Increment displayed to keep track of how many lines are
	 * displayed (this is to keep track of scrolling).
	 */
	for (int i = 0; sv->displayed < max_y && sv->displayed < sv->total_rows 
		 && i < total_nodes; i++) 
	{
		struct budget_tokens *bt = tokenize_budget_fpi(curr->catg_fp);
		print_category_hr(sv->wptr_data, sv->cw, bt, curr, sv->displayed);
		mvwchgat(sv->wptr_data, sv->displayed, 0, -1, A_NORMAL, category_color(i), NULL); 
		sv->displayed++;

		for (size_t j = 0; 
			 sv->displayed < max_y && 
		     sv->displayed < sv->total_rows && 
		     j < curr->data->size; 
		     j++)
		{
			fseek(fptr, curr->data->data[j], SEEK_SET);
			line_str = fgets(linebuff, sizeof(linebuff), fptr);
			tokenize_record(&ld, &line_str);
			print_record_hr(sv->wptr_data, sv->cw, &ld, sv->displayed);
			free_tokenized_record_strings(&ld);
			sv->displayed++;
		}

		curr = curr->next;
		free_budget_tokens(bt);
	}

	wmove(sv->wptr_data, 0, 0);
	mvwchgat(sv->wptr_data, sv->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (debug_flag) {
		curs_set(1);
	}

	wrefresh(sv->wptr_data);
}

/* Returns the value of the total number of rows to display in 
 * nc_read_budget_loop() to handle scrolling.
 *
 * The return value is calculated by adding up add struct catg_node and their
 * data member's size. */
static int get_total_displayed_rows(struct catg_node *head)
{
	struct catg_node *curr = head;
	int rows = 0;

	while (1) {
		if (curr->next == NULL) {
			rows += 1 + curr->data->size;
			break;
		} else {
			rows += 1 + curr->data->size;
		}
		curr = curr->next;
	}

	return rows;
}

static void print_balances_text(WINDOW *wptr, struct vec_d *records)
{
	struct vec2f_fin pb_, *pb = &pb_;
	int total_len;
	calculate_balance(pb, records);
	total_len = intlen(pb->income) + intlen(pb->expense) + 
					strlen("Expenses: $.00 Income: $.00");

	mvwprintw(wptr, 0, getmaxx(wptr) / 2 - total_len / 2, 
		   	  "Income: $%.2f Expenses: $%.2f", pb->income, pb->expense);
	wrefresh(wptr);
}

/* Draws all of the window borders, then the border text on top. Call this
 * function any time the borders/text need to be updated. */
static void draw_read_window_borders_and_text(struct ReadWins *wins,
											  struct vec_d *records)
{
	/* Draw borders in order for correct intersecting lines */
	if (wins->sidebar_parent != NULL || wins->sidebar_body != NULL) {
		draw_sidebar_parent_border(wins->sidebar_parent);
		draw_body_border(wins->sidebar_body);
	} else {
		print_balances_text(wins->parent, records);
	}
}

/* Creates a sub window inside of wptr to display a line-by-line format of
 * the selected record at index i of pidx->data. Following the format style
 * from edit_transaction() */
static int show_detail_subwindow(char *line)
{
	WINDOW *wptr_detail = create_input_subwindow();
	int y = getmaxy(wptr_detail);

	box(wptr_detail, 0, 0);
	mvwxcprintw(wptr_detail, 0, "Details");

	struct transaction_tokens linedata_, *ld = &linedata_;
	tokenize_record(ld, &line);

	nc_print_record_vert(wptr_detail, ld, BOX_OFFSET);

	free_tokenized_record_strings(ld);
	nc_exit_window_key(wptr_detail);
	return y;
}

static void refresh_displayed_counter(struct scroll_vars *sv)
{
	int max_x, max_y;
	getmaxyx(sv->wptr_parent, max_y, max_x);
	mvwhline(sv->wptr_parent, max_y - 1, BOX_OFFSET, 0, max_x - (BOX_OFFSET + 1));
	wrefresh(sv->wptr_parent);

	// Calc length of, for example, "32-50 of 80 records"
	char *disp = " -  of  records";
	int x_offset = 
		strlen(disp) + intlen(sv->displayed) + intlen(sv->total_rows) + BOX_OFFSET;

	if (sv->displayed < sv->total_rows) {
		mvwprintw(sv->wptr_parent, max_y - 1, max_x - x_offset,
			"%d-%d of %d records", sv->vr->first, sv->vr->last, sv->total_rows);
		wrefresh(sv->wptr_parent);
	}
}

static void scroll_prev(long b,
						FILE *fptr,
						WINDOW *wptr,
						struct column_width *cw,
						struct catg_node *node,
						bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER] = { 0 };
	char *line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	
	}

	if (catg) {
		struct budget_tokens *bt = tokenize_budget_fpi(b);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		print_category_hr(wptr, cw, bt, node, 0);
		free_budget_tokens(bt);
	} else {
		struct transaction_tokens linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		print_record_hr(wptr, cw, ld, 0);
		free_tokenized_record_strings(ld);
	}
}

static void scroll_next(long b,
						FILE *fptr,
						WINDOW *wptr,
						struct column_width *cw,
						struct catg_node *node,
						bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER] = { 0 };
	char *line_str;
	line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}

	if (catg) {
		struct budget_tokens *bt = tokenize_budget_fpi(b);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		print_category_hr(wptr, cw, bt, node, getmaxy(wptr) - 1);
		free_budget_tokens(bt);
	} else {
		struct transaction_tokens linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		print_record_hr(wptr, cw, ld, getmaxy(wptr) - 1);
		free_tokenized_record_strings(ld);
	}
}

/* Returns 1 if the text was scrolled down, 0 if a normal scroll occured */
static int scroll_prev_records(struct scroll_vars *sv,
							   FILE *fptr,
							   struct vec_d *recs)
{
	int retval = 0;

	unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
	sv->cur_y--;
	sv->select_idx--;

	if (sv->cur_y < 0) {
		sv->cur_y = -1;
	}
	
	/* For safe cast */
	assert(sv->displayed >= 0);

	if (sv->select_idx >= 0 && (size_t)sv->displayed < recs->size && sv->cur_y == -1) {
		scroll_prev(recs->data[sv->select_idx], fptr, sv->wptr_data, sv->cw, NULL, false);
		sv->cur_y = getcury(sv->wptr_data);
		retval++;
	}
	mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured */
static int scroll_next_records(struct scroll_vars *sv,
							  FILE *fptr,
							  struct vec_d *recs)
{
	int retval = 0;

	unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
	sv->cur_y++;
	sv->select_idx++;

	/* For safe cast */
	assert(sv->displayed >= 0);

	if ((size_t)sv->displayed < recs->size && sv->cur_y == getmaxy(sv->wptr_data)) {
		scroll_next(recs->data[sv->select_idx], fptr, sv->wptr_data, sv->cw, NULL, false);
		sv->cur_y = getcury(sv->wptr_data);
		retval++;
	}
	mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

static void scroll_n_next_records(int n,
								  struct scroll_vars *sv,
								  FILE *fptr,
								  struct vec_d *records)
{
	int scroll_ret;
	for (int i = 0; i < n; i++) {
		if (sv->select_idx + 1 < sv->total_rows) {
			scroll_ret = scroll_next_records(sv, fptr, records);
			sv->vr->first -= scroll_ret;
			sv->vr->last -= scroll_ret;
		} else {
			break;
		}
	}
}

static void scroll_n_prev_records(int n,
								  struct scroll_vars *sv,
								  FILE *fptr,
								  struct vec_d *records)
{
	int scroll_ret;
	for (int i = 0; i < n; i++) {
		if (sv->select_idx > 0) {
			scroll_ret = scroll_prev_records(sv, fptr, records);
			sv->vr->first += scroll_ret;
			sv->vr->last += scroll_ret;
		} else {
			break;
		}
	}
}

static void unhighlight_and_color_catg(WINDOW *wptr, int y, int node_idx)
{
	mvwchgat(wptr, y, 0, -1, A_NORMAL, category_color(node_idx), NULL); 
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured, -1
 * if no scroll occured. */
static int scroll_prev_category(struct catg_node *head,
								struct scroll_vars *sv, 
								FILE *rfptr,
								FILE *bfptr)
{
	if (debug_flag) {
		BENCHMARK_START();
	}

	int retval = -1;
	struct catg_node *tmp = NULL;

	/* If the cursor is on a category */
	if (sv->catg_data < 0) {
		/* Unhighlight, recolor, and ready sv members to access the last
		 * record of the previous category */
		unhighlight_and_color_catg(sv->wptr_data, sv->cur_y, sv->catg_node);
		sv->catg_node--;
		tmp = get_node_by_idx(head, sv->catg_node);
		sv->catg_data = tmp->data->size - 1;
		retval++;

	/* If the cursor is on a record and there's a record previous to it */
	} else if (sv->catg_data >= 0) {
		unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
		sv->catg_data--;
		retval++;
	
	/* The cursor is on the first record of a category */
	} else if (sv->catg_data == 0) {
		unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
		sv->catg_data = -1;
		sv->catg_node--;
		retval++;
	}

	sv->cur_y--;
	sv->select_idx--;
	mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (sv->select_idx >= 0 && sv->displayed < sv->total_rows && sv->cur_y < 0) {
		sv->cur_y = -1;
		tmp = get_node_by_idx(head, sv->catg_node);
		if (sv->catg_data == -1) {
			scroll_prev(tmp->catg_fp, 
			   				bfptr, sv->wptr_data, sv->cw, tmp, true);
		} else {
			scroll_prev(tmp->data->data[sv->catg_data], 
			   				rfptr, sv->wptr_data, sv->cw, NULL, false);
		}
		retval++;
		sv->cur_y = getcury(sv->wptr_data);
		mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	if (debug_flag) {
		BENCHMARK_END();
		mvprintw(getmaxy(stdscr) - 1, getmaxx(stdscr) - 45, 
		   		 "s to scroll: %Lf\n", BENCHMARK_RESULT);
		refresh();
	}

	return retval;
}

/* Scrolls the window at sv->wptr_data when inside of the nc_read_budget_loop()
 * The function detects if the line the cursor is on is a category or a record
 * and scrolls appropriately, updating the members of "sv".
 * Returns 1 if the text was scrolled down, 0 if a normal scroll occured, -1
 * if no scroll occured. */
static int scroll_next_category(struct catg_node *head,
								struct scroll_vars *sv,
								FILE *rfptr,
								FILE *bfptr)
{
	if (debug_flag) {
		BENCHMARK_START();
	}

	int retval = -1;
	struct catg_node *tmp = get_node_by_idx(head, sv->catg_node);
	assert(tmp->data->size <= INT_MAX);

	/* If the cursor is on a category and it has records associated with it */
	if (sv->catg_data < 0 && tmp->data->size > 0) {
		/* Unhighlight, recolor, and ready sv members to access the 0th records
		 * of that category */
		unhighlight_and_color_catg(sv->wptr_data, sv->cur_y, sv->catg_node);
		sv->catg_data = 0;
		retval++;

	/* If the cursor is on a record and the next line is also a record */
	} else if (sv->catg_data < (int)tmp->data->size - 1) {
		/* Unhighlight, ready sv members to access the next record */
		unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
		sv->catg_data++;
		retval++;

	/* If the cursor is on a record and the next line is a category OR 
	 * if the cursor is on a category which contains no records. */
	} else if (sv->catg_data == (int)tmp->data->size - 1) {
		/* If cursor is on a category which contains no records */
		if (tmp->data->size == 0) {
			unhighlight_and_color_catg(sv->wptr_data, sv->cur_y, sv->catg_node);
		/* If the cursor is on a record and the next line is a category */
		} else {
			unhighlight(sv->wptr_data, sv->cur_y, 0, -1);
		}
		sv->catg_data = -1;
		sv->catg_node++;
		retval++;
	}

	sv->cur_y++;
	sv->select_idx++;
	mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	/* If theres more data to display and the cursor is at the bottom of 
	 * the window... */

	if (sv->displayed < sv->total_rows && sv->cur_y == getmaxy(sv->wptr_data)) {
		tmp = get_node_by_idx(head, sv->catg_node);

		/* If what the cursor is scrolling to is a category */
		if (sv->catg_data == -1) {
			scroll_next(tmp->catg_fp, bfptr, sv->wptr_data, sv->cw, tmp, true);
		} else {
			scroll_next(tmp->data->data[sv->catg_data],
			   			rfptr, sv->wptr_data, sv->cw, NULL, false);
		}
		retval++;
		sv->cur_y = getcury(sv->wptr_data);
		mvwchgat(sv->wptr_data, sv->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	if (debug_flag) {
		BENCHMARK_END();
		mvprintw(getmaxy(stdscr) - 1, getmaxx(stdscr) - 45, 
		   		 "s to scroll: %Lf\n", BENCHMARK_RESULT);
		refresh();
	}

	return retval;
}

static void scroll_n_next_categories(int n,
								     struct catg_node *head,
								     struct scroll_vars *sv,
								     FILE *rfptr,
								     FILE *bfptr)
{
	int scroll_ret;
	for (int i = 0; i < n; i++) {
		if (sv->select_idx + 1 < sv->total_rows) {
			scroll_ret = scroll_next_category(head, sv, rfptr, bfptr);
			sv->vr->first += scroll_ret;
			sv->vr->last += scroll_ret;
		} else {
			break;
		}
	}
}

static void scroll_n_prev_categories(int n,
								     struct catg_node *head,
								     struct scroll_vars *sv,
								     FILE *rfptr,
								     FILE *bfptr)
{
	int scroll_ret;
	for (int i = 0; i < n; i++) {
		if (sv->select_idx > 0) {
			scroll_ret = scroll_prev_category(head, sv, rfptr, bfptr);
			sv->vr->first -= scroll_ret;
			sv->vr->last -= scroll_ret;
		} else {
			break;
		}
	}
}

/*
 * After the detail sub window is closed, there is a gap of invisible lines
 * where the window was. This loops through each line and changes it attr
 * back to A_NORMAL and then re-reverse-video's the original line. Cursor
 * position is NOT changed.
 *
 * Refreshes n number of lines.
 */
static void refresh_on_detail_close_uniform(WINDOW *wptr,
											WINDOW *wptr_parent,
											int n)
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
static void refresh_budget_loop(struct catg_node *nodes,
								struct scroll_vars *sv,
								FILE *rfptr,
								FILE *bfptr,
								int subwin_y)
{
	int tmp_idx = sv->select_idx;
	int subw_y_upper = (getmaxy(stdscr) / 2) - (subwin_y / 2) - BOX_OFFSET;
	int subw_y_lower = (getmaxy(stdscr) / 2) + (subwin_y / 2) - BOX_OFFSET;

	if (sv->cur_y <= subw_y_upper) {
		while (sv->cur_y < subw_y_lower && sv->select_idx + 1 < sv->total_rows) {
			scroll_next_category(nodes, sv, rfptr, bfptr);
		}
		while (sv->select_idx != tmp_idx && sv->select_idx > 0) {
			scroll_prev_category(nodes, sv, rfptr, bfptr);
		}
	} else if (sv->cur_y >= subw_y_lower) {
		while (sv->cur_y > subw_y_upper && sv->select_idx > 0) { 
			scroll_prev_category(nodes, sv, rfptr, bfptr);
		}
		while (sv->select_idx != tmp_idx && sv->select_idx + 1 < sv->total_rows) {
			scroll_next_category(nodes, sv, rfptr, bfptr);
		}
	} else {
		while (sv->cur_y > subw_y_upper && sv->select_idx > 0) { 
			scroll_prev_category(nodes, sv, rfptr, bfptr);
		}
		while (sv->cur_y < subw_y_lower && sv->select_idx + 1 < sv->total_rows) {
			scroll_next_category(nodes, sv, rfptr, bfptr);
		}
		while (sv->select_idx != tmp_idx && sv->select_idx + 1 < sv->total_rows) {
			scroll_prev_category(nodes, sv, rfptr, bfptr);
		}
	}
}

static void clear_number_buffer(struct num_buffer *b)
{
	for (int i = 0; i < NUM_BUFFER_SZ; i++) {
		b->buffer[i] = -1;
	}
	b->result = 0;
}

static void number_buffer_to_int(struct num_buffer *b)
{
	int multiplier = 1;
	for (int i = 0; i < NUM_BUFFER_SZ; i++) {
		if (i > 0) {
			if (b->buffer[i] >= '0' && b->buffer[i] <= '9') {
				multiplier *= 10;
			}
		}
	}

	for (int i = 0; i < NUM_BUFFER_SZ; i++) {
		if (b->buffer[i] != '0' && b->buffer[i] >= 48) {
			b->result += (b->buffer[i] - 48) * multiplier;
		}
		multiplier /= 10;
	}
}

static int fill_number_buffer(int init, struct num_buffer *buff)
{
	int tmp = 0;
	int i = 0;
	buff->buffer[i++] = init;
	while (i < NUM_BUFFER_SZ) {
		halfdelay(5);
		tmp = getch();
		if (tmp == ERR) {
			break;
		} else if (tmp >= '0' && tmp <= '9') {
			buff->buffer[i++] = tmp;
		} else {
			cbreak();
			return tmp;
		}
	}

	cbreak();
	return 0;
}

/* Prints the value of each struct column_width memeber to the window wptr */
void debug_columns(WINDOW *wptr, struct column_width *cw)
{
	int print_y = 10;
	mvwprintw(wptr, print_y++, 10, "COLUMN WIDTHS:\n");
	mvwprintw(wptr, print_y, 10, "DATE: %d CATG: %d, DESC: %d, TRNS: %d, AMNT: %d\n",
	cw->date, cw->catg,	cw->desc, cw->trns, cw->amnt);
	wrefresh(wptr);
	wgetch(wptr);
}

/* Main loop for the user to interact with when selecting the read menu option.
 * If sorted by anything other than 'Category', nc_read_loop will be used.
 *
 * Prints scrollable data to the window pointed to by wptr, sorted by Category.
 * Categories will have their own row in the data with a user-modifiable value
 * retrived from BUDGET_DIR. */
void nc_read_budget_loop(struct ReadWins *wins,
						 FILE *rfptr,
						 FILE *bfptr,
						 struct record_select *rs,
						 struct vec_d *psc,
						 struct catg_node *head)
{
	struct column_width cw_;
	struct visible_range vr_;
	/* Every other member implicitly set to 0 */
	struct scroll_vars scrl = {
		.total_rows = get_total_displayed_rows(head),
		.catg_data = -1,
		.cw = &cw_,
		.vr = &vr_,
		.wptr_data = wins->data,
		.wptr_parent = wins->parent,
	}; 
	struct num_buffer numbuf = { 0 };
	struct catg_node *tmp = NULL;
	char *line;
	char linebuff[LINE_BUFFER] = { 0 };
	int c = 0;
	int subwin_y;

	scrl.vr->first = 1;

	calculate_columns(scrl.cw, getmaxx(wins->data) + BOX_OFFSET);

	if (debug_flag) {
		debug_columns(wins->parent, scrl.cw);
	}

	nc_print_read_footer(stdscr);
	print_init_budget_loop(&scrl, head, rfptr);
	draw_read_window_borders_and_text(wins, psc);
	scrl.vr->last = scrl.displayed;

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(&scrl);
		clear_number_buffer(&numbuf);

		if (debug_flag) {
			print_debug_line(&scrl);
		}
		c = wgetch(wins->data);

		switch (c) {

		case ('j'):
		case KEY_DOWN:
			scroll_n_next_categories(1, head, &scrl, rfptr, bfptr);
			break;

		case ('k'):
		case KEY_UP:
			scroll_n_prev_categories(1, head, &scrl, rfptr, bfptr);
			break;

		case ('K'):
		case KEY_SHOME: // "SHIFT + HOME"
			if (scrl.catg_data == -1 && scrl.catg_node != 0) {
				mv_category_to_top(&head, scrl.catg_node);
				rs->flag = RESIZE;
				rs->index = 0;
				return;
			}
			break;

		case ('?'):
			subwin_y = show_help_subwindow();
			init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
			refresh_budget_loop(head, &scrl, rfptr, bfptr, subwin_y);
			break;

		case KEY_ENTER:
		case ('\n'):
		case ('\r'):
			if (scrl.catg_data >= 0) {
				tmp = get_node_by_idx(head, scrl.catg_node);
				fseek(rfptr, tmp->data->data[scrl.catg_data], SEEK_SET);
				line = fgets(linebuff, sizeof(linebuff), rfptr);
				subwin_y = show_detail_subwindow(line);
				init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
				refresh_budget_loop(head, &scrl, rfptr, bfptr, subwin_y);
			}
			c = 0;
			break;

		case KEY_NPAGE: /* PAGE DN */
			scroll_n_next_categories(PAGE_KEY_ROWS, head, &scrl, rfptr, bfptr);
			break;

		case KEY_PPAGE: /* PAGE UP */
			scroll_n_prev_categories(PAGE_KEY_ROWS, head, &scrl, rfptr, bfptr);
			break;

		case KEY_EOL:
		case KEY_END:
			scroll_n_next_categories(scrl.total_rows, head, &scrl, rfptr, bfptr);
			break;

		case KEY_BEG:
		case KEY_HOME:
			scroll_n_prev_categories(scrl.total_rows, head, &scrl, rfptr, bfptr);
			break;
		
		case ('1'):
		case ('2'):
		case ('3'):
		case ('4'):
		case ('5'):
		case ('6'):
		case ('7'):
		case ('8'):
		case ('9'): /* Vim-like number buffer */
			c = fill_number_buffer(c, &numbuf);
			number_buffer_to_int(&numbuf);

			if (c == 0) {
				c = wgetch(scrl.wptr_data);
			}

			if (c != 'k' && c != 'j' && c != KEY_DOWN && c != KEY_UP) {
				break;
			}

			if (c == 'j' || c == KEY_DOWN) {
				scroll_n_next_categories(numbuf.result, head, &scrl, rfptr, bfptr);
			} else if (c == 'k' || c == KEY_UP) {
				scroll_n_prev_categories(numbuf.result, head, &scrl, rfptr, bfptr);
			} 
			break;

		case ('['):
			if (scrl.sidebar_idx > 0) {
				scrl.sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
			}
			break;

		case (']'):
			tmp = get_node_by_idx(head, scrl.sidebar_idx);
			if (tmp->next != NULL) {
				scrl.sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
			}
			break;

		case ('A'):
		case ('a'):
		case KEY_F(ADD):
			rs->flag = ADD;
			return;

		case ('E'):
		case ('e'):
		case KEY_F(EDIT):
			tmp = get_node_by_idx(head, scrl.catg_node);
			if (scrl.catg_data < 0) { 
				rs->flag = EDIT_CATG;
				rs->opt = tmp->data->size;
				rs->index = scrl.catg_node;
			} else {
				rs->flag = EDIT;
				rs->index = tmp->data->data[scrl.catg_data];
			}
			return;

		case ('d'):
			c = 0;
			halfdelay(5);
			c = wgetch(wins->data);
			cbreak();
			if (c == 'd') {
				if (scrl.catg_data >= 0) {
					tmp = get_node_by_idx(head, scrl.catg_node);
					rs->flag = EDIT;
					rs->index = tmp->data->data[scrl.catg_data];
					rs->opt = EDIT_RCRD_DELETE;
					return;
				}
			}
			c = 0;
			break;

		case ('R'):
		case ('r'):
		case KEY_F(READ):
			rs->flag = READ;
			rs->index = 0;
			return;

		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			rs->flag = QUIT;
			rs->index = 0;
			return;

		case ('S'):
		case ('s'):
		case KEY_F(SORT):
			rs->flag = SORT;
			rs->index = 0;
			return;

		case ('O'):
		case ('o'):
		case KEY_F(OVERVIEW):
			rs->flag = OVERVIEW;
			rs->index = 0;
			return;

		/* Alternate HOME and END sequences, especially for TMUX */
		case (27):
			halfdelay(1);
			if (wgetch(wins->data) == 91) {
				c = wgetch(wins->data);
				if (c == 49) {
					scroll_n_prev_categories(scrl.total_rows, head, &scrl, rfptr, bfptr);
				} else if (c == 52) {
					scroll_n_next_categories(scrl.total_rows, head, &scrl, rfptr, bfptr);
				}
			}
			cbreak();
			c = 0;
			break;

		case KEY_RESIZE:
			rs->flag = RESIZE;
			rs->index = 0;
			return;
		}
	}

	rs->flag = NO_SELECT;
	rs->index = 0;
	return;
}

/* Print initial lines based on screen size for nc_read_loop */
static void print_initial_read_loop(struct scroll_vars *sv,
									   FILE *fptr,
									   struct vec_d *psc)
{
	char *line_str;
	char linebuff[LINE_BUFFER] = { 0 };
	struct transaction_tokens ld;

	/* For safe cast */
	assert(sv->displayed >= 0);

	for (int i = 0; i < getmaxy(sv->wptr_data) && (size_t)sv->displayed < psc->size; i++) {
		fseek(fptr, psc->data[i], SEEK_SET);
		line_str = fgets(linebuff, sizeof(linebuff), fptr);
		tokenize_record(&ld, &line_str);
		print_record_hr(sv->wptr_data, sv->cw, &ld, i);
		free_tokenized_record_strings(&ld);
		sv->displayed++;
	}

	wmove(sv->wptr_data, 0, 0);
	mvwchgat(sv->wptr_data, sv->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	if (debug_flag) {
		curs_set(1);
	}
	wrefresh(sv->wptr_data);
}

/*
 * Main read loop. Populates member values in the struct pointed to 
 * by sr on a MenuKeys press. Prints lines by seeking FPI to the byte offset
 * of psc->data. Sort occurs before this function in nc_read_setup.
 */
void nc_read_loop(struct ReadWins *wins, 
				  FILE *fptr, 
				  struct record_select *sr, 
				  struct vec_d *records,
				  struct catg_node *head)
{
	struct num_buffer numbuf;
	struct column_width cw_;
	struct visible_range vr_;
	struct scroll_vars scrl = {
		.catg_data = -1,
		.cw = &cw_,
		.vr = &vr_,
		.wptr_data = wins->data,
		.wptr_parent = wins->parent
	};
	struct catg_node *tmp = NULL;
	scrl.vr->first = 1;
	char linebuff[LINE_BUFFER] = { 0 };
	int c = 0;

	calculate_columns(scrl.cw, getmaxx(scrl.wptr_data) + BOX_OFFSET);
	nc_print_read_footer(stdscr);
	print_initial_read_loop(&scrl, fptr, records);

	assert(records->size <= INT_MAX);

	scrl.total_rows = records->size;
	scrl.vr->last = scrl.displayed;
	draw_read_window_borders_and_text(wins, records);

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(&scrl);
		clear_number_buffer(&numbuf);

		if (debug_flag) {
		}
		c = wgetch(wins->data);

		switch (c) {

		case ('j'):
		case KEY_DOWN:
			scroll_n_next_records(1, &scrl, fptr, records);
			break;

		case ('k'):
		case KEY_UP:
			scroll_n_prev_records(1, &scrl, fptr, records);
			break;

		case ('\n'):
		case ('\r'):
			fseek(fptr, records->data[scrl.select_idx], SEEK_SET);
			char *line = fgets(linebuff, sizeof(linebuff), fptr);
			show_detail_subwindow(line);
			refresh_on_detail_close_uniform(wins->data, wins->parent, scrl.displayed);
			c = 0;
			break;

		case KEY_NPAGE: /* PAGE DN */
			scroll_n_next_records(PAGE_KEY_ROWS, &scrl, fptr, records);
			break;

		case KEY_PPAGE: /* PAGE UP */
			scroll_n_prev_records(PAGE_KEY_ROWS, &scrl, fptr, records);
			break;

		case KEY_END:
			scroll_n_next_records(scrl.total_rows, &scrl, fptr, records);
			break;

		case KEY_HOME:
			scroll_n_prev_records(scrl.total_rows, &scrl, fptr, records);
			break;

		case ('1'):
		case ('2'):
		case ('3'):
		case ('4'):
		case ('5'):
		case ('6'):
		case ('7'):
		case ('8'):
		case ('9'): /* Vim-like number buffer */
			c = fill_number_buffer(c, &numbuf);
			number_buffer_to_int(&numbuf);

			if (c == 0) {
				c = wgetch(scrl.wptr_data);
			}

			if (c != 'k' && c != 'j' && c != KEY_DOWN && c != KEY_UP) {
				break;
			}

			if (c == 'j' || c == KEY_DOWN) {
				scroll_n_next_records(numbuf.result, &scrl, fptr, records);
			} else if (c == 'k' || c == KEY_UP) {
				scroll_n_prev_records(numbuf.result, &scrl, fptr, records);
			} 
			break;


		case ('['):
			if (scrl.sidebar_idx > 0) {
				scrl.sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
			}
			break;

		case (']'):
			tmp = get_node_by_idx(head, scrl.sidebar_idx);
			if (tmp->next != NULL) {
				scrl.sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, head, scrl.sidebar_idx);
			}
			break;

		case ('A'):
		case ('a'):
		case KEY_F(ADD):
			sr->flag = ADD;
			sr->index = records->data[scrl.select_idx];
			return;

		case ('E'):
		case ('e'):
		case KEY_F(EDIT):
			sr->flag = EDIT;
			sr->index = records->data[scrl.select_idx];
			return;

		case ('R'):
		case ('r'):
		case KEY_F(READ):
			sr->flag = READ;
			sr->index = 0;
			return;
				
		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			sr->flag = QUIT;
			sr->index = 0;
			return;

		case ('S'):
		case ('s'):
		case KEY_F(SORT):
			sr->flag = SORT;
			sr->index = 0;
			return;

		case ('O'):
		case ('o'):
		case KEY_F(OVERVIEW):
			sr->flag = OVERVIEW;
			sr->index = 0;
			return;

		/* Alternate HOME and END sequences, especially for TMUX */
		case (27):
			halfdelay(1);
			if (wgetch(wins->data) == 91) {
				c = wgetch(wins->data);
				if (c == 49) {
					scroll_n_prev_records(scrl.total_rows, &scrl, fptr, records);
				} else if (c == 52) {
					scroll_n_next_records(scrl.total_rows, &scrl, fptr, records);
				}
			}
			cbreak();
			c = 0;
			break;

		case KEY_RESIZE:
			sr->flag = RESIZE;
			sr->index = 0;
			return;

		}
	}

	sr->flag = NO_SELECT;
	sr->index = 0;
	return;
}
