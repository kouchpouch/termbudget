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

/* Returns the row on the bottom 4th of wptr */
static int last_quarter_row(WINDOW *wptr)
{
	return getmaxy(wptr) - getmaxy(wptr) / 4;
}

static int first_quarter_row(WINDOW *wptr)
{
	return getmaxy(wptr) / 4;
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

/* Returns the number of spaces between each bar graph for the overview
 * option */
static void nc_print_overview_graphs(WINDOW *wptr,
									 struct vec_d *months,
									 int year)
{
	double ratios[12] = {0.0}; // Holds each month's income/expense ratio
	double maxvals[12] = {0.0};
	struct balances pb_, *pb = &pb_;
	int space = calculate_overview_columns(wptr);
	int mo = 1;

	enum GraphRatios {
		NO_INCOME = -1,
		NO_EXPENSE = -2,
	};

	for (size_t i = 0; i < months->size && mo <= 12; i++, mo++) {
		if (months->data[i] == mo) {
			struct vec_d *pbo = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(pb, pbo);
			if (pb->income == 0) { // Prevent a div by zero
				ratios[mo - 1] = NO_INCOME;
			} else if (pb->expense == 0) {
				ratios[mo - 1] = NO_EXPENSE;
			} else {
				ratios[mo - 1] = pb->expense / pb->income;
			}
			maxvals[mo - 1] = pb->expense >= pb->income ? pb->expense : pb->income;
			free(pbo);
		} else {
			i--;
		}
	}

	int bar_width = 3;
	int cur = (getmaxx(wptr) - space * 11) / 2 - 1;
	double exp_bar_len = 0;
	double inc_bar_len = 0;
	int max_bar_len = (last_quarter_row(wptr) - 2) - first_quarter_row(wptr) + 4;

	double maxval = get_max_value(12, maxvals);

	if (debug_flag) {
		wmove(wptr, 1, 1);
		for (int i = 0; i < 12; i++) {
			wprintw(wptr, "RAT: %.2f VAL: %.2f\n", ratios[i], maxvals[i]);
		}
	}

	for (int i = 0; i < 12; i++) {
		if (maxvals[i] == 0) {
			cur += space;
			continue;
		}

		if (ratios[i] > 1) {
			// Expenses are greater than income
			exp_bar_len = maxvals[i] / maxval;
			exp_bar_len = max_bar_len * exp_bar_len;
			inc_bar_len = exp_bar_len / ratios[i];

		} else if (ratios[i] == NO_INCOME) {
			// There are only expenses
			exp_bar_len = max_bar_len * (maxvals[i] / maxval);
			inc_bar_len = 0;

		} else if (ratios[i] == NO_EXPENSE) {
			inc_bar_len = max_bar_len * (maxvals[i] / maxval);
			exp_bar_len = 0;

		} else {
			// Income is greater than expenses
			inc_bar_len = maxvals[i] / maxval;
			inc_bar_len = max_bar_len * inc_bar_len;
			exp_bar_len = inc_bar_len * ratios[i];
		}

		if (inc_bar_len > 0 && inc_bar_len < 1) {
			inc_bar_len = 1;
		}

		if (exp_bar_len > 0 && exp_bar_len < 1) {
			exp_bar_len = 1;
		}

		for (int j = 0; j < (int)inc_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_GREEN, NULL);
		}

		cur += bar_width;

		for (int j = 0; j < (int)exp_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_RED, NULL);
		}

		cur += space - bar_width;
	}
}

static void nc_print_overview_balances(WINDOW *wptr,
									   struct vec_d *months,
									   int year)
{
	int tmpx = 0;
	int space = calculate_overview_columns(wptr);
	int y = last_quarter_row(wptr) + 2;
	int cur = (getmaxx(wptr) - space * 11) / 2;
	int mo = 1;
	struct balances pb_, *pb = &pb_;
	for (size_t i = 0; i < months->size && mo <= 12; i++, mo++) {
		tmpx = 0;

		if (months->data[i] == mo) {
			struct vec_d *pbo = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(pb, pbo);
			tmpx = intlen(pb->income) / 2;
			wmove(wptr, y, cur - tmpx);
			wattron(wptr, COLOR_PAIR(2));
			wprintw(wptr, "+$%.0f", pb->income);
			wattroff(wptr, COLOR_PAIR(2));
			tmpx = intlen(pb->expense) / 2;
			wmove(wptr, y + 1, cur - tmpx);
			wattron(wptr, COLOR_PAIR(1));
			wprintw(wptr, "-$%.0f", pb->expense);
			wattron(wptr, COLOR_PAIR(1));
			free(pbo);
			cur += space;
		} else {
			wmove(wptr, y, cur);
			wattron(wptr, COLOR_PAIR(2));
			wprintw(wptr, "+$0");
			wattroff(wptr, COLOR_PAIR(2));
			wmove(wptr, y + 1, cur);
			wattron(wptr, COLOR_PAIR(1));
			wprintw(wptr, "-$0");
			wattron(wptr, COLOR_PAIR(1));
			cur += space;
			i--;
		}
	}

	wrefresh(wptr);
}

static void nc_print_overview_months(WINDOW *wptr)
{
	int space = calculate_overview_columns(wptr);
	int y = last_quarter_row(wptr);
	int cur = (getmaxx(wptr) - space * 11) / 2;

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

static unsigned int nc_overview_loop(WINDOW *wptr,
									 struct vec_d *months,
									 int year)
{
	unsigned int flag = 0;
	int c;
	int space = calculate_overview_columns(wptr);
	if (space > 0) {
		nc_print_overview_months(wptr);
		nc_print_overview_balances(wptr, months, year);
		nc_print_overview_graphs(wptr, months, year);
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

void nc_overview_setup(int year)
{
	WINDOW *wptr_parent = newwin(LINES - 1, 0, 0, 0);
	WINDOW *wptr_data = create_lines_subwindow(getmaxy(wptr_parent) - 1,
									getmaxx(wptr_parent), 1, BOX_OFFSET);
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);
	box(wptr_parent, 0, 0);
	mvwxcprintw_digit(wptr_parent, 0, year);
	wrefresh(wptr_parent);
	
	FILE *fptr = open_record_csv("r");
	struct vec_d *months = get_months_with_data(fptr, year, 1);
	fclose(fptr);

	unsigned int flag = nc_overview_loop(wptr_data, months, year);

	free(months);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr_data);

	switch (flag) {
	case RESIZE:
		nc_overview_setup(year);
		break;
	case QUIT:
		break;
	default:
		break;
	}
}
