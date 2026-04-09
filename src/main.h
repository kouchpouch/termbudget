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
#ifndef MAIN_H
#define MAIN_H

#include <stddef.h>
#include <ncurses.h>
#include "vector.h"
#include "categories.h"
#include "parser.h"

#define REALLOC_INCR 64
#define INDEX_ALLOC 1024
#define MAX_ALLOC 1024 * 1024

#define CSV_FIELDS 7
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8

#define MIN_YEAR 2000
#define MAX_YEAR 2100
#define MONTHS_IN_YEAR 12

#define MAX_LEN_AMOUNT 9
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4
#define MIN_INPUT_CHAR 2
#define INPUT_MSG_Y_OFFSET 2

struct ScrollCursor {
	/* Total size of the data that can be shown. */
	int total_rows; 

	/* Total number of records and/or categories displayed on the screen. */
	int displayed; 
	int select_idx;
	int cur_y;
	int catg_node;
	int catg_data;
	size_t sidebar_idx;
};

struct DispCursor {
	int first;
	int last;
};

struct Datevals {
	int year;
	int month;
};

struct Plannedvals {
	double inc;
	double exp;
};

struct Balances {
	double income;
	double expense;
};

/* Exits the program with "exit(1)" and prints the error message. */
extern void mem_alloc_fail(void);

/* Sets the values of struct members 'pb' using records in RECORD_DIR
 * at file positions of 'pbo' */
extern void calculate_balance(struct Balances *pb, Vec *pbo);

extern struct Plannedvals *get_total_planned(CategoryNode **nodes);

extern double get_left_to_budget(CategoryNode **nodes);

extern void nc_print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off);

// To be moved from main.h
extern bool duplicate_category_exists(struct Categories *psc, char *catg);

extern bool nc_confirm_record(struct LineData *ld);

extern char *nc_select_category(int month, int year);

extern int show_help_subwindow(void);

extern void nc_edit_transaction(long b);

extern void nc_overview_setup(int year);


#endif
