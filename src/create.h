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

#include "main.h"
#include "vector.h"

#ifndef CREATE_H
#define CREATE_H

enum AddMainMenu {
	ADD_TRANSACTION = 0,
	ADD_CATEGORY,
	CREATE_BUDGET
}; 

extern void insert_budget_record(char *catg, int m, int y, int transtype, double amt);

extern char *create_budget_record(int yr, int mo);

extern void insert_transaction_record(int linetoadd, struct LineData *ld);

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
extern void create_transaction(int year, int month);

extern void create_transaction_default(void);

extern struct Datevals *nc_create_new_budget(void);

extern int get_add_selection(void);

extern void add_main_with_date(struct Datevals *dv);

extern void add_main_no_date(void);

#endif
