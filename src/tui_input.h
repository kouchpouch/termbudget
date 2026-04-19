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

#ifndef TUI_INPUT_H
#define TUI_INPUT_H

#include <ncurses.h>

#include "parser.h"

struct UserInput {
	char *str;
	int flag;
};

struct UserInputDigit {
	int data;
	int flag;
};

void nc_user_input(int n, WINDOW *wptr, struct UserInput *pui);

void nc_input_n_digits
(struct UserInputDigit *puid, WINDOW *wptr, size_t max_len, size_t min_len);

bool nc_confirm_input_loop(WINDOW *wptr);

bool nc_confirm_input(char *msg);

bool nc_confirm_record(_transact_tokens_t *ld);
/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_month(void);

/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_year(void);

/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_day(int month, int year);

char *nc_input_string(char *msg);

int nc_input_category_type(void);

int nc_input_transaction_type(void);

double nc_input_amount(void);

double nc_input_budget_amount(void);

char *nc_select_category(int month, int year);

#endif
