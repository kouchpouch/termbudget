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

#ifndef TUI_INPUT_H
#define TUI_INPUT_H

#include <ncurses.h>

#include "parser.h"

typedef struct __full_date_t {
	int day;
	int month;
	int year;
} _full_date_t;

struct UserInput {
	char *str;
	int flag;
};

int nc_input_full_date(int old_mo, int old_day, int old_yr, _full_date_t *new_date);

bool nc_confirm_input_loop(WINDOW *wptr);

bool nc_confirm_input(char *msg);

bool nc_confirm_record(_transact_tokens_t *ld);

int nc_input_month(int old_month, int old_year);

int nc_input_year(int old_year);

int nc_input_day(int month, int year, int old_day);

char *nc_input_string(char *msg);

int nc_input_category_type(void);

int nc_input_transaction_type(void);

double nc_input_amount(void);

double nc_input_budget_amount(void);

char *nc_select_category(int month, int year);

#endif
