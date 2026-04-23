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

#ifndef CREATE_H
#define CREATE_H

#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "vector.h"
#include "parser.h"
#include "tui_input.h"

enum AddMainMenu {
	ADD_TRANSACTION = 0,
	ADD_CATEGORY,
	CREATE_BUDGET
}; 

void insert_budget_record(char *catg, int m, int y, int transtype, double amt);
char *create_budget_record(int yr, int mo);
void insert_transaction_record(int insert_line, _transact_tokens_t *ld);

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
void create_transaction(int year, int month);
void create_transaction_default(void);
int nc_create_new_budget_intret(void);
struct full_date *nc_create_new_budget(void);
int get_add_selection(void);
void add_main_with_date(struct Datevals *dv);
void add_main_no_date(void);

#endif
