/*
 * Includes parsing functions for CSV files used in termBudget
 */
#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"

struct DateMDY {
	unsigned int m;
	unsigned int d;
	unsigned int y;
};

struct BudgetTokens {
	unsigned int m;
	unsigned int y;
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
extern unsigned int get_num_fields(FILE *fptr);

/* Frees the struct and any applicable members */
extern void free_budget_tokens(struct BudgetTokens *pbt);

/* Returns number of categories and the string literal categories of given
 * month and year in BUDGET_DIR(main.h) */
extern struct Categories *get_budget_catg_by_date(int month, int year);

/* Returns malloc'd tokenized variables in BudgetTokens. 
 * BudgetTokens catg is separately malloc'd and must be free'd */
extern struct BudgetTokens *tokenize_budget_line(int line);

/* Returns month and year of line from budget csv */
extern struct DateMY *get_date_my(int line);

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

/* Returns byte offset values for the beginning of each line in fptr file
 * position indicator position will be retained */
//extern struct FlexArr *index_csv_flexarr(FILE *fptr);

#endif
