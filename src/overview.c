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
#include <ncurses.h>

#include "main.h"
#include "tui.h"
#include "helper.h"
#include "parser.h"
#include "overview.h"
#include "vector.h"
#include "flags.h"
#include "filemanagement.h"

#define MIN_OVERIVEW_WIDTH 84
#define OVERVIEW_BAR_WIDTH 3

enum GraphRatios {
	NO_INCOME = -1,
	NO_EXPENSE = -2,
};

static const char *abbr_months[] = {
	"JAN", 
	"FEB", 
	"MAR", 
	"APR",
	"MAY",
	"JUN",
	"JUL",
	"AUG",
	"SEP",
	"OCT",
	"NOV",
	"DEC"
};

struct overview_bar_graph {
	double height_exp;
	double height_inc;
	double max_val;
	int max_height;
	int width;
};

/* Returns the row on the bottom 4th of wptr */
static int last_quarter_row(WINDOW *wptr)
{
	return getmaxy(wptr) - getmaxy(wptr) / 4;
}

static int first_quarter_row(WINDOW *wptr)
{
	return getmaxy(wptr) / 4;
}

static void printw_bal_green(WINDOW *wptr, double f)
{
	wattron(wptr, COLOR_PAIR(2));
 	wprintw(wptr, "+$%.0f", f);
	wattroff(wptr, COLOR_PAIR(2));
}

static void printw_bal_red(WINDOW *wptr, double f)
{
	wattron(wptr, COLOR_PAIR(1));
 	wprintw(wptr, "-$%.0f", f);
	wattroff(wptr, COLOR_PAIR(1));
}

static double get_max_value(int elements, double *arr)
{
	double tmp = 0.0;
	double max = 0.0;

	for (int i = 0; i < elements; i++) {
		tmp = arr[i];
		if (tmp > max) {
			max = tmp;	
		}
	}

	return max;
}

int calculate_overview_columns(WINDOW *wptr)
{
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

	int space = 0; /* Space between graphs */

	if (getmaxx(wptr) >= MIN_OVERIVEW_WIDTH) {
		space = getmaxx(wptr) / 12;
		if (space % 2 == 0) { /* Normalize */
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

static void calculate_bar_height(struct overview_bar_graph *bar,
								 double rat,
								 double maxval)
{
	if (rat > 1) {
		/* Expenses are greater than income */
		bar->height_exp = maxval / bar->max_val;
		bar->height_exp = bar->max_height * bar->height_exp;
		bar->height_inc = bar->height_exp / rat;

	} else if (rat == NO_INCOME) {
		/* There are only expenses */
		bar->height_exp = bar->max_height * (maxval / bar->max_val);
		bar->height_inc = 0;

	} else if (rat == NO_EXPENSE) {
		bar->height_inc = bar->max_height * (maxval / bar->max_val);
		bar->height_exp = 0;

	} else {
		/* Income is greater than expenses */
		bar->height_inc = maxval / bar->max_val;
		bar->height_inc = bar->max_height * bar->height_inc;
		bar->height_exp = bar->height_inc * rat;
	}

	if (bar->height_inc > 0 && bar->height_inc < 1) {
		bar->height_inc = 1;
	}

	if (bar->height_exp > 0 && bar->height_exp < 1) {
		bar->height_exp = 1;
	}
}

static void print_overview_graphs(WINDOW *wptr,
									 struct vec_d *months,
									 int year,
									 int space)
{
	double ratios[MONTHS_IN_YEAR] = {0.0};
	double maxvals[MONTHS_IN_YEAR] = {0.0};
	struct overview_bar_graph bar = {
		.width = OVERVIEW_BAR_WIDTH
	};
	struct vec2f_fin bal;
	struct vec_d *records;
	int mo = 1;
	int curr_x;

	for (size_t i = 0; i < months->size && mo <= MONTHS_IN_YEAR; i++, mo++) {
		if (months->data[i] == mo) {
			records = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(&bal, records);
			if (bal.income == 0) { /* Prevent div by zero */
				ratios[mo - 1] = NO_INCOME;
			} else if (bal.expense == 0) {
				ratios[mo - 1] = NO_EXPENSE;
			} else {
				ratios[mo - 1] = bal.expense / bal.income;
			}

			if (bal.expense >= bal.income) {
				maxvals[mo - 1] = bal.expense;
			} else {
				maxvals[mo - 1] = bal.income;
			}

			free(records);
		} else {
			i--;
		}
	}

	curr_x = (getmaxx(wptr) - space * 11) / 2 - (BOX_OFFSET);
	bar.max_height = (last_quarter_row(wptr) - 2) - first_quarter_row(wptr) + 4;
	bar.max_val = get_max_value(MONTHS_IN_YEAR, maxvals);

	if (debug_flag) {
		wmove(wptr, 1, 1);
		for (int i = 0; i < MONTHS_IN_YEAR; i++) {
			wprintw(wptr, "RAT: %.2f VAL: %.2f\n", ratios[i], maxvals[i]);
		}
	}

	for (int i = 0; i < MONTHS_IN_YEAR; i++) {
		if (maxvals[i] == 0) {
			curr_x += space;
			continue;
		}

		calculate_bar_height(&bar, ratios[i], maxvals[i]);

		/* Print income bar */
		for (int j = 0; j < (int)bar.height_inc; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, curr_x, bar.width, 
					 A_REVERSE, COLOR_GREEN, NULL);
		}

		curr_x += bar.width;

		/* Print expense bar */
		for (int j = 0; j < (int)bar.height_exp; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, curr_x, bar.width, 
					 A_REVERSE, COLOR_RED, NULL);
		}

		curr_x += space - bar.width;
	}
}

static void print_overview_balances(WINDOW *wptr,
						   struct vec_d *months,
						   int year,
						   int space)
{
	struct vec2f_fin balances;
	struct vec_d *records = NULL;
	size_t i;
	int y = last_quarter_row(wptr) + 2;
	int cur = ((getmaxx(wptr) - space * 11) / 2) - 1;
	int mo = 1;

	for (i = 0; i < months->size && mo <= 12; i++, mo++) {
		if (months->data[i] == mo) {
			records = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(&balances, records);

			wmove(wptr, y, cur - (intlen(balances.income) / 2));
			printw_bal_green(wptr, balances.income);

			wmove(wptr, y + 1, cur - (intlen(balances.expense) / 2));
			printw_bal_red(wptr, balances.expense);
			free(records);
			records = NULL;
			cur += space;
		} else {
			wmove(wptr, y, cur);
			printw_bal_green(wptr, 0);

			wmove(wptr, y + 1, cur);
			printw_bal_red(wptr, 0);
			cur += space;
			i--;
		}
	}

	wrefresh(wptr);
}

static void print_overview_months(WINDOW *wptr, int space)
{
	int y = last_quarter_row(wptr);
	int cur = ((getmaxx(wptr) - space * 11) / 2) - 1;

	if (debug_flag) {
		wprintw(wptr, "INIT CUR: %d ", cur);
		wprintw(wptr, "SPACE: %d ", space);
		wprintw(wptr, "SPACEx11: %d ", space * 11);
		wprintw(wptr, "MAX X: %d ", getmaxx(wptr));
	}

	for (int i = 0; i < 12; i++) {
		wmove(wptr, y, cur);
		wprintw(wptr, "%s", abbr_months[i]);
		cur += space;
	}
}

static unsigned char overview_loop(WINDOW *wptr,
									 struct vec_d *months,
									 int year)
{
	unsigned char flag = 0;
	int c;
	int space = calculate_overview_columns(wptr);
	if (space > 0) {
		print_overview_months(wptr, space);
		print_overview_balances(wptr, months, year, space);
		print_overview_graphs(wptr, months, year, space);
		nc_print_quit_footer(stdscr);
		wrefresh(wptr);
	} else {
		wprintw(wptr, "Terminal is too small");
		wrefresh(wptr);
	}

	while (1) {
		c = wgetch(wptr);
		switch (c) {
		case KEY_RESIZE:
			return RESIZE;
		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			return QUIT;
		default:
			flag = NO_SELECT;
			break;
		}
	}

	return flag;
}

void overview_setup(int year)
{
	WINDOW *wptr_parent = newwin(LINES - 1, 0, 0, 0);
	WINDOW *wptr_data = create_lines_subwindow(getmaxy(wptr_parent) - 1,
									getmaxx(wptr_parent), 1, BOX_OFFSET);
	FILE *fptr = open_record_csv("r");
	struct vec_d *months = get_months_with_data(fptr, year, 1);
	unsigned char flag;

	fclose(fptr);
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);
	box(wptr_parent, 0, 0);
	mvwxcprintw_digit(wptr_parent, 0, year);
	wrefresh(wptr_parent);
	
	flag = overview_loop(wptr_data, months, year);

	free(months);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr_data);

	switch (flag) {
	case RESIZE:
		overview_setup(year);
		break;
	case QUIT:
		break;
	default:
		break;
	}
}
