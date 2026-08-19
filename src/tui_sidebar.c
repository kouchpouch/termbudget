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

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <limits.h>

#include "main.h"
#include "tui.h"
#include "tui_sidebar.h"
#include "parser.h"
#include "categories.h"
#include "helper.h"

#define GRAPH_LENGTH 30

void draw_body_border(WINDOW *wptr)
{
	wborder(wptr, 0, 0, 0, 0, ACS_LTEE, ACS_RTEE, ACS_BTEE, 0);
	mvwxcprintw(wptr, 0, "Categories");
	wrefresh(wptr);
}

static void write_parent_title(WINDOW *wptr)
{
	mvwxcprintw(wptr, 0, "Summary");
	wrefresh(wptr);
}

static bool check_y_fit(WINDOW *wptr, int y)
{
	if (y >= getmaxy(wptr) - 1) {
		return false;
	} else {
		return true;
	}
}

WINDOW *create_sidebar_parent(WINDOW *wptr_parent, int std_y, int std_x)
{
	int parent_x = getmaxx(wptr_parent);
	WINDOW *wptr = newwin(std_y - 1, std_x - parent_x + 1, 0, parent_x - 1);
	if (wptr == NULL) {
		window_creation_fail();
	}

	return wptr;
}

WINDOW *create_sidebar_body(WINDOW *wptr_parent, WINDOW *wptr_sidebar_parent)
{
	// TODO: This should come from the return value of print_parent_header
	int head_y = 5;
	WINDOW *wptr = newwin(getmaxy(wptr_sidebar_parent) - head_y, 
					   SIDEBAR_COLUMNS + 1, head_y, getmaxx(wptr_parent) - 1); 
	if (wptr == NULL) {
		window_creation_fail();
	}

	return wptr;
}

static bool verify_sidebar_strlen(char *str, WINDOW *wptr)
{
	if ((int)strnlen(str, INT_MAX) > getmaxx(wptr) - BOX_OFFSET) {
		return false;
	} else {
		return true;
	}
}

/* Prints the string 'str' and returns the number of rows printed to */
static int print_body_categories(char *str, WINDOW *wptr, int y, int x, int i)
{
	if (!check_y_fit(wptr, y)) {
		return 0;
	}
	wattron(wptr, COLOR_PAIR(category_color(i)));
	if (!verify_sidebar_strlen(str, wptr)) {
		mvwprintw(wptr, y, x, "%.*s%s", getmaxx(wptr) - (BOX_OFFSET + 2), str, "..");
	} else {
		mvwprintw(wptr, y, x, "%s", str);
	}
	wattroff(wptr, COLOR_PAIR(category_color(i)));
	return 1;
}

static int calculate_graph_length(int transaction_type,
								  double planned,
								  double expenses,
								  double remaining)
{
	int length = 0;

	if (planned == 0) {
		length = GRAPH_LENGTH - 1;
		/* Bug 36 fix
		 * Handles edge case where a category has a planned value of zero and
		 * only contains income transactions */
		if (transaction_type == TT_EXPENSE && expenses > 0) {
			length = 0;
		}
	} else {
		if (transaction_type == TT_INCOME) {
			length = (GRAPH_LENGTH - 1) * (expenses / planned);
		} else {
			length = (GRAPH_LENGTH - 1) * (1 - (remaining / planned));
		}
	}

	if (length > GRAPH_LENGTH - 1) {
		length = GRAPH_LENGTH - 1;
	} else if (length < 0) {
		length = 0;
	}

	return length;
}

static int print_body_graphs_and_values(double planned,
										double exp,
										int tt,
										WINDOW *wptr,
										int y,
										int i)
{
	char graph[GRAPH_LENGTH];
	double remaining = 0.0;
	int graph_len;
	int fill_graph;
	int graph_x_begin;
	int remain_x_begin;
	int planned_x_begin;
	int tracked_x_begin;

	if (!check_y_fit(wptr, y)) {
		return 0;
	}

	for (int i = 0; i < GRAPH_LENGTH; i++) {
		graph[i] = ' ';
	}
	graph[sizeof(graph) - 1] = '\0';

	/* The 'exp' variable holds a positive number when income outweighs expenses,
	 * a negative number when expenses outweigh income. */
	remaining = planned + exp;
	remaining = normalize_near_zero(remaining);

	graph_len = calculate_graph_length(tt, planned, exp, remaining);

	graph_x_begin = (getmaxx(wptr) - GRAPH_LENGTH) / 2;
	remain_x_begin = (getmaxx(wptr) - graph_x_begin - strlen(" Remaining") - finlen(remaining) - BOX_OFFSET - 4);
	planned_x_begin = (getmaxx(wptr) - graph_x_begin - strlen(" Planned") - finlen(planned) - BOX_OFFSET - 4);
	tracked_x_begin = BOX_OFFSET + 6;

	mvwaddch(wptr, y, getmaxx(wptr) - 5 - BOX_OFFSET, ACS_URCORNER);
	mvwaddch(wptr, y, getmaxx(wptr) - 5 - BOX_OFFSET - 1, ACS_HLINE);
	if (tt == TT_INCOME) {
		mvwprintw(wptr, y, planned_x_begin, " $%.2f Planned", planned);
	} else {
		if (remaining < 0) {
			wattron(wptr, COLOR_PAIR(1));
			mvwprintw(wptr, y, remain_x_begin, " $%.2f Remaining", remaining);
			wattroff(wptr, COLOR_PAIR(1));
		} else {
			mvwprintw(wptr, y, remain_x_begin, " $%.2f Remaining", remaining);
		}
	}
	y++;
	if (!check_y_fit(wptr, y)) {
		return 1;
	}

	wattron(wptr, COLOR_PAIR(category_color(i)));
	mvwprintw(wptr, y, graph_x_begin, "[%s]", graph);
	mvwchgat(wptr, y, graph_x_begin + 1, graph_len, A_REVERSE, category_color(i), NULL);
	fill_graph = graph_x_begin + 1 + graph_len;
	for (int j = fill_graph; j < getmaxx(wptr) - graph_x_begin - 1; j++) {
		mvwaddch(wptr, y, j, ACS_BULLET);
	}
	wattroff(wptr, COLOR_PAIR(category_color(i)));
	y++;
	if (!check_y_fit(wptr, y)) {
		return 2;
	}

	mvwaddch(wptr, y, BOX_OFFSET + 4, ACS_LLCORNER);
	mvwaddch(wptr, y, BOX_OFFSET + 5, ACS_HLINE);
	if (tt == TT_INCOME) {
		mvwprintw(wptr, y, tracked_x_begin, " $%.2f Received", exp);
	} else {
		mvwprintw(wptr, y, tracked_x_begin, " $%.2f Tracked", planned - remaining);
	}

	return 4;
}

int init_sidebar_body(WINDOW *wptr, struct catg_node *head, size_t i)
{
	struct catg_node *tmp = get_node_by_idx(head, i);
	struct budget_tokens *bt = NULL;
	int n_displayed = 0;
	int y = 1;
	int x = 1;
	double exp;

	wclear(wptr);
	draw_body_border(wptr);

	while (y < getmaxy(wptr) - 4) {
		bt = tokenize_budget_fpi(tmp->catg_fp);
		exp = get_expenditures_per_category_fast(tmp);
		if (tmp->next == NULL) {
			y += print_body_categories(bt->catg, wptr, y, x, i);
			if (!check_y_fit(wptr, y)) {
				free_budget_tokens(bt);
				break;
			}
			y += print_body_graphs_and_values(bt->amount, exp, bt->transtype, wptr, y, i);
			free_budget_tokens(bt);
			bt = NULL;
			break;
		} else {
			y += print_body_categories(bt->catg, wptr, y, x, i);
			if (!check_y_fit(wptr, y)) {
				free_budget_tokens(bt);
				break;
			}
			y += print_body_graphs_and_values(bt->amount, exp, bt->transtype, wptr, y, i);
			free_budget_tokens(bt);
			bt = NULL;
		}
		i++;
		n_displayed++;
		tmp = tmp->next;
	}

	wrefresh(wptr);
	return n_displayed;
}

static int print_parent_header(WINDOW *wptr, struct vec_d *psc, double leftover)
{
	int x = 1;
	int y = 1;
	int max_x = getmaxx(wptr);
	struct vec2f_fin pb;
	calculate_balance(&pb, psc);
	double remaining = pb.income - pb.expense;
	remaining = normalize_near_zero(remaining);

	mvwprintw(wptr, y, x, "Income:");
	mvwprintw(wptr, y, max_x - (finlen(pb.income) + BOX_OFFSET), "$%.2f", pb.income);
	y++;

	mvwprintw(wptr, y, x, "Expenses:");
	mvwprintw(wptr, y, max_x - (finlen(pb.expense) + BOX_OFFSET), "$%.2f", pb.expense);
	y++;

	mvwprintw(wptr, y, x, "Remaining:");
	if (remaining < 0.0) {
		wattron(wptr, COLOR_PAIR(1));
	}
	mvwprintw(wptr, y, max_x - (finlen(remaining) + BOX_OFFSET), "$%.2f", remaining);
	wattroff(wptr, COLOR_PAIR(1));
	y++;

	mvwprintw(wptr, y, x, "Left to Budget:");
	mvwprintw(wptr, y, max_x - (finlen(leftover) + BOX_OFFSET), "$%.2f", leftover);
	y++;

	wrefresh(wptr);
	return y;
}

void draw_sidebar_parent_border(WINDOW *wptr)
{
	wborder(wptr, 0, 0, 0, 0, ACS_TTEE, 0, 0, 0);
	write_parent_title(wptr);
}

void init_sidebar_parent(WINDOW *wptr, struct vec_d *psc, double leftover)
{
	draw_sidebar_parent_border(wptr);
	print_parent_header(wptr, psc, leftover);
}
