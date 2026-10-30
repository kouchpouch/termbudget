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
 * Copyright (c) 2026 termbudget
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#include <assert.h>
#include <limits.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "categories.h"
#include "helper.h"
#include "main.h"
#include "parser.h"
#include "read_init.h"
#include "tui.h"
#include "tui_sidebar.h"

/* The TUI sidebar lives on the right side of the main read view, if there's
 * enough room to draw it. The sidebar is comprised of 2 ncurses windows, the
 * parent and body. These windows are created in create_read_windows() in
 * tui.c. 
 *
 * The sidebar parent is a container for the entire sidebar area, includes
 * the header text. The body is where the bar graphs and values per categories
 * exist. */

#define GRAPH_LENGTH 30

struct bar_graph_dimensions {
	/* For terminal symbols */
	char graph_buffer[GRAPH_LENGTH];
	int graph_len;
	int graph_x_begin;

	/* For text */
	int remain_print_x;
	int planned_print_x;
	int tracked_print_x;
};

struct sidebar_catg_data {
	/* Financials, everything comes from budget_tokens_new */
	char category[MAX_LEN_CATG];
	double remaining;
	double planned;
	double expenses;
	enum transtypes transtype;
};

static void draw_sidebar_body_border(WINDOW *wptr_sidebar_body)
{
	wborder(wptr_sidebar_body, 0, 0, 0, 0, ACS_LTEE, ACS_RTEE, ACS_BTEE, 0);
	mvwxcprintw(wptr_sidebar_body, 0, "Categories");
}

static void write_parent_title(WINDOW *wptr_sidebar_parent)
{
	mvwxcprintw(wptr_sidebar_parent, 0, "Summary");
}

static void draw_sidebar_parent_border(WINDOW *wptr_sidebar_parent)
{
	wborder(wptr_sidebar_parent, 0, 0, 0, 0, ACS_TTEE, 0, 0, 0);
	write_parent_title(wptr_sidebar_parent);
}

static void refresh_sidebar_borders(struct read_wins *wins)
{
	draw_sidebar_body_border(wins->sidebar_body);
	draw_sidebar_parent_border(wins->sidebar_parent);
	write_parent_title(wins->sidebar_parent);
	wrefresh(wins->sidebar_parent);
	wrefresh(wins->sidebar_body);
}

/* If 'y' is greater than the window's maximum (y - 1) (to offset for window
 * border) the function returns false. */
static bool check_y_fit(WINDOW *wptr, int y)
{
	if (y >= getmaxy(wptr) - 1) {
		return false;
	} else {
		return true;
	}
}

/* Create three sidebar windows based on the dimensions from stdscr and the
 * read window parent. wins->parent must be a valid window
 *  
 * |------------------------------\-------\
 * |                              \       \   <--- wins->sidebar_parent
 * |                              \       \
 * |                              \-------\
 * |         wins->parent         \\-----\\
 * |                              \\     \\  <---- (outer) wins->sidebar_body
 * |                              \\     \\  <---- (inner) wins->sidebar_data
 * |                              \\     \\
 * |                              \\-----\\
 * |______________________________\\_____\\
 *
 */
int create_sidebar_windows(struct read_wins *wins)
{
	if (wins->sidebar_parent != NULL &&
		wins->sidebar_body   != NULL &&
		wins->sidebar_data   != NULL) {
		return -1;
	}

	if (wins->parent == NULL) {
		return -1;
	}

	int stdscr_x, stdscr_y, parent_x, parent_header_cols;
	getmaxyx(stdscr, stdscr_y, stdscr_x);
	parent_x = getmaxx(wins->parent);
	parent_header_cols = 5;

	/* Create the sidebar parent window on the right side of the screen */
	wins->sidebar_parent = newwin(stdscr_y - 1,
							      stdscr_x - parent_x + 1,
							      0,
							      parent_x - 1);
	if (wins->sidebar_parent == NULL) {
		window_creation_fail();
	}

	/* Create the sidebar body inside of the sidebar parent, leaving room for
	 * the header */
	wins->sidebar_body = newwin(getmaxy(wins->sidebar_parent) - parent_header_cols,
							    SIDEBAR_COLUMNS + 1, 
							    parent_header_cols,
							    parent_x - 1);
	if (wins->sidebar_body == NULL) {
		window_creation_fail();
	}

	/* Create the sidebar data inside of the sidebar body, leaving the sidebar
	 * body able to draw a border */
	wins->sidebar_data = newwin(getmaxy(wins->sidebar_body) - BOX_OFFSET,
								getmaxx(wins->sidebar_body) - BOX_OFFSET,
								parent_header_cols + 1,
								parent_x);
	if (wins->sidebar_data == NULL) {
		window_creation_fail();
	}

	return 0;
}

static inline bool verify_sidebar_strlen(char *str, WINDOW *wptr)
{
	if (strlen_int(str) > getmaxx(wptr) - BOX_OFFSET) {
		return false;
	}
	return true;
}

/* Prints the category and returns the number of rows printed to */
static int print_body_categories(struct sidebar_deque *deque,
								 size_t index,
								 int print_row)
{
	char *catg = deque->data[index].category;
	int print_n = getmaxx(deque->wptr) - 2;

	if (!check_y_fit(deque->wptr, print_row)) {
		return 0;
	}

	wattron(deque->wptr, COLOR_PAIR(category_color(index)));
	if (!verify_sidebar_strlen(catg, deque->wptr)) {
		mvwprintw(deque->wptr, print_row, 0, "%.*s%s", print_n, catg, "..");
	} else {
		mvwprintw(deque->wptr, print_row, 0, "%s", catg);
	}
	wattroff(deque->wptr, COLOR_PAIR(category_color(index)));
	return 1;
}

static int calculate_graph_length(struct sidebar_deque *deque, size_t index)
{
	int planned 		 = deque->data[index].planned;
	int remaining 		 = deque->data[index].remaining;
	int expenses 		 = deque->data[index].expenses;
	int transaction_type = deque->data[index].transtype;
	int length = 0;

	if (planned == 0) {
		length = GRAPH_LENGTH - 1;
		/* Handles edge case where a category has a planned value of zero and
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

/* Sets the buffer to all spaces and null terminates. */
static inline void init_graph_buffer(char *graph_buff, size_t size)
{
	memset(graph_buff, ' ', size);
	graph_buff[size - 1] = '\0';
}

/* Calculates the text x coordinate members of struct bar_graph_dimensions */
static void calculate_graph_text(WINDOW *wptr, 
								 struct bar_graph_dimensions *graph,
								 double remaining,
								 double planned) 
{
	int max_x = getmaxx(wptr);
	int print_offset = graph->graph_x_begin = (max_x - GRAPH_LENGTH) / 2;
	print_offset += BOX_OFFSET;

	graph->remain_print_x = (max_x - graph->graph_x_begin - \
		strlen_int(" Remaining") - finlen(remaining) - print_offset);

	graph->planned_print_x = (max_x - graph->graph_x_begin - \
		strlen_int(" Planned") - finlen(planned) - print_offset);
	
	graph->tracked_print_x = BOX_OFFSET + 6;
}

/* Calculates and sets all members of 'graph' */
static void init_bar_graph_dimensions(struct bar_graph_dimensions *graph,
									  struct sidebar_deque *deque,
									  size_t index)
{
	init_graph_buffer(graph->graph_buffer, sizeof(graph->graph_buffer));
	graph->graph_len = calculate_graph_length(deque, index);
	calculate_graph_text(deque->wptr,
					     graph,
					     deque->data[index].remaining,
					     deque->data[index].planned);
}

static int print_body_graphs_and_values(struct sidebar_deque *deque,
										size_t index,
										int print_row)
{
	struct bar_graph_dimensions graph = { 0 };
	int max_x = getmaxx(deque->wptr);
	int lines_printed = 0;
	bool color_red = (deque->data[index].remaining < 0.0) ? true : false;

	if (!check_y_fit(deque->wptr, print_row)) {
		return 0;
	}

	init_bar_graph_dimensions(&graph, deque, index);

	/* Prints terminal decoration */
	mvwaddch(deque->wptr, print_row, max_x - 6, ACS_URCORNER);
	mvwaddch(deque->wptr, print_row, max_x - 6 - 1, ACS_HLINE);
	
	if (deque->data[index].transtype == TT_INCOME) {
		mvwprintw(deque->wptr, print_row, graph.planned_print_x,
				  " $%.2f Planned",
				  deque->data[index].planned);
	} else {
		if (color_red) {
			wattron(deque->wptr, COLOR_PAIR(1));
		}
		mvwprintw(deque->wptr, print_row, graph.remain_print_x,
				  " $%.2f Remaining",
				  deque->data[index].remaining);
		if (color_red) {
			wattroff(deque->wptr, COLOR_PAIR(1));
		}
	}

	lines_printed++;
	print_row++;

	wattron(deque->wptr, COLOR_PAIR(category_color(index)));
	mvwprintw(deque->wptr, print_row, graph.graph_x_begin, "[%s]",
		      graph.graph_buffer);
	mvwchgat(deque->wptr, print_row, graph.graph_x_begin + 1, graph.graph_len,
		     A_REVERSE, category_color(index), NULL);
	for (int j = graph.graph_x_begin + graph.graph_len + 1; 
	     j < max_x - graph.graph_x_begin - 1; j++) {
		mvwaddch(deque->wptr, print_row, j, ACS_BULLET);
	}
	wattroff(deque->wptr, COLOR_PAIR(category_color(index)));

	lines_printed++;
	print_row++;

	/* Prints terminal decoration */
	mvwaddch(deque->wptr, print_row, 5, ACS_LLCORNER);
	mvwaddch(deque->wptr, print_row, 6, ACS_HLINE);

	if (deque->data[index].transtype == TT_INCOME) {
		mvwprintw(deque->wptr, print_row, graph.tracked_print_x,
			      " $%.2f Received", deque->data[index].expenses);
	} else {
		mvwprintw(deque->wptr, print_row, graph.tracked_print_x,
			      " $%.2f Tracked",
			      deque->data[index].planned - deque->data[index].remaining);
	}

	lines_printed++;
	return lines_printed;
}

/* Fills the data member of 'deque' with all data required. */
static int get_sidebar_data(struct sidebar_deque *deque,
							struct catg_node *head)
{
	struct budget_tokens_new bt = { 0 };
	struct catg_node *curr = head;
	double expenses;

	for (size_t i = 0; i < deque->len; i++) {
		memset(bt.catg, 0, MAX_LEN_CATG);
		if (curr == NULL) {
			return -1;
		}
		if (tokenize_budget_fpi_new(&bt, curr->catg_fp) != BT_OK) {
			return -1;
		}
		expenses = get_expenditures_per_category_fast(curr);
		deque->data[i].remaining = normalize_near_zero(bt.amount + expenses);
		deque->data[i].expenses  = expenses;
		deque->data[i].planned   = bt.amount;
		deque->data[i].transtype = bt.transtype;
		strncpy(deque->data[i].category, bt.catg, MAX_LEN_CATG - 1);

		curr = curr->next;
	}

	return 0;
}

struct sidebar_deque create_sidebar_deque(struct catg_node *head)
{
	struct sidebar_deque deque = { 0 };
	size_t n_nodes = get_total_nodes(head);
	if (n_nodes == 0) {
		return deque;
	}
	
	deque = (struct sidebar_deque) {
		.data = malloc(sizeof(struct sidebar_catg_data) * n_nodes),
		.start = 0,
		.end = n_nodes - 1,
		.len = n_nodes,
		.begin_node = 0
	};

	if (deque.data == NULL) {
		mem_alloc_fail();
	}

	if (get_sidebar_data(&deque, head) == -1) {
		free(deque.data);
		deque.data = NULL;
		deque.len = 0;
	}

	return deque;
}

int init_sidebar_body(struct sidebar_deque *deque)
{
	if (deque == NULL) {
		return 0;
	}
	size_t index = deque->start;
	int n_displayed = 0;
	int print_row = 0;

	wclear(deque->wptr);

	while (print_row < getmaxy(deque->wptr) - 4) {
		print_row += print_body_categories(deque, index, print_row);
		print_row += print_body_graphs_and_values(deque, index, print_row);
		index++;
		if (index > deque->len) {
			break;
		}
	}

	wrefresh(deque->wptr);
	return n_displayed;
}

/* Prints financial information in the sidebar parent window */
static int print_sidebar_parent_info(WINDOW *wptr,
									 struct vec_d *psc,
									 double leftover)
{
	struct vec2f_fin pb;
	calculate_balance(&pb, psc);
	double remaining = pb.income - pb.expense;
	remaining = normalize_near_zero(remaining);
	int x = 1; /* Offset from the border */
	int y = 1; /* Offset from the border */
	int max_x = getmaxx(wptr);
	int print_x = 0;

	mvwprintw(wptr, y, x, "Income:");
	print_x = max_x - finlen(pb.income) - BOX_OFFSET;
	mvwprintw(wptr, y, print_x, "$%.2f", pb.income);
	y++;

	mvwprintw(wptr, y, x, "Expenses:");
	print_x = max_x - finlen(pb.expense) - BOX_OFFSET;
	mvwprintw(wptr, y, print_x, "$%.2f", pb.expense);
	y++;

	mvwprintw(wptr, y, x, "Remaining:");
	if (remaining < 0.0) {
		wattron(wptr, COLOR_PAIR(1));
	}
	print_x = max_x - finlen(remaining) - BOX_OFFSET;
	mvwprintw(wptr, y, print_x, "$%.2f", remaining);
	wattroff(wptr, COLOR_PAIR(1));
	y++;

	mvwprintw(wptr, y, x, "Left to Budget:");
	print_x = max_x - finlen(leftover) - BOX_OFFSET;
	mvwprintw(wptr, y, print_x, "$%.2f", leftover);
	y++;

	wrefresh(wptr);
	return y;
}

/* Draws a border around the window 'wptr' with "T" shaped characters to
 * mesh seamlessly with the window border of the sidebar. */
void draw_parent_box_with_sidebar(WINDOW *wptr)
{
	wborder(wptr, 0, 0, 0, 0, 0, ACS_TTEE, 0, ACS_BTEE);
	wrefresh(wptr);
}

struct sidebar_deque sidebar_create(struct read_wins *wins,
									struct vec_d *records,
									struct catg_node *head)
{
	refresh_sidebar_borders(wins);
	print_sidebar_parent_info(wins->sidebar_parent,
						      records,
						      get_left_to_budget(head));
	struct sidebar_deque deque = create_sidebar_deque(head);
	deque.wptr = wins->sidebar_data;
	return deque;
}
