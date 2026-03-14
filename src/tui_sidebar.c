//#include "tui_sidebar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "tui.h"

void sidebar_body_border(WINDOW *wptr) {
	wborder(wptr, 0, 0, 0, 0, ACS_LTEE, ACS_RTEE, ACS_BTEE, 0);
	mvwxcprintw(wptr, 0, "Categories");
	wrefresh(wptr);
}

WINDOW *create_sidebar_body(WINDOW *wptr_sidebar, int y, int x) {
	WINDOW *wptr = newwin(getmaxy(wptr_sidebar) - y, SIDEBAR_COLUMNS + 1, y, x); 
	if (wptr == NULL) {
		window_creation_fail();
	}
	return wptr;
}

bool verify_sidebar_strlen(char *str, WINDOW *wptr) {
	if (strlen(str) > getmaxx(wptr) - BOX_OFFSET) {
		return false;
	} else {
		return true;
	}
}

/* Prints the string 'str' and returns the number of rows printed to */
int print_sidebar_category_string(char *str, WINDOW *wptr, int y, int x, int i) {
	wattron(wptr, COLOR_PAIR(category_color(i)));
	if (!verify_sidebar_strlen(str, wptr)) {
		mvwprintw(wptr, y, x, "%.*s%s", getmaxx(wptr) - (BOX_OFFSET + 2), str, "..");
	} else {
		mvwprintw(wptr, y, x, "%s", str);
	}
	wattroff(wptr, COLOR_PAIR(category_color(i)));
	return 1;
}

int print_sidebar_category_values(double inc, double exp, int tt, WINDOW *wptr, int y, int i) {
	char graph[30];
	for (int i = 0; i < 30; i++) {
		graph[i] = ' ';
	}
	graph[sizeof(graph) - 1] = '\0';
	double remaining = 0;
	if (exp <= 0) {
		remaining = inc + exp;
	} else {
		remaining = inc - exp;
	}

	int graph_len = (sizeof(graph) - 1) * (1 - (remaining / inc));
	if (graph_len > sizeof(graph) - 1) {
		graph_len = sizeof(graph) - 1;
	}
	int fill_graph;
	int graph_x_begin = ((getmaxx(wptr) - sizeof(graph)) / 2);
	int remain_x_begin = (getmaxx(wptr) - graph_x_begin - strlen(" Remaining") - finlen(remaining) - BOX_OFFSET - 4);
	int planned_x_begin = (getmaxx(wptr) - graph_x_begin - strlen(" Planned") - finlen(inc) - BOX_OFFSET - 4);
	int tracked_x_begin = BOX_OFFSET + 6;

	mvwaddch(wptr, y, getmaxx(wptr) - 5 - BOX_OFFSET, ACS_URCORNER);
	mvwaddch(wptr, y, getmaxx(wptr) - 5 - BOX_OFFSET - 1, ACS_HLINE);
	if (tt == TT_INCOME) {
		mvwprintw(wptr, y, planned_x_begin, " $%.2f Planned", inc);
	} else {
		mvwprintw(wptr, y, remain_x_begin, " $%.2f Remaining", remaining);
	}
	y++;

	wattron(wptr, COLOR_PAIR(category_color(i)));
	mvwprintw(wptr, y, graph_x_begin, "[%s]", graph);
	mvwchgat(wptr, y, graph_x_begin + 1, graph_len, A_REVERSE, category_color(i), NULL);
	//wattroff(wptr, COLOR_PAIR(category_color(i)));
	fill_graph = graph_x_begin + 1 + graph_len;
	for (int j = fill_graph; j < getmaxx(wptr) - graph_x_begin - 1; j++) {
		mvwaddch(wptr, y, j, ACS_BULLET);
	}
	wattroff(wptr, COLOR_PAIR(category_color(i)));
	y++;

	mvwaddch(wptr, y, BOX_OFFSET + 4, ACS_LLCORNER);
	mvwaddch(wptr, y, BOX_OFFSET + 5, ACS_HLINE);
	if (tt == TT_INCOME) {
		mvwprintw(wptr, y, tracked_x_begin, " $%.2f Received", exp);
	} else {
		mvwprintw(wptr, y, tracked_x_begin, " $%.2f Tracked", inc - remaining);
	}
	return 4;
}

void init_sidebar_body(WINDOW *wptr, CategoryNode **nodes) {
	sidebar_body_border(wptr);
	int i = 0;
	int y = 1;
	int x = 1;
	double exp;
	while (1) {
		struct BudgetTokens *bt = tokenize_budget_byte_offset(nodes[i]->catg_fp);
		exp = get_expenditures_per_category(bt);
		if (nodes[i]->next == NULL) {
			y += print_sidebar_category_string(bt->catg, wptr, y, x, i);
			y += print_sidebar_category_values(bt->amount, exp, bt->transtype, wptr, y, i);
			free_budget_tokens(bt);
			bt = NULL;
			break;
		} else {
			y += print_sidebar_category_string(bt->catg, wptr, y, x, i);
			y += print_sidebar_category_values(bt->amount, exp, bt->transtype, wptr, y, i);
			free_budget_tokens(bt);
			bt = NULL;
		}
		i++;
	}

}

double get_left_to_budget(CategoryNode **nodes) {
	struct Plannedvals *pv = get_total_planned(nodes);
	double ret = pv->inc - pv->exp;
	free(pv);
	pv = NULL;
	return ret;
}

int nc_print_sidebar_head(WINDOW *wptr, Vec *psc, double leftover) {
	int x = 1;
	int y = 1;
	int max_x = getmaxx(wptr);
	struct Balances pb;
	calculate_balance(&pb, psc);
	double remaining = pb.income - pb.expense;

	mvwprintw(wptr, y, x, "Income:");
	mvwprintw(wptr, y, max_x - (finlen(pb.income) + BOX_OFFSET), "$%.2f", pb.income);
	y++;

	mvwprintw(wptr, y, x, "Expenses:");
	mvwprintw(wptr, y, max_x - (finlen(pb.expense) + BOX_OFFSET), "$%.2f", pb.expense);
	y++;

	mvwprintw(wptr, y, x, "Remaining:");
	if (pb.income - pb.expense < 0) {
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
