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
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "main.h"
#include "tui.h"
#include "filemanagement.h"
#include "vector.h"

struct record_search_for {
	bool date;
	bool cat;
	bool desc;
	bool tt;
	bool amt;
};

int get_total_file_lines(FILE *fptr)
{
	int lines = 0;
	char buff[LINE_BUFFER];
	while (fgets(buff, sizeof(buff), fptr) != NULL) {
		lines++;
	}
	return lines;
}

unsigned int boff_to_linenum(long b)
{
	FILE *fptr = open_record_csv("r");
	char linebuff[LINE_BUFFER] = { 0 }; 
	int linenum = 0;
	
	while (fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		linenum++;
		if (ftell(fptr) == b) {
			break;
		}
	}

	fclose(fptr);
	
	return linenum;
}

unsigned int boff_to_linenum_budget(long b)
{
	FILE *fptr = open_budget_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	int linenum = 0;
	
	while (fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		linenum++;
		if (ftell(fptr) == b) {
			break;
		}
	}

	fclose(fptr);
	
	return linenum;
}

void seek_n_fields(char **line, int n)
{
	for (int i = 0; i < n; i++) {
		strsep(line, ",");
	}
}

static void init_record_header_struct(struct record_header *rh)
{
	rh->month = -1;
	rh->day = -1;
	rh->year = -1;
	rh->catg = -1;
	rh->desc = -1;
	rh->transtype = -1;
	rh->value = -1;
	rh->n_fields = -1;
}

struct record_header *parse_record_header(FILE *fptr)
{
	struct record_header *rh = malloc(sizeof(*rh));
	if (rh == NULL) {
		mem_alloc_fail();
	}
	init_record_header_struct(rh);
	rewind(fptr);

	char *fields[] = {
		"month",
		"day",
		"year",
		"category",
		"description",
		"transtype",
		"value"
	};

	int n_fields = (int)sizeof(fields) / (int)sizeof(char *);
	rh->n_fields = n_fields;
	char linebuffer[LINE_BUFFER] = { 0 };
	char *header = fgets(linebuffer, sizeof(linebuffer), fptr);
	char *token;
	int cmp;

	for (int i = 0; i < n_fields; i++) {
		token = strsep(&header, ",");
		if (token == NULL) {
			// end of string is reached
			goto end_of_str;
		}
		token[strcspn(token, "\n")] = '\0';

		for (int j = 0; j < n_fields; j++) {
			cmp = strncmp(token, fields[j], LINE_BUFFER);
			if (cmp == 0) {
				switch (j) {
				case 0:
					rh->month = i;
					break;
				case 1:
					rh->day = i;
					break;
				case 2:
					rh->year = i;
					break;
				case 3:
					rh->catg = i;
					break;
				case 4:
					rh->desc = i;
					break;
				case 5:
					rh->transtype = i;
					break;
				case 6:
					rh->value = i;
					break;
				default:
					break;
				}
			}
		}
	}
end_of_str:
	return rh;
}

static void init_budget_header_struct(struct budget_header *bh)
{
	bh->month = -1;
	bh->year = -1;
	bh->catg = -1;
	bh->transtype = -1;
	bh->value = -1;
	bh->n_fields = -1;
}

struct budget_header *parse_budget_header(FILE *fptr)
{
	struct budget_header *bh = malloc(sizeof(*bh));
	if (bh == NULL) {
		mem_alloc_fail();
	}
	init_budget_header_struct(bh);
	rewind(fptr);

	char *fields[] = {"month", "year", "category", "transtype", "value"};
	int n_fields = (int)sizeof(fields) / (int)sizeof(char *);
	bh->n_fields = n_fields;
	char linebuffer[LINE_BUFFER] = { 0 };
	char *header = fgets(linebuffer, sizeof(linebuffer), fptr);
	char *token;
	int cmp;

	for (int i = 0; i < n_fields; i++) {
		token = strsep(&header, ",");
		if (token == NULL) {
			// end of string is reached
			goto end_of_str;
		}
		token[strcspn(token, "\n")] = '\0';

		for (int j = 0; j < n_fields; j++) {
			cmp = strncmp(token, fields[j], LINE_BUFFER);
			if (cmp == 0) {
				switch (j) {
				case 0:
					bh->month = i;
					break;
				case 1:
					bh->year = i;
					break;
				case 2:
					bh->catg = i;
					break;
				case 3:
					bh->transtype = i;
					break;
				case 4:
					bh->value = i;
					break;
				default:
					break;
				}
			}
		}
	}
end_of_str:
	return bh;
}

int seek_beyond_header(FILE *fptr)
{
	if (ftell(fptr) != 0) {
		rewind(fptr);
	}

	int i = 0;
	char c = getc(fptr);
	while (c != '\n' && c != EOF) {
		c = getc(fptr);
		i++;
	}
	if (i == 0) {
		return -1;
	} else {
		return 0;
	}
}

int get_num_fields(FILE *fptr)
{
	int n = 1;
	rewind(fptr);

	char c;
	do {
		c = fgetc(fptr);
		if (c == ',') {
			n++;
		}
	} while (c != '\n' && c != EOF);

	return n;
}

void free_budget_tokens(struct budget_tokens *pbt)
{
	free(pbt->catg);
	free(pbt);
}

/* 
 * Loop through each category in the budget. Returns true or false if the
 * category exists for the given date range 
 */
bool category_exists_in_budget(char *catg, int month, int year)
{
	struct budget_tokens *bt;
	long i = 1;

	while ((bt = tokenize_budget_line(i)) != NULL) {
		if (bt->y == year && bt->m == month && strcasecmp(bt->catg, catg) == 0) {
			free_budget_tokens(bt);
			return true;
		}
		free_budget_tokens(bt);
		i++;
	}
	return false;
}

bool month_or_year_exists(int m, int y)
{
	FILE *bfptr = open_budget_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	bool mo_exists = false;

	if (seek_beyond_header(bfptr) == -1) {
		perror("Unable to read header");
		return false;
	}

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), bfptr);
		if (str == NULL) {
			break;
		}
		mo_exists = false;
		if (atoi(strsep(&str, ",")) == m) {
			mo_exists = true;
		}
		if (atoi(strsep(&str, ",")) == y && mo_exists) {
			fclose(bfptr);
			return true;
		}
	}
	fclose(bfptr);
	return false;
}

/* Faster version of get_expenditures_per_category, but requires a built
 * catg_node linked list */
double get_expenditures_per_category_fast(struct catg_node *node)
{
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	FILE *fptr = open_record_csv("r");
	double total = 0.0;
	size_t i;
	int transtype;

	if (node == NULL) {
		fclose(fptr);
		return 0;
	}

	for (i = 0; i < node->data->size; i++) {
		fseek(fptr, node->data->data[i], SEEK_SET);
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}
		seek_n_fields(&str, 5);
		transtype = atoi(strsep(&str, ","));
		if (transtype == TT_EXPENSE) {
			total -= atof(strsep(&str, ","));
		} else {
			total += atof(strsep(&str, ","));
		}
	}

	fclose(fptr);
	return total;
}

/* Returns all income records subtracted by expense records */
double get_expenditures_per_category(struct budget_tokens *bt)
{
	double total = 0;
	struct vec_d *income = get_records_by_any(bt->m, -1, bt->y, bt->catg, 
									      NULL, TT_INCOME, -1, NULL);
	struct vec_d *expense = get_records_by_any(bt->m, -1, bt->y, bt->catg, 
									      NULL, TT_EXPENSE, -1, NULL);
	for (size_t i = 0; i < income->size; i++) {
		total += get_record_amount(income->data[i]);
	}
	for (size_t i = 0; i < expense->size; i++) {
		total += get_record_amount(expense->data[i]);
	}
	free(expense);
	free(income);
	return total;
}

struct vec_d *get_years_with_data(FILE *fptr, int field)
{
	struct vec_d *pr = vec_d_create();

	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	int prevyear = 0;
	int tempyear;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		free(pr);
		return NULL;
	}

	/* Check first record outside of the loop to check if no records exist.
	 * If none exists, the function returns NULL */
	str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		free(pr);
		return NULL;
	}
	seek_n_fields(&str, field);
	tempyear = atoi(strsep(&str, ","));
	prevyear = tempyear;
	vec_d_append(&pr, tempyear);

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		seek_n_fields(&str, field);
		tempyear = atoi(strsep(&str, ","));
		if (tempyear != prevyear) {
			prevyear = tempyear;
			vec_d_append(&pr, tempyear);
		}
	}

	return pr;
}

static void init_data_array(struct vec_d *vec)
{
	for (size_t i = 0; i < vec->size; i++) {
		vec->data[i] = 0;
	}
}

/* Returns a malloc'd vector containing all months with records in file 'fptr',
 * which match the year 'matchyear'. Pass the parameter 'fields' to tell
 * the function how many CSV fields to skip ahead. This 'fields' passing is
 * temporary and a new function will be added to find which fields to read
 * based on the header. */
struct vec_d *get_months_with_data(FILE *fptr, int matchyear, int field)
{
	int year;
	int month;
	int i = 0;
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	struct vec_d *months = malloc(sizeof(*months) + (sizeof(long) * MONTHS_IN_YEAR));

	if (months == NULL) {
		mem_alloc_fail();
	}

	months->size = MONTHS_IN_YEAR;
	init_data_array(months);


	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		month = atol(strsep(&str, ","));
		seek_n_fields(&str, field);
		year = atol(strsep(&str, ","));
		if (matchyear == year) {
			if (months->data[0] == 0) {
				months->data[0] = month;
			} else if (month != months->data[i]) {
				i++;
				months->data[i] = month;
			}
		} else if (matchyear < year) {
			break;
		}
	}

	return months;
}

struct vec_d *get_matching_line_nums(FILE *fptr, int month, int year)
{
	struct vec_d *pl = vec_d_create();
	long linenumber = 0;
	int line_month, line_year;
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;

	rewind(fptr);
	if (seek_beyond_header(fptr) == -1) {
		perror("Unable to read header");
		free(pl);
		return NULL;
	}

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		line_month = atoi(strsep(&str, ","));
		seek_n_fields(&str, 1);
		line_year = atoi(strsep(&str, ","));
		if (year == line_year && month == line_month) {
			vec_d_append(&pl, linenumber);
		}
		linenumber++;
	}

	return pl;
}

/* For a given month and year, return an array of strings from the category
 * field of the RECORD_DIR csv file. */
struct catg_vec *get_categories(int month, int year)
{
	FILE *fptr = open_record_csv("r");
	char *line;
	char *token;
	char linebuff[LINE_BUFFER] = { 0 };
	struct catg_vec *pc = catg_vec_create();

	seek_beyond_header(fptr);

	while ((line = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (month != atoi(strsep(&line, ","))) {
			goto duplicate_exists;
		}

		seek_n_fields(&line, 1);
		
		if (year != atoi(strsep(&line, ","))) {
			goto duplicate_exists;
		}

		token = strsep(&line, ",");
		token[strcspn(token, "\n")] = '\0';

		if (pc->size != 0) { // Duplicate Check
			for (size_t i = 0; i < pc->size; i++) {
				if (strcasecmp(pc->categories[i], token) == 0) {
					goto duplicate_exists;
				}
			}
		}

		catg_vec_append(&pc, strdup(token));

duplicate_exists:
		memset(linebuff, 0, sizeof(linebuff)); // Reset the Buffer
	}

	fclose(fptr);
	fptr = NULL;
	return pc; // Struct and each index of categories must be free'd
}

/* Sets all members of struct search_for to false */
static void set_to_false(struct record_search_for *s)
{
	s->amt = false;
	s->cat = false;
	s->date = false;
	s->desc = false;
	s->tt = false;
}

struct vec_d *get_records_by_yr(int year)
{
	return get_records_by_any(-1, -1, year, NULL, NULL, -1, -1, NULL);
}

struct vec_d *get_records_by_mo_yr(int month, int year)
{
	return get_records_by_any(month, -1, year, NULL, NULL, -1, -1, NULL);
}

struct vec_d *get_records_by_any(int month,
								 int day,
								 int year,
								 char *category,
								 char *description, 
								 int transtype,
								 double amount,
								 struct vec_d *chunk) 
{
	struct vec_d *matching_recs = vec_d_create();
	struct transaction_tokens ttok;
	struct record_search_for found;
	char linebuff[LINE_BUFFER] = { 0 };
	FILE *fptr = open_record_csv("r");
	char *str;
	long tmpbo;
	size_t i = 0;

	seek_beyond_header(fptr);

	if (chunk == NULL) {
		;
	} else if (chunk != NULL && chunk->size > 0) {
		fseek(fptr, chunk->data[i], SEEK_SET);
		i++;
	} else if (chunk->size == 0) {
		fclose(fptr);
		return matching_recs;
	}

	while (1) {
		tmpbo = ftell(fptr);
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}
		tokenize_record(&ttok, &str);
		set_to_false(&found);
		if (year < 0 && month < 0 && day < 0) {
			found.date = true;
		} else if (year > 0 && month > 0 && day > 0) {
			if (year == ttok.year && month == ttok.month && day == ttok.day) {
				found.date = true;
			}
		} else if (year > 0 && month > 0 && day < 0) {
			if (year == ttok.year && month == ttok.month) {
				found.date = true;
			}
		} else if (year > 0 && month < 0 && day < 0) {
			if (year == ttok.year) {
				found.date = true;
			}
		}

		if (category == NULL) {
			found.cat = true;
		} else if (category != NULL) {
			if (strncmp(category, ttok.category, MAX_LEN_CATG) == 0) {
				found.cat = true;
			}
		}

		if (description == NULL) {
			found.desc = true;
		} else if (description != NULL) {
			if (strncmp(description, ttok.desc, MAX_LEN_DESC) == 0) {
				found.desc = true;
			}
		}

		if (transtype < 0) {
			found.tt = true;
		} else if (transtype >= 0) {
			if (transtype == ttok.transtype) {
				found.tt = true;
			}
		}

		if (amount < 0) {
			found.amt = true;
		} else if (amount >= 0) {
			if (amount == ttok.amount) {
				found.amt = true;
			}
		}

		if (found.date && found.cat && found.desc && found.tt && found.amt) {
			vec_d_append(&matching_recs, tmpbo);
		}

		free_tokenized_record_strings(&ttok);

		if (chunk != NULL) {
			if (i < chunk->size) {
				fseek(fptr, chunk->data[i], SEEK_SET);
				i++;
			}
		}
	}

	fclose(fptr);
	return matching_recs;
}

struct catg_vec *get_budget_catg_by_date(int month, int year)
{
	struct catg_vec *pc = catg_vec_create();
	char linebuff[LINE_BUFFER] = { 0 };
	FILE *fptr = open_budget_csv("r");
	char *str;
	char *catg;
	int m;
	int y;

	seek_beyond_header(fptr);

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		m = atoi(strsep(&str, ","));
		y = atoi(strsep(&str, ","));
		catg = strsep(&str, ",");

		if (y == year && m == month) {
			catg_vec_append(&pc, strdup(catg));
		}
	}

	fclose(fptr);
	return pc;
}

struct vec_d *get_budget_catg_by_date_bo(int month, int year)
{
	struct vec_d *pcbo = vec_d_create();
	char linebuff[LINE_BUFFER] = { 0 };
	FILE *fptr = open_budget_csv("r");
	char *str;
	long bo;
	int m;
	int y;

	while (1) {
		bo = ftell(fptr);
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}
		m = atoi(strsep(&str, ","));
		y = atoi(strsep(&str, ","));

		if (y == year && m == month) {
			vec_d_append(&pcbo, bo);
		}
	}

	fclose(fptr);

	return pcbo;
}

/* Populates members of struct budget_token_buff with the data contained in 
 * the string "budget". */
void tokenize_budget_string(struct budget_tokens_buff *tokens, char *budget)
{
	if (budget == NULL || tokens == NULL) {
		return;
	}
	char *tmp;

	tokens->m = atoi(strsep(&budget, ","));
	tokens->y = atoi(strsep(&budget, ","));
	tmp = strsep(&budget, ",");
	memcpy(tokens->catg, tmp, strlen(tmp) + 1); /* Hold null-byte */
	tokens->transtype = atoi(strsep(&budget, ","));
	tokens->amount = atof(strsep(&budget, ","));
}

struct budget_tokens *tokenize_budget_fpi(long bo)
{
	if (bo == 0) {
		return NULL;
	}
	struct budget_tokens *pbt = malloc(sizeof(*pbt));
	if (pbt == NULL) {
		mem_alloc_fail();
	}

	FILE *fptr = open_budget_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	char *tmp;

	fseek(fptr, bo, SEEK_SET);
	str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		free(pbt);
		return NULL;
	}

	pbt->m = atoi(strsep(&str, ","));
	pbt->y = atoi(strsep(&str, ","));
	tmp = strndup(strsep(&str, ","), MAX_LEN_CATG);
	if (tmp == NULL) {
		free(pbt);
		mem_alloc_fail();
	}
	pbt->catg = tmp;
	pbt->transtype = atoi(strsep(&str, ","));
	pbt->amount = atof(strsep(&str, ","));

	fclose(fptr);
	return pbt;
}

struct budget_tokens *tokenize_budget_line(long line)
{
	if (line == 0) {
		return NULL;
	}
	struct budget_tokens *pbt = malloc(sizeof(struct budget_tokens));
	if (pbt == NULL) {
		mem_alloc_fail();
	}

	FILE *fptr = open_budget_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	char *tmp;
	char *str;
	long i = 1;

	rewind(fptr);
	seek_beyond_header(fptr);

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			free(pbt);
			return NULL;
		} else if (i == line) {
			break;
		}
		i++;
	}

	fclose(fptr);

	if (i > line || i < line) {
		free(pbt);
		return NULL;
	}

	pbt->m = atoi(strsep(&str, ","));
	pbt->y = atoi(strsep(&str, ","));
	tmp = strndup(strsep(&str, ","), MAX_LEN_CATG);
	if (tmp == NULL) {
		free(pbt);
		mem_alloc_fail();
	}
	pbt->catg = tmp;
	pbt->transtype = atoi(strsep(&str, ","));
	pbt->amount = atof(strsep(&str, ","));

	return pbt;
}

void free_tokenized_record_strings(struct transaction_tokens *ld)
{
	if (ld->category != NULL) {
		free(ld->category);
		ld->category = NULL;
	}
	if (ld->desc != NULL) {
		free(ld->desc);
		ld->desc = NULL;
	}
}

int tokenize_record_fpi(long b, struct transaction_tokens *ld)
{
	FILE *fptr = open_record_csv("r");
	char *str;
	char linebuff[LINE_BUFFER] = { 0 };

	fseek(fptr, b, SEEK_SET);
	str = fgets(linebuff, sizeof(linebuff), fptr);
	fclose(fptr);
	if (str == NULL) {
		return -1;
	}

	tokenize_record(ld, &str);
	return 0;
}

void tokenize_record(struct transaction_tokens *ld, char **str)
{
	char *token;
	for (size_t i = 0; i < CSV_FIELDS; i++) {
		token = strsep(str, ",");
		if (token == NULL) break;
		switch (i) {
		case 0:
			ld->month = atoi(token);
			break;
		case 1:
			ld->day = atoi(token);
			break;
		case 2:
			ld->year = atoi(token);
			break;
		case 3:
			token[strcspn(token, "\n")] = '\0';
			ld->category = strndup(token, strlen(token) + 1);
			break;
		case 4:
			token[strcspn(token, "\n")] = '\0';
			ld->desc = strndup(token, strlen(token) + 1);
			break;
		case 5:
			ld->transtype = atoi(token);
			break;
		case 6:
			ld->amount = atof(token);
			break;
		}
	}
}

double get_record_amount(long b)
{
	FILE *fptr = open_record_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	int transtype;

	fseek(fptr, b, SEEK_SET);
	str = fgets(linebuff, sizeof(linebuff), fptr);
	seek_n_fields(&str, 5);
	fclose(fptr);
	transtype = atoi(strsep(&str, ","));
	if (transtype == 0) {
		return -(atof(strsep(&str, ",")));
	} else {
		return atof(strsep(&str, ","));
	}
}

int get_int_field(int line, int field)
{
	FILE *fptr = open_record_csv("r");
	char linebuff[LINE_BUFFER] = { 0 };
	char *str;
	int i = 0;

	if (field > get_num_fields(fptr) || field < 1) {
		perror("That field doesn't exist");
		fclose(fptr);
		return -1;
	}

	rewind(fptr);

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (i == line) {
			break;
		}
		i++;
	}

	if (i > line || i < line) {
		return -1;
	}

	fclose(fptr);

	seek_n_fields(&str, field - 1);

	return atoi(strsep(&str, ","));
}

struct vec_d *index_csv(FILE *fptr)
{
	struct vec_d *pidx = vec_d_create();
	char linebuff[LINE_BUFFER] = { 0 };

	while (fgets(linebuff, sizeof(linebuff), fptr) != NULL) {
		vec_d_append(&pidx, ftell(fptr));
	}
	return pidx;
}
