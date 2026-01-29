/*
 * Includes parsing functions for CSV files used in termBudget
 */
#ifndef PARSER_H
#define PARSER_H
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

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

/* Returns line numbers from BUDGET_DIR that match month, year */
extern struct FlexArr *get_buget_catg_by_date(int month, int year);

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

/* Returns byte offset values for the beginning of each line in fptr file
 * position indicator position will be retained */
struct FlexArr *index_csv(FILE *fptr);

#endif
