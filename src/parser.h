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

#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "categories.h"
#include "vector.h"

#define CSV_FIELDS 7

enum transtypes {
	TT_EXPENSE = 0,
	TT_INCOME = 1
};

typedef struct __budget_header_t {
	int month;
	int year;
	int catg;
	int transtype;
	int value;
	int n_fields;
} _budget_header_t;

typedef struct __record_header_t {
	int month;
	int day;
	int year;
	int catg;
	int desc;
	int transtype;
	int value;
	int n_fields;
} _record_header_t;

typedef struct __budget_tokens_t {
	int m;
	int y;
	char *catg;
	int transtype;
	double amount;
} _budget_tokens_t;

typedef struct __transact_tokens_t {
	int month;
	int day;
	int year;
	char *category;
	char *desc;
	int transtype;
	double amount;
	int linenum;
} _transact_tokens_t;

/* Returns total lines in the CSV */
int get_total_csv_lines(void);

/* Returns the line number of byte-offset b in file CSV_DIR */
unsigned int boff_to_linenum(long b);

/* Returns the line number of byte-offset b in file BUDGET_DIR */
unsigned int boff_to_linenum_budget(long b);

/* Moves char * forward n fields in the CSV */
void seek_n_fields(char **line, int n);

/* Returns struct initialized with the values of the field number which
 * that member starts at, the first field is 0. If a field was not found,
 * the member is set to -1 */
_record_header_t *parse_record_header(FILE *fptr);

/* Returns struct initialized with the values of the field number which
 * that member starts at, the first field is 0. If a field was not found,
 * the member is set to -1 */
_budget_header_t *parse_budget_header(FILE *fptr);

/* Reads the header of the file until a newline is found */
int seek_beyond_header(FILE *fptr);

/* Returns the number of fields by reading the header of fptr */
int get_num_fields(FILE *fptr);

/* Frees the struct and any applicable members */
void free_budget_tokens(_budget_tokens_t *pbt);

/* Loop through each category in the budget. Returns true or false if the
 * category exists for the given date range */
bool category_exists_in_budget(char *catg, int month, int year);

/* Returns bool if a month or year exists. Only checks BUDGET_DIR, as any
 * record there must either contain records or categories. Month parameter
 * is optional, call function with the month paramater <= 0*/
bool month_or_year_exists(int m, int y);

/* Returns all income records subtracted by expense records */
double get_expenditures_per_category(_budget_tokens_t *bt);

/* Returns a vector of years which contain data */
_vector_t *get_years_with_data(FILE *fptr, int field);

/*
 * Returns a malloc'd vector containing all months with records in file 'fptr',
 * which match the year 'matchyear'. Pass the parameter 'fields' to tell
 * the function how many CSV fields to skip ahead. This 'fields' passing is
 * temporary and a new function will be added to find which fields to read
 * based on the header.
 */
_vector_t *get_months_with_data(FILE *fptr, int matchyear, int field);

/*
 * Returns a vector containing the line numbers of the records with matching
 * month and year.
 */
_vector_t *get_matching_line_nums(FILE *fptr, int month, int year);

/*
 * For a given month and year, return an array of strings from the category
 * field of the RECORD_DIR csv file.
 */
struct catg_vec *get_categories(int month, int year);

/* Uses get_records_by_any with all parameters except year as NULL
 * or -1, omitting the search for other fields */
_vector_t *get_records_by_yr(int year);

/* Uses get_records_by_any with all parameters except month and year as NULL
 * or -1, omitting the search for other fields */
_vector_t *get_records_by_mo_yr(int month, int year);

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
 * Returns a _vector_t data member containing byte offset values of records that
 * match and _vector_t data member size with the total number of records that
 * matched. */
_vector_t *get_records_by_any(int month, int day, int year, char *category,
							   char *description, int transtype, double amount,
							   _vector_t *chunk);

/* Returns number of categories and the string literal categories of given
 * month and year in BUDGET_DIR */
struct catg_vec *get_budget_catg_by_date(int month, int year);

/* Returns _vector_t containing byte offsets of each category that matches month,
 * year in BUDGET_DIR. */
_vector_t *get_budget_catg_by_date_bo(int month, int year);

/* Returns malloc'd tokenized variables in BudgetTokens by seeking the file
 * position indicator to bo. BudgetTokens catg is separately malloc'd and must 
 * be free'd. Use free_budget_tokens(). 
 * Returns NULL on failure, pointer to _budget_tokens_t on success. */
_budget_tokens_t *tokenize_budget_fpi(long bo);

/* Returns malloc'd tokenized variables in BudgetTokens which matches
 * line number line. BudgetTokens catg is separately malloc'd and must 
 * be free'd. Use free_budget_tokens().
 * Returns NULL on failure, pointer to _budget_tokens_t on success. */
_budget_tokens_t *tokenize_budget_line(int line);

/* Frees the strings inside of 'ld', if they are not NULL */
void free_tokenized_record_strings(_transact_tokens_t *ld);

/* Populates ld members with tokens from fpi b */
int tokenize_record_fpi(long b, _transact_tokens_t *ld);

/* Populates ld members with tokens from str */
void tokenize_record(_transact_tokens_t *ld, char **str);

/* Returns the amount value in RECORD_DIR from file position b */
double get_record_amount(long b);

/* Returns an integer value of a given line number line and of field number 
 * field. Field numbers start at 1 */
int get_int_field(int line, int field);

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
 * Also changed return type to be a _vector_t. Removes the local variable to keep
 * track of capacity, it is now apart of the struct.
 */
_vector_t *index_csv(FILE *fptr);

#endif
