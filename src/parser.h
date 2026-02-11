/*
 * Includes parsing functions for CSV files used in termBudget.
 *
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
#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"

struct BudgetTokens {
	int m;
	int y;
	char *catg;
	double amount;
};

struct LineData {
	int month;
	int day;
	int year;
	char *category;
	char *desc;
	int transtype;
	double amount;
	int linenum;
};

/* Opens file located at RECORD_DIR in mode mode. Exits on failure */
extern FILE *open_record_csv(char *mode);

/* Opens file located at BUDGET_DIR in mode mode. Exits on failure */
extern FILE *open_budget_csv(char *mode);

/* Opens file located at TMP_DIR in w+ mode. Exits on failure */
extern FILE *open_temp_csv(void);

/* Returns the line number of byte-offset b in file CSV_DIR */
extern unsigned int boff_to_linenum(long b);

/* Moves char * forward n fields in the CSV */
extern void seek_n_fields(char **line, int n);

/* Reads the header of the file until a newline is found */
extern int seek_beyond_header(FILE *fptr);

/* Returns the number of fields by reading the header of fptr */
extern int get_num_fields(FILE *fptr);

/* Frees the struct and any applicable members */
extern void free_budget_tokens(struct BudgetTokens *pbt);

/* Generic function for getting record byte offsets by some parameter.
 * To disregard an argument, pass a negative value for any int or double and
 * pass NULL for any char *, these arguments will not be checked.
 *
 * EXAMPLE:
 * Get records by date: 
 *     get_records_by_any(2, 5, 2025, NULL, NULL, -1, -1, NULL);
 * Get records by category: 
 *     get_records_by_any(-1, -1, -1, paychecks, NULL, -1, -1, NULL);
 * Get records by category in a defined chunk: 
 *     get_records_by_any(-1, -1, -1, paychecks, NULL, -1, -1, chunk);
 * 													   
 * Returns a Vec data member containing byte offset values of records that
 * match and Vec data member size with the total number of records that
 * matched. */
extern Vec *get_records_by_any(int month, int day, int year, char *category,
							   char *description, int transtype, double amount,
							   Vec *chunk);

/* Returns number of categories and the string literal categories of given
 * month and year in BUDGET_DIR(main.h) */
extern struct Categories *get_budget_catg_by_date(int month, int year);

/* Returns Vec containing byte offsets of each category that matches month,
 * year in BUDGET_DIR. */
extern Vec *get_budget_catg_by_date_bo(int month, int year);

/* Returns malloc'd tokenized variables in BudgetTokens by seeking the file
 * position indicator to bo. BudgetTokens catg is separately malloc'd and must 
 * be free'd. Use free_budget_tokens(). 
 * Returns NULL on failure, pointer to struct BudgetTokens on success. */
extern struct BudgetTokens *tokenize_budget_byte_offset(long bo);

/* Returns malloc'd tokenized variables in BudgetTokens which matches
 * line number line. BudgetTokens catg is separately malloc'd and must 
 * be free'd. Use free_budget_tokens().
 * Returns NULL on failure, pointer to struct BudgetTokens on success. */
extern struct BudgetTokens *tokenize_budget_line(int line);

/* Populates ld members with tokens from str */
extern void tokenize_record(struct LineData *ld, char **str);

/* Returns an integer value of a given line number line and of field number 
 * field. Field numbers start at 1 */
extern int get_int_field(int line, int field);

/* 
 * Rewrite of index_csv to include amortized memory allocation. Amortization on
 * other functions in the program doesn't exist, but due to this function
 * reading and storing the entire file, it may as well be amortized. The old
 * function scans through the entire RECORD_DIR file twice. Once to get the
 * total number of records to allocate the appropriate amount of memory, then
 * goes through the entire file again and inserts the byte offset vales into
 * a dynamically sized array. There will most likely be a performance decrease
 * for small files but for very large RECORD_DIR files this approach will most
 * likely be faster.
 *
 * To counter that, the initial allocation will be much larger than
 * REALLOC_INCR which is used in cases where realistically not many values
 * will be stored in the array.
 *
 * Also changed return type to be a Vec. Removes the local variable to keep
 * track of capacity, it is now apart of the struct.
 */
extern Vec *index_csv(FILE *fptr);

#endif
