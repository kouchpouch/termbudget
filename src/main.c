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
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <ncurses.h>
#include <limits.h>

#include "main.h"
#include "edit_categories.h"
#include "get_date.h"
#include "fileintegrity.h"
#include "filemanagement.h"
#include "helper.h"
#include "sorter.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "tui_sidebar.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "convert_csv.h"
#include "flags.h"

// 'R'ead 'RET'urn values
#define RRET_DEFAULT 0
#define RRET_BYDATE 1
#define RRET_QUIT 2

// GLOBAL FLAGS. Defined in flags.h, initialized here in main.c
int debug_flag;
int cli_flag;
int verify_flag;

struct ReadRet {
	int flag;
	int yr;
	int mo;
	int sort;
};

enum EditRecordFields {
	NO_SEL,
	E_DATE,
	E_CATG,
	E_DESC,
	E_TYPE,
	E_AMNT,
	DELETE,
};

enum SortBy {
	SORT_DATE = 0,
	SORT_CATG = 1
} sortby;

const char *abbr_months[] = {
	"JAN", 
	"FEB", 
	"MAR", 
	"APR",
	"MAY",
	"JUN",
	"JUL",
	"AUG",
	"SEP",
	"OCT",
	"NOV",
	"DEC"
};

struct SelRecord {
	unsigned int flag;
	long index;
	long opt; // Optional flag
};

void free_categories(struct Categories *pc);
Vec *get_months_with_data(FILE *fptr, int matchyear, int field);
Vec *get_years_with_data(FILE *fptr, int field);
void memory_allocate_fail(void);
int mv_tmp_to_budget_file(FILE *tmp, FILE* main);
void nc_read_setup_default(struct ReadRet *rret);
void calculate_balance(struct Balances *pb, Vec *pbo);
void nc_read_setup(int sel_year, int sel_month, int sort, struct ReadRet *rret);
bool nc_confirm_record(struct LineData *ld);
void nc_print_record_hr(WINDOW *wptr, struct ColWidth *cw, struct LineData *ld, int y);
void nc_print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off);
struct Categories *list_categories(int month, int year);
int mv_tmp_to_record_file(FILE *tempfile, FILE *mainfile);
int delete_csv_record(int linetodelete);
Vec *get_matching_line_nums(FILE *fptr, int month, int year);
char *nc_add_budget_category(int yr, int mo);
void draw_parent_box_with_sidebar(WINDOW *wptr);
void draw_scroll_indicator(WINDOW *wptr);
void draw_read_window_borders_and_text(struct ReadWins *wins, struct ScrollCursor *sc, Vec *psc);

char *user_input(int n) {
	size_t buffersize = n + 1;
	char *buffer = (char *)malloc(buffersize);

	if (buffer == NULL) {
		memory_allocate_fail();
	}
	if (fgets(buffer, buffersize, stdin) == NULL) {
		printf("Invalid Input\n");
		goto err_fail;
	}

	int length = strnlen(buffer, buffersize);

	if (buffer[length - 1] != '\n' && buffer[length - 1] != '\0') {
		printf("Input is too long, try again\n");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		goto err_fail;
	}
	if (length < MIN_INPUT_CHAR) {
		puts("Input is too short");
		goto err_fail;
	}
	if (strstr(buffer, ",")) {
		puts("No commas allowed, we're using a CSV, after all!");
		goto err_fail;
	}
	return buffer; // Must be free'd

err_fail:
	free(buffer);
	buffer = NULL;
	return buffer;
}

void free_user_input(struct UserInput *pui) {
	if (pui->str != NULL) {
		free(pui->str);
	}
	free(pui);
}

int input_n_digits(size_t max_len, size_t min_len) {
	char *str = user_input(max_len + 1);

	while (str == NULL) {
		str = user_input(max_len + 1);
	}

	if (strlen(str) <= min_len) {
		puts("Input is too short");
		free(str);
		str = NULL;
		return -1;
	}

	for (size_t i = 0; i < strlen(str); i++) {
		if (!isdigit(*(str + i)) && *(str + i) != '\n') {
			printf("Invalid character \"%c\", must be digit\n", *(str + i));
			free(str);
			str = NULL;
			return -1;
		}
	}

	int digits = atoi(str);
	free(str);
	str = NULL;

	return digits;
}

int confirm_input(void) {
	char *confirm = user_input(STDIN_SMALL_BUFF);
	if (confirm == NULL) {
		return -1;
	}

	char c_confirm = (char)upper(confirm);
	free(confirm);
	confirm = NULL;

	if (c_confirm == 0) {
		return -1;
	}
	if (c_confirm == 'Y') {
		return 1;
	} else if (c_confirm == 'N') {
		return 0;
	}
	return -1;
}

int input_month(void) {
	int month;
	puts("Enter Month");
	while ((month = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Enter a Vaid Month");
	}
	return month;
}

int input_year(void) {
	int year;
	if (cli_flag) {
		puts("Enter Year");
	}
	while ((year = input_n_digits(MAX_LEN_YEAR, MIN_LEN_YEAR)) == -1);
	return year;
}

int input_day(int month, int year) {
	int day;
	puts("Enter Day");

	while ((day = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1 ||
			dayexists(day, month, year) == false) {
		if (dayexists(day, month, year) == false) { 
			puts("Invalid Day");
		}
	}
	return day;
}

int input_transaction_type(void) {
	int t;

	/* 0 is an expense and 1 is income in the CSV */
	puts("Enter 1 or 2");
	puts("1. Expense");
	puts("2. Income");

	while ((t = input_n_digits(1, 1)) == -1 || (t != 1 && t != 2)) {
		puts("Invalid");
	}
	return t - 1; // sub 1 to convert human readable to CSV format
}

double input_amount(void) {
	puts("$ Amount:");
	char *str = user_input(MAX_LEN_AMOUNT);
	while (str == NULL) {
		puts("Invalid");
		str = user_input(MAX_LEN_AMOUNT);
	}
	double amount = atof(str);
	free(str);
	str = NULL;
	return amount;
}

/* 
 * Takes a user's input and displays msg, on failure to read the user's
 * input the user's input is read again. The newline character is removed 
 */
char *input_str_retry(char *msg) {
	puts(msg);
	int len;
	char *str = user_input(STDIN_LARGE_BUFF);	
	while (str == NULL) {
		str = user_input(STDIN_LARGE_BUFF);
	}
	len = strlen(str);
	if (str[len - 1] == '\n') {
		str[len - 1] = 0;
	}
	return str;
}

/* Returns a malloc'd char * */
char *input_category(int month, int year) {
	char *str;
	bool cat_exists = false;
	struct Categories *pc = list_categories(month, year);

	if (pc->size > 0) {
		puts("Categories:");
		for (size_t i = 0; i < pc->size; i++) {
			printf("%s ", pc->categories[i]);
		}
		printf("\n");
	} else {
		puts("No categories exist for this month");
	}

retry_input:
	cat_exists = false;
	str = input_str_retry("Enter Category:");

	for (size_t i = 0; i < pc->size; i++) {
		if (strcasecmp(str, pc->categories[i]) == 0) {
			cat_exists = true;
			break;
		}
	}

	if (cat_exists != true) {
		puts("That category doesn't exist for this month, create it? [y/n]");
		if (confirm_input() != 1) {
			free(str);
			str = NULL;
			goto retry_input;
		}
	}

	free_categories(pc);
	return str;
}

char *nc_select_category(int month, int year) {
	struct Categories *pc = get_budget_catg_by_date(month, year);
	WINDOW *wptr_parent = create_category_select_parent(pc->size);
	WINDOW *wptr = create_category_select_subwindow(wptr_parent);
	int sz;

	if (debug_flag) {
		curs_set(1);
	}

	if (pc->size == 0) {
		goto manual_selection;
	}

	if (pc->size > INT_MAX) {
		return NULL;
	} else {
		sz = (int)pc->size;
	}

	int displayed = 0;
	/* Print intital data based on window size */
	for (int i = 0; i < getmaxy(wptr) && i < sz; i++) {
		mvwxcprintw(wptr, i, pc->categories[i]);
		displayed++;
	}

	if (sz > displayed) {
		draw_scroll_indicator(wptr_parent);
	}

	wrefresh(wptr);
	mvwchgat(wptr, 0, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
	int cur = 0; // Y=0 is the box and title, datalines start at 1.
	int selection_idx = 0;
	int c = 0;

	int max_y = getmaxy(wptr);
	keypad(wptr, true);

	while (c != '\n' && c != '\r') {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (selection_idx + 1 < sz) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur++;
				selection_idx++;

				if (displayed < sz && cur == max_y) {
					wmove(wptr, 0, 0);
					wdeleteln(wptr);
					mvwxcprintw(wptr, max_y - 1, pc->categories[selection_idx]);
					cur = max_y - 1;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case('k'):
		case(KEY_UP):
			if (selection_idx - 1 >= 0) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur--;
				selection_idx--;

				if (selection_idx >= 0 && displayed < sz && cur == -1) {
					wmove(wptr, 0, 0);
					winsertln(wptr);
					mvwxcprintw(wptr, 0, pc->categories[selection_idx]);
					cur = 0;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case('c'):
manual_selection:
			free_categories(pc);
			nc_exit_window(wptr_parent);
			nc_exit_window(wptr);
			nc_print_input_footer(stdscr);
			return nc_add_budget_category(year, month);
		case('\n'):
		case('\r'):
		case(KEY_ENTER):
			break;
		case('q'):
		case(KEY_F(QUIT)):
			goto cleanup;
		}
	}

	if (selection_idx >= 0) {
		char *tmp = strdup(pc->categories[selection_idx]); // Must be free'd
		free_categories(pc);
		nc_exit_window(wptr_parent);
		nc_exit_window(wptr);
		return tmp; // Will return NULL if stdup failed, callee checks
	}

cleanup:
	free_categories(pc);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr);
	return NULL;
}

//---------------------------------------------------------------------------//
//----------------------------USER INPUT ABOVE-------------------------------//
//---------------------------------------------------------------------------//

void memory_allocate_fail(void) {
	perror("Failed to allocate memory");
	exit(1);
}

void print_record_vert(struct LineData *ld) {
	printf(
		"1.) Date-->  %d/%d/%d\n"
		"2.) Cat.-->  %s\n"
		"3.) Desc-->  %s\n"
		"4.) Type-->  %s\n"
		"5.) Amt.-->  $%.2f\n",
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype == 0 ? "Expense" : "Income", 
		ld->amount
	);
}

void print_record_hr(struct LineData *ld) {
	printf(
		"%d.) %d/%d/%d Category: %s Description: %s, %s, $%.2f\n",
		ld->linenum, 
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype == 0 ? "Expense" : "Income", 
		ld->amount
	 );
}

void replace_category(struct BudgetTokens *bt, long b) {
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();
	char *str;
	char linebuff[LINE_BUFFER];
	size_t line = boff_to_linenum_budget(b) + 2;
	size_t linenum = 0;

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), bfptr);
		if (str == NULL) {
			break;
		}
		linenum++;	
		if (linenum != line) {
			fputs(str, tmpfptr);
		} else if (linenum == line) {
			fprintf(tmpfptr, "%d,%d,%s,%d,%.2f\n", 
				bt->m, 
				bt->y, 
				bt->catg, 
				bt->transtype,
				bt->amount);
		}
	}

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

/*
 * For a given month and year, return an array of strings from the category
 * field of the RECORD_DIR csv file.
 */
struct Categories *list_categories(int month, int year) {
	FILE *fptr = open_record_csv("r");
	char *line;
	char *token;
	char linebuff[LINE_BUFFER];
	struct Categories *pc = malloc(sizeof(*pc) + (sizeof(char *) * REALLOC_INCR));
	pc->size = 0;
	pc->capacity = REALLOC_INCR;

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

		if (pc->size >= pc->capacity) {
			pc->capacity += REALLOC_INCR;
			struct Categories *temp = realloc(pc, sizeof(struct Categories) + 
										((pc->capacity) * sizeof(char *)));
			if (temp == NULL) {
				memory_allocate_fail();
			}
			pc = temp;
		}

		pc->categories[pc->size] = strdup(token);
		pc->size++;

duplicate_exists:
		memset(linebuff, 0, sizeof(linebuff)); // Reset the Buffer
	}

	fclose(fptr);
	fptr = NULL;
	return pc; // Struct and each index of categories must be free'd
}

/*
 * Returns an vector of byte offsets sorted by date. No actual sorting is done
 * in this function, it is assumed the CSV is already sorted. This is basically
 * just moving memory around for the sake of portability and other sorting
 * selections.
 */
Vec *sort_by_date(FILE *fptr, Vec *pidx, Vec *plines)
{
	Vec *psbd = malloc(sizeof(*psbd) + (sizeof(long) * plines->size));

	if (psbd == NULL) {
		memory_allocate_fail();
	}

	psbd->capacity = plines->size;
	psbd->size = 0;
	rewind(fptr);

	for (size_t i = 0; i < plines->size; i++) {
		psbd->data[i] = pidx->data[plines->data[i]];
		psbd->size++;
	}

	return psbd;
}

/* Returns an array of integers representing the byte offsets of records 
 * sorted by category */
Vec *sort_by_category(FILE *fptr, Vec *pidx, Vec *plines, int yr, int mo)
{
	Vec *prsc = malloc(sizeof(*prsc) + (sizeof(long) * REALLOC_INCR));
	prsc->capacity = REALLOC_INCR;
	prsc->size = 0;

	rewind(fptr);
	struct Categories *pc = list_categories(mo, yr);

	char linebuff[LINE_BUFFER];
	char *line;
	char *token;

	for (size_t i = 0; i < pc->size; i++) { // Iterate categories
		prsc->data[prsc->size] = 0;
		prsc->size++;
		for (size_t j = 0; j < plines->size; j++) { // Iterate records
			/* Check prsc->size + 1 because a zero will be added to the array
			 * to mark the spaces between categories */
			if (prsc->size + 1 >= prsc->capacity) {
				prsc->capacity += REALLOC_INCR;
				Vec *tmp =
					realloc(prsc, sizeof(*prsc) + (prsc->capacity * sizeof(char *)));
				if (tmp == NULL) {
					free(prsc);
					prsc = NULL;
					goto err_null;
				}
				prsc = tmp;
			}

			fseek(fptr, pidx->data[plines->data[j]], SEEK_SET);
			line = fgets(linebuff, sizeof(linebuff), fptr);
			if (line == NULL) {
				free(prsc);
				prsc = NULL;
				goto err_null;
			}

			seek_n_fields(&line, 3);
			token = strsep(&line, ",");
			
			if (token == NULL) {
				free(prsc);
				prsc = NULL;
				goto err_null;
			}
			if (strcasecmp(token, pc->categories[i]) == 0) {
				prsc->data[prsc->size] = pidx->data[plines->data[j]];
				prsc->size++;
			}
		}
	}

err_null:
	free_categories(pc);
	return prsc;
}

void add_budget_category(char *catg, int m, int y, int transtype, double amt) {
	unsigned int linetoadd = sort_budget_csv(m, y);
	FILE *fptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER];
	char *line;
	unsigned int linenum = 0;

	while ((line = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		linenum++;	
		if (linenum != linetoadd) {
			fputs(line, tmpfptr);
		} else if (linenum == linetoadd) {
			fputs(line, tmpfptr);
			fprintf(tmpfptr, "%d,%d,%s,%d,%.2f\n", m, y, catg, transtype, amt);
		}
	}

	mv_tmp_to_budget_file(tmpfptr, fptr);
}

/* 
 * Loop through each category in the budget. Returns true or false if the
 * category exists for the given date range 
 */
bool category_exists_in_budget(char *catg, int month, int year) {
	struct BudgetTokens bt, *pbt = &bt;
	int i = 1;

	while ((pbt = tokenize_budget_line(i)) != NULL) {
		if (pbt->y == year && pbt->m == month && strcasecmp(pbt->catg, catg) == 0) {
			free_budget_tokens(pbt);
			return true;
		}
		free_budget_tokens(pbt);
		i++;
	}
	return false;
}

//void free_categories(struct Categories *pc) {
//	for (size_t i = 0; i < pc->size; i++) {
//		free(pc->categories[i]);
//	}
//	free(pc);
//}

int cmp_catg_and_fix(struct Categories *prc, struct Categories *pbc, 
					 int m, int y) 
{
	int corrected = 0;
	bool cat_exists = false;
	for (size_t i = 0; i < prc->size; i++) {
		cat_exists = false;
		for (size_t j = 0; j < pbc->size; j++) {
			if (strcasecmp(prc->categories[i], pbc->categories[j]) == 0) {
				cat_exists = true;
			}
		}
		if (!cat_exists) {
			add_budget_category(prc->categories[i], m, y, 0, 0.0);
			corrected++;
		}
	}
	return corrected;
}

//FIX Implement
//int remove_category_orphans(void) {
//	FILE *bfptr = open_budget_csv("r");
//	FILE *rfptr = open_record_csv("r");
//
//	return 0;
//}

/*
 * Ensures that if a category exists in RECORD_DIR(main.h)
 * it will also exist in BUDGET_DIR(main.h). BUDGET_DIR is verified against
 * RECORD_DIR, not the other way around. If a category exists in BUDGET_DIR
 * and not RECORD_DIR leading to an orphaned category--this is not checked.
 *
 * Orphaned categories are expected and a normal part of the program that are
 * used for budget planning.
 *
 * Returns a 0 or positive value of records that were corrected successfully.
 * Returns -1 on failure.
 */
int verify_categories_exist_in_budget(void) {
	FILE *rfptr = open_record_csv("r");
	int corrected = 0;

	/* Go through each year and find the matching months, then find the
	 * matching categories, then compare. */

	struct Categories prc_, *prc = &prc_;
	struct Categories pbc_, *pbc = &pbc_;
	Vec *years = get_years_with_data(rfptr, 2);
	if (years == NULL) {
		return 0;
	}

	for (size_t i = 0; i < years->size; i++) {
		rewind(rfptr);
		Vec *months = get_months_with_data(rfptr, years->data[i], 1);
		for (size_t j = 0; j < months->size; j++) {
			prc = list_categories(months->data[j], years->data[i]);
			pbc = get_budget_catg_by_date(months->data[j], years->data[i]);
			corrected += cmp_catg_and_fix(prc, pbc, months->data[j], years->data[i]);
			free_categories(prc);
			free_categories(pbc);
		}
		free(months);
	}

	free(years);
	fclose(rfptr);

	return corrected;
}

/* Adds a record to the CSV on line linetoadd */
void add_csv_record(int linetoadd, struct LineData *ld) {
	if (!category_exists_in_budget(ld->category, ld->month, ld->year)) {
		add_budget_category(ld->category, ld->month, ld->year, ld->transtype, 0.0);
	} 

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER];
	char *line;
	int linenum = 0;

	while ((line = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		linenum++;	
		if (linenum != linetoadd) {
			fputs(line, tmpfptr);
		} else if (linenum == linetoadd) {
			fputs(line, tmpfptr);
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
			ld->month, 
			ld->day, 
			ld->year, 
			ld->category, 
			ld->desc, 
			ld->transtype, 
			ld->amount
		   );
		}
	}
	
	mv_tmp_to_record_file(tmpfptr, fptr);
}

/* Returns true if a duplicate is found, false if not */
bool check_dup_catg(struct Categories *psc, char *catg) {
	for (size_t i = 0; i < psc->size; i++) {
		if (strcasecmp(psc->categories[i], catg) == 0) {
			return true;
		}
	}
	return false;
}

bool nc_confirm_budget_category(char *catg, double amt) {
	WINDOW *wptr = create_input_subwindow();
	int maxy = getmaxy(wptr);
	int maxx = getmaxx(wptr);
	char *msg = "Confirm Category";
	mvwprintw(wptr, 0, (maxx / 2 - strlen(msg) / 2), "%s", msg);
	mvwprintw(wptr, 3, (maxx / 2 - strlen(catg) / 2), "%s", catg);
	mvwprintw(wptr, 4, (maxx / 2 - intlen(amt) / 2) - 2, "$%.2f", amt);
	mvwxcprintw(wptr, maxy - BOX_OFFSET, "(Y)es  /  (N)o");

	bool retval = nc_confirm_input_loop(wptr);

	nc_exit_window(wptr);
	return retval;
}

/* For a la carte budget category creation */
char *nc_add_budget_category(int yr, int mo) {
	if (mo == 0 || yr == 0) {
		yr = nc_input_year();
		if (yr == -1) {
			return NULL;
		}
		mo = nc_input_month();
		if (mo == -1) {
			return NULL;
		}
	}
	char *catg = nc_input_string("Enter Category");
	if (catg == NULL) {
		return NULL;
	}
	int transtype = nc_input_category_type();
	if (transtype < 0) {
		return NULL;
	}

	struct Categories *psc = get_budget_catg_by_date(mo, yr);
	if (check_dup_catg(psc, catg)) {
		nc_message("That Category Already Exists");
		free(catg);
		free_categories(psc);
		return NULL;
	}

	double amt = nc_input_budget_amount();
	if (nc_confirm_budget_category(catg, amt)) {
		add_budget_category(catg, mo, yr, transtype, amt);
	}

	free_categories(psc);
	return catg;
}

struct Datevals *nc_create_new_budget(void) {
	struct Datevals *dv = malloc(sizeof(struct Datevals));
	if (dv == NULL) {
		memory_allocate_fail();
	}

	dv->year = nc_input_year();
	if (dv->year < 0) {
		free(dv);
		return NULL;
	}
	dv->month = nc_input_month();
	if (dv->month < 0) {
		free(dv);
		return NULL;
	}
	if (month_or_year_exists(dv->month, dv->year)) {
		nc_message("A budget already exists for that month");
		return dv;
	}
	char *catg = nc_add_budget_category(dv->year, dv->month);
	if (catg == NULL) {
		free(dv);
		return NULL;
	}

	free(catg);
	return dv;
}

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
void nc_add_transaction(int year, int month) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	nc_print_input_footer(stdscr);

	year > 0 ? (uld->year = year) : (uld->year = nc_input_year());
	if (uld->year < 0) {
		return;
	}

	month > 0 ? (uld->month = month) : (uld->month = nc_input_month());
	if (uld->month < 0) {
		return;
	}

	uld->day = nc_input_day(uld->month, uld->year);
	if (uld->day < 0) {
		return;
	}

	uld->category = nc_select_category(uld->month, uld->year);
	if (uld->category == NULL) {
		return;
	}

	uld->desc = nc_input_string("Enter Description");
	if (uld->desc == NULL) {
		free(uld->category);
		return;
	}

	uld->transtype = nc_input_transaction_type();
	if (uld->transtype < 0) {
		goto input_quit;
	}

	uld->amount = nc_input_amount();
	if (uld->amount < 0) {
		goto input_quit;
	}

	if (!nc_confirm_record(uld)) {
		goto input_quit;
	}

	unsigned int resultline = sort_record_csv(uld->month, uld->day, uld->year);
	add_csv_record(resultline, uld);

input_quit:
	free(uld->category);
	free(uld->desc);
// 	This was causing the nc_read_setup function to be called twice
//	nc_read_setup(uld->year, uld->month, sort);
}

void nc_add_transaction_default(void) {
	nc_add_transaction(0, 0);
}

void add_transaction(void) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	FILE *fptr = open_record_csv("r");
	Vec *pidx = index_csv(fptr);
	fclose(fptr);

	uld->year = input_year();
	uld->month = input_month();
	uld->day = input_day(uld->month, uld->year);
	uld->category = input_category(uld->month, uld->year);
	if (uld->category == NULL) {
		goto err_category;
	}
	uld->desc = input_str_retry("Description:");
	uld->transtype = input_transaction_type();
	uld->amount = input_amount();

	puts("Verify Data is Correct:");
	print_record_vert(uld);
	printf("Y/N:  ");

	int result = confirm_input();
	if (result != 1) {
		goto err_confirm;
	}

	unsigned int resultline = sort_record_csv(uld->month, uld->day, uld->year);

	if (debug_flag) {
		printf("Result line: %d\n", resultline);
	}

	add_csv_record(resultline, uld);

err_confirm:
	free(uld->category);
	free(uld->desc);
err_category:
	free(pidx);
}

bool nc_confirm_record(struct LineData *ld) {
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 0, "Confirm Record");
	nc_print_record_vert(wptr, ld, BOX_OFFSET);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		switch(c) {
		case('y'):
		case('Y'):
			nc_exit_window(wptr);
			return true;
		case('n'):
		case('N'):
		case(KEY_F(QUIT)):
		case('q'):
		case('Q'):
			nc_exit_window(wptr);
			return false;
		default:
			break;
		}
	}

	nc_exit_window_key(wptr);
	return false;
}

int nc_edit_csv_record(int replaceln, int field, struct LineData *ld) {
	if (replaceln == 0) {
		puts("Cannot delete line 0");
		return -1;
	}

	replaceln += 1;

	char linebuff[LINE_BUFFER];
	char *line;
	int linenum = 0;

	switch(field) {
	case 1:
		ld->year = nc_input_year();
		if (ld->year < 0) {
			goto err_fail;
		}
		ld->month = nc_input_month();
		if (ld->month < 0) {
			goto err_fail;
		}
		ld->day = nc_input_day(ld->month, ld->year);
		if (ld->day < 0) {
			goto err_fail;
		}
		if (!nc_confirm_record(ld)) {
			goto err_fail;
		}

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		delete_csv_record(replaceln - 1);
		add_csv_record(sort_record_csv(ld->month, ld->day, ld->year), ld);
		return 0;

	case 2:
		ld->category = nc_select_category(ld->month, ld->year);
		if (ld->category == NULL) {
			goto err_fail;
		}
		break;
	case 3:
		ld->desc = nc_input_string("Enter Description");
		if (ld->desc == NULL) {
			goto err_fail;
		}
		break;
	case 4:
		ld->transtype = nc_input_transaction_type();
		if (ld->transtype < 0) {
			goto err_fail;
		}
		break;
	case 5:
		ld->amount = nc_input_amount();
		if (ld->amount < 0) {
			goto err_fail;
		}
		break;
	default:
		puts("Not a valid choice, exiting");
		return -1;
	}

	if (!nc_confirm_record(ld)) {
		if (field == 2) {
			free(ld->category);
			ld->category = NULL;
		}
		if (field == 3) {
			free(ld->desc);
			ld->desc = NULL;
		}
		goto err_fail;
	}

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);

		if (line == NULL) {
			break;
		}

		linenum++;	
		if (linenum != replaceln) {
			fputs(line, tmpfptr);
		} else if (linenum == replaceln) {
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
			ld->month, 
			ld->day, 
			ld->year, 
			ld->category, 
			ld->desc, 
			ld->transtype, 
			ld->amount
		   );
		}
	} while (line != NULL);

	/* mv_tmp_to_record_file() closes the file pointers */
	mv_tmp_to_record_file(tmpfptr, fptr);

	if (field == 2) {
		free(ld->category);
		ld->category = NULL;
	}
	if (field == 3) {
		free(ld->desc);
		ld->desc = NULL;
	}

	return 0;

err_fail:
	return -1;
}

int edit_csv_record(int linetoreplace, struct LineData *ld, int field) {
	if (linetoreplace == 0) {
		puts("Cannot delete line 0");
		return -1;
	}

	linetoreplace += 1;

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;

	switch(field) {
	case 1:
		ld->year = input_year();
		ld->month = input_month();
		ld->day = input_day(ld->month, ld->year);

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		delete_csv_record(linetoreplace - 1);
		add_csv_record(sort_record_csv(ld->month, ld->day, ld->year), ld);
		return 0;

	case 2:
		ld->category = input_category(ld->month, ld->year);
		break;
	case 3:
		ld->desc = input_str_retry("Enter Description");	
		break;
	case 4:
		ld->transtype = input_transaction_type();
		break;
	case 5:
		ld->amount = input_amount();
		break;
	default:
		puts("Not a valid choice, exiting");
		return -1;
	}

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);

		if (line == NULL) {
			break;
		}

		linenum++;	
		if (linenum != linetoreplace) {
			fputs(line, tmpfptr);
		} else if (linenum == linetoreplace) {
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
			ld->month, 
			ld->day, 
			ld->year, 
			ld->category, 
			ld->desc, 
			ld->transtype, 
			ld->amount
		   );
		}
	} while (line != NULL);

	/* mv_tmp_to_record_file() closes the file pointers */
	mv_tmp_to_record_file(tmpfptr, fptr);

	if (field == 2) free(ld->category);
	if (field == 3) free(ld->desc);
	return 0;
}

bool duplicate_vector_data(Vec *vec, long y) {
	for (size_t i = 0; i < vec->size; i++) {
		if (vec->data[i] == y) {
			return true;
		}
	}
	return false;
}

void combine_dedup_vectors(Vec *vec1, Vec *vec2, Vec *result) {
	size_t tmp1;
	size_t tmp2;
	size_t max = max_val(vec1->size, vec2->size);

	for (size_t i = 0; i < max; i++) {
		if (i < vec2->size) {
			tmp1 = vec2->data[i];
		} else {
			tmp1 = 0;
		}

		if (i < vec1->size) {
			tmp2 = vec1->data[i];
		} else {
			tmp2 = 0;
		}

		if (tmp1 > 0 && tmp2 > 0) {
			if (tmp1 == tmp2) {
				result->data[result->size] = tmp1;
				result->size++;
			} else {
				if (!duplicate_vector_data(result, tmp1)) {
					result->data[result->size] = tmp1;
					result->size++;
				}
				if (!duplicate_vector_data(result, tmp2)) {
					result->data[result->size] = tmp2;
					result->size++;
				}
			}

		} else if (tmp1 > 0 && tmp2 == 0) {
			if (!duplicate_vector_data(result, tmp1)) {
				result->data[result->size] = tmp1;
				result->size++;
			}
		} else if (tmp2 > 0 && tmp1 == 0) {
			if (!duplicate_vector_data(result, tmp2)) {
				result->data[result->size] = tmp2;
				result->size++;
			}
		}
	}
}

/* Returns a Vec of deduplicated data from record_years and budget_years */
Vec *consolidate_year_vectors(Vec *vec1, Vec *vec2) {
	Vec *result = malloc(sizeof(*result) + (sizeof(long) * vec1->size) +
				  	 sizeof(long) * vec2->size);
	if (result == NULL) {
		free(vec1);
		free(vec2);
		memory_allocate_fail();
	}

	result->size = 0;
	result->capacity = vec1->size + vec2->size;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

Vec *get_years_with_data(FILE *fptr, int field) {
	Vec *pr = malloc(sizeof(*pr) + sizeof(long) * REALLOC_INCR);
	if (pr == NULL) {
		memory_allocate_fail();
	}

	pr->size = 0;
	pr->capacity = REALLOC_INCR;

	char linebuff[LINE_BUFFER];
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
	pr->data[pr->size] = tempyear;
	pr->size++;

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (pr->size >= pr->capacity) {
			pr->capacity += REALLOC_INCR;
			Vec *tmp = realloc(pr, sizeof(*pr) + (sizeof(long) * pr->capacity));
			if (tmp == NULL) {
				free(pr);
				memory_allocate_fail();
			}
			pr = tmp;
		}
		seek_n_fields(&str, field);
		tempyear = atoi(strsep(&str, ","));
		if (tempyear != prevyear) {
			prevyear = tempyear;
			pr->data[pr->size] = tempyear;
			pr->size++;
		}
	}

	return pr;
}

void init_data_array(Vec *vec) {
	for (size_t i = 0; i < vec->size; i++) {
		vec->data[i] = 0;
	}
}

/*
 * Returns a malloc'd vector containing all months with records in file 'fptr',
 * which match the year 'matchyear'. Pass the parameter 'fields' to tell
 * the function how many CSV fields to skip ahead. This 'fields' passing is
 * temporary and a new function will be added to find which fields to read
 * based on the header.
 */
Vec *get_months_with_data(FILE *fptr, int matchyear, int field) {
	char linebuff[LINE_BUFFER];
	char *str;
	Vec *months = malloc(sizeof(*months) + (sizeof(long) * MONTHS_IN_YEAR));

	if (months == NULL) {
		memory_allocate_fail();
	}

	months->size = MONTHS_IN_YEAR;
	init_data_array(months);

	int year;
	int month;
	int i = 0;

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

/* Returns the row on the bottom 4th of wptr */
int last_quarter_row(WINDOW *wptr) {
	return getmaxy(wptr) - getmaxy(wptr) / 4;
}

int first_quarter_row(WINDOW *wptr) {
	return getmaxy(wptr) / 4;
}

double get_max_value(int elements, double *arr) {
	double tmp = 0.0;
	double max = 0.0;

	for (int i = 0; i < elements; i++) {
		tmp = arr[i];
		if (tmp > max) {
			max = tmp;	
		}
	}

	return max;
}

void nc_print_overview_graphs(WINDOW *wptr, Vec *months, int year) {
	double ratios[12] = {0.0}; // Holds each month's income/expense ratio
	double maxvals[12] = {0.0};
	struct Balances pb_, *pb = &pb_;
	int space = calculate_overview_columns(wptr);
	int mo = 1;

	enum GraphRatios {
		NO_INCOME = -1,
		NO_EXPENSE = -2,
	};

	for (size_t i = 0; i < months->size && mo <= 12; i++, mo++) {
		if (months->data[i] == mo) {
			Vec *pbo = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(pb, pbo);
			if (pb->income == 0) { // Prevent a div by zero
				ratios[mo - 1] = NO_INCOME;
			} else if (pb->expense == 0) {
				ratios[mo - 1] = NO_EXPENSE;
			} else {
				ratios[mo - 1] = pb->expense / pb->income;
			}
			maxvals[mo - 1] = pb->expense >= pb->income ? pb->expense : pb->income;
			free(pbo);
		} else {
			i--;
		}
	}

	int bar_width = 3;
	int cur = (getmaxx(wptr) - space * 11) / 2 - 1;
	double exp_bar_len = 0;
	double inc_bar_len = 0;
	int max_bar_len = (last_quarter_row(wptr) - 2) - first_quarter_row(wptr) + 4;

	double maxval = get_max_value(12, maxvals);

	if (debug_flag) {
		wmove(wptr, 1, 1);
		for (int i = 0; i < 12; i++) {
			wprintw(wptr, "RAT: %.2f VAL: %.2f\n", ratios[i], maxvals[i]);
		}
	}

	for (int i = 0; i < 12; i++) {
		if (maxvals[i] == 0) {
			cur += space;
			continue;
		}

		if (ratios[i] > 1) {
			// Expenses are greater than income
			exp_bar_len = maxvals[i] / maxval;
			exp_bar_len = max_bar_len * exp_bar_len;
			inc_bar_len = exp_bar_len / ratios[i];

		} else if (ratios[i] == NO_INCOME) {
			// There are only expenses
			exp_bar_len = max_bar_len * (maxvals[i] / maxval);
			inc_bar_len = 0;

		} else if (ratios[i] == NO_EXPENSE) {
			inc_bar_len = max_bar_len * (maxvals[i] / maxval);
			exp_bar_len = 0;

		} else {
			// Income is greater than expenses
			inc_bar_len = maxvals[i] / maxval;
			inc_bar_len = max_bar_len * inc_bar_len;
			exp_bar_len = inc_bar_len * ratios[i];
		}

		if (inc_bar_len > 0 && inc_bar_len < 1) {
			inc_bar_len = 1;
		}

		if (exp_bar_len > 0 && exp_bar_len < 1) {
			exp_bar_len = 1;
		}

		for (int j = 0; j < (int)inc_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_GREEN, NULL);
		}

		cur += bar_width;

		for (int j = 0; j < (int)exp_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_RED, NULL);
		}

		cur += space - bar_width;
	}
}

void nc_print_overview_balances(WINDOW *wptr, Vec *months, int year) {
	int tmpx = 0;
	int space = calculate_overview_columns(wptr);
	int y = last_quarter_row(wptr) + 2;
	int cur = (getmaxx(wptr) - space * 11) / 2;
	int mo = 1;
	struct Balances pb_, *pb = &pb_;
	for (size_t i = 0; i < months->size && mo <= 12; i++, mo++) {
		tmpx = 0;

		if (months->data[i] == mo) {
			Vec *pbo = get_records_by_mo_yr(months->data[i], year);
			calculate_balance(pb, pbo);
			tmpx = intlen(pb->income) / 2;
			wmove(wptr, y, cur - tmpx);
			wattron(wptr, COLOR_PAIR(2));
			wprintw(wptr, "+$%.0f", pb->income);
			wattroff(wptr, COLOR_PAIR(2));
			tmpx = intlen(pb->expense) / 2;
			wmove(wptr, y + 1, cur - tmpx);
			wattron(wptr, COLOR_PAIR(1));
			wprintw(wptr, "-$%.0f", pb->expense);
			wattron(wptr, COLOR_PAIR(1));
			free(pbo);
			cur += space;
		} else {
			wmove(wptr, y, cur);
			wattron(wptr, COLOR_PAIR(2));
			wprintw(wptr, "+$0");
			wattroff(wptr, COLOR_PAIR(2));
			wmove(wptr, y + 1, cur);
			wattron(wptr, COLOR_PAIR(1));
			wprintw(wptr, "-$0");
			wattron(wptr, COLOR_PAIR(1));
			cur += space;
			i--;
		}
	}

	wrefresh(wptr);
}

void nc_print_overview_months(WINDOW *wptr) {
	int space = calculate_overview_columns(wptr);
	int y = last_quarter_row(wptr);
	int cur = (getmaxx(wptr) - space * 11) / 2;

	if (debug_flag) {
		wprintw(wptr, "INIT CUR: %d ", cur);
		wprintw(wptr, "SPACE: %d ", space);
		wprintw(wptr, "SPACEx11: %d ", space * 11);
		wprintw(wptr, "MAX X: %d ", getmaxx(wptr));
	}

	for (int i = 0; i < 12; i++) {
		wmove(wptr, y, cur);
		wprintw(wptr, "%s", abbr_months[i]);
		cur += space;
	}
}

unsigned int nc_overview_loop(WINDOW *wptr, Vec *months, int year) {
	unsigned int flag = 0;
	int c;
	int space = calculate_overview_columns(wptr);
	if (space > 0) {
		nc_print_overview_months(wptr);
		nc_print_overview_balances(wptr, months, year);
		nc_print_overview_graphs(wptr, months, year);
		nc_print_quit_footer(stdscr);
		wrefresh(wptr);
	} else {
		wprintw(wptr, "Terminal is too small");
		wrefresh(wptr);
	}

	while (1) {
		c = wgetch(wptr);
		switch(c) {
		case(KEY_RESIZE):
			return RESIZE;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			return QUIT;
		default:
			flag = NO_SELECT;
			break;
		}
	}

	return flag;
}

void nc_overview_setup(int year) {
	WINDOW *wptr_parent = newwin(LINES - 1, 0, 0, 0);
	WINDOW *wptr_data = create_lines_subwindow(getmaxy(wptr_parent) - 1,
									getmaxx(wptr_parent), 1, BOX_OFFSET);
	init_pair(1, COLOR_RED, -1);
	init_pair(2, COLOR_GREEN, -1);
	box(wptr_parent, 0, 0);
	mvwxcprintw_digit(wptr_parent, 0, year);
	wrefresh(wptr_parent);
	
	FILE *fptr = open_record_csv("r");
	Vec *months = get_months_with_data(fptr, year, 1);
	fclose(fptr);

	unsigned int flag = nc_overview_loop(wptr_data, months, year);

	free(months);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr_data);

	switch(flag) {
	case(RESIZE):
		nc_overview_setup(year);
		break;
	case(QUIT):
		break;
	default:
		break;
	}
}

/* Prints a 2 bar graphs showing the difference between income and expense */
void print_bar_graph(double expense, double income) {
	char income_bar[10];
	char expense_bar[10];

	for (size_t i = 0; i < sizeof(income_bar); i++) {
		income_bar[i] = '#';
		expense_bar[i] = '#';
	}

	if (income > expense) {
		double diff = expense / income;
		diff *= 10;
		for (size_t i = 0; i < sizeof(expense_bar); i++) {
			i < diff ? 
			(expense_bar[i] = '#') : (expense_bar[i] = '-');
		}
	} else {
		double diff = income / expense;
		diff *= 10;
		for (size_t i = 0; i < sizeof(income_bar); i++) {
			i < diff ? 
			(income_bar[i] = '#') : (income_bar[i] = '-');
		}
	}

	printf("Income:  $%.2f [", income);
	for (size_t i = 0; i < sizeof(income_bar); i++) {
		printf("%c", income_bar[i]);
	}
	printf("]\n");

	printf("Expense: $%.2f [", expense);
	for (size_t i = 0; i < sizeof(expense_bar); i++) {
		printf("%c", expense_bar[i]);
	}
	printf("]\n");

	printf("Total:   $%.2f\n", income - expense);
}

void legacy_read_csv(void) {
	int useryear;
	int usermonth;
	double income = 0;
	double expense = 0;
	int linenum = 0;
	char linebuff[LINE_BUFFER] = {0};
	FILE *fptr = open_record_csv("r");
	bool month_record_exists = false;
	bool year_record_exists = false;

	struct LineData linedata_, *ld = &linedata_;

	Vec *years = get_years_with_data(fptr, 2);
	rewind(fptr);

	while (year_record_exists == false) {
		useryear = input_year();
		for (size_t i = 0; i < years->size; i++) {
			if (years->data[i] == useryear) {
				year_record_exists = true;
				break;
			}
		}

		if (year_record_exists == false) {
			printf("%s '%d'\n", "No records match the year", useryear);
		}
	}

	free(years);
	years = NULL;
	rewind(fptr);

	Vec *months = get_months_with_data(fptr, useryear, 1);

	while (month_record_exists == false) {
		usermonth = input_month();
		for (size_t i = 0; i < months->size; i++) {
			if (months->data[i] == usermonth) {
				month_record_exists = true;
				break;
			}
		}

		if (month_record_exists == false) {
			printf("%s '%d'\n", "No records match the month", usermonth);
		}
	}

	free(months);
	months = NULL;
	rewind(fptr);

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	char *line;
	while (1) {
		linenum++;
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) {
			break;
		}
		tokenize_record(ld, &line);
		ld->linenum = linenum;
		if (ld->month == usermonth && ld->year == useryear) {
			month_record_exists = true;
			print_record_hr(ld);
			if(ld->transtype == 1) {
				income+=ld->amount;
			} else if (ld->transtype == 0) {
				expense+=ld->amount;
			}
		}
		free_tokenized_record_strings(ld);
	}

	if (month_record_exists) {
		print_bar_graph(expense, income);
	} else {
		printf("No records match the entered date\n");
	}
	fclose(fptr);
	fptr = NULL;
}

Vec *consolidate_month_vectors(Vec *vec1, Vec *vec2) {
	Vec *result = malloc(sizeof(*result) + (sizeof(long) * MONTHS_IN_YEAR));
	if (result == NULL) {
		free(vec1);
		free(vec2);
		memory_allocate_fail();
	}

	result->size = 0;
	result->capacity = MONTHS_IN_YEAR;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

Vec *init_nc_read_select_year(void) {
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	Vec *pr = get_years_with_data(rfptr, 2);
	Vec *pb = get_years_with_data(bfptr, 1);
	if (pr == NULL && pb != NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return pb;
	} else if (pb == NULL && pr != NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return pr;
	} else if (pr == NULL && pb == NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return NULL;
	}

	fclose(rfptr);
	fclose(bfptr);

	Vec *retval = consolidate_year_vectors(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

Vec *init_nc_read_select_month(int year) {
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	Vec *pr = get_months_with_data(rfptr, year, 1);
	Vec *pb = get_months_with_data(bfptr, year, 0);

	fclose(rfptr);
	fclose(bfptr);

	Vec *retval = consolidate_month_vectors(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

/* On a non-select, the return value is the inverted menukeys value */
int nc_read_select_year(WINDOW *wptr) {
	keypad(wptr, true);	

	Vec *years = init_nc_read_select_year();
	if (years == NULL) {
		return -(NO_RCRD);
	}

	int selected_year = 0;
	int print_y = 1;
	int print_x = 2;
	size_t year_index = 0;
	size_t scr_idx = 0;

	box(wptr, 0, 0);
	wrefresh(wptr);
	wmove(wptr, print_y, print_x);

	for (size_t i = 0; i < years->size; i++) {
		if (years->data[i] == get_current_year()) {
			year_index = i;
		}
		wprintw(wptr, "%ld ", years->data[i]);
		wrefresh(wptr);
	}

	/* Initially highlight the current year and move cursor to it */
	int init_rv_x = print_x;
	if (year_index > 0) {
		init_rv_x += (4 * year_index) + year_index;
		scr_idx = year_index;
	}
	
	mvwchgat(wptr, print_y, init_rv_x, 4, A_REVERSE, REVERSE_COLOR, NULL);
	wrefresh(wptr);

	int c = 0;
	int temp_x;
	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		temp_x = getcurx(wptr);
		switch(c) {
		case('h'):
		case(KEY_LEFT):
			if (scr_idx > 0) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x - 5, 4, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				scr_idx--;
			}
			break;
		case('l'):
		case(KEY_RIGHT):
			if (scr_idx + 1 < years->size) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x + 5, 4, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				scr_idx++;
			}
			break;
		case('a'):
		case(KEY_F(ADD)):
			free(years);
			years = NULL;
			return -(ADD);
		case(KEY_RESIZE):
			free(years);
			years = NULL;
			return -(RESIZE);
		case('\n'):
		case('\r'):
			selected_year = years->data[scr_idx];
			free(years);
			years = NULL;
			return selected_year;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			free(years);
			return -(QUIT);
		default: 
			break;
		}
	}

	free(years);
	years = NULL;

	return selected_year;
}

int get_current_mo_idx(Vec *months) {
	int mo = get_current_month();
	// months->size can never be more than MONTHS_IN_YEAR so this cast is
	for (int i = 0; i < (int)months->size; i++) {
		if (months->data[i] - 1 == mo) {
			return i;
		}
	}
	return -1;
}

/* On a non-select, the return value is the inverted menukeys value */
int nc_read_select_month(WINDOW *wptr, int year) {
	Vec *months_data = init_nc_read_select_month(year);

	int selected_month = 0;
	int monlen = strlen(abbr_months[0]);

	int temp_y = 0;
	int scr_idx = 0;
	int cur_idx = 0;
	int current_mo_idx = get_current_mo_idx(months_data);

	wmove(wptr, BOX_OFFSET, BOX_OFFSET);
	for (size_t i = 0; i < months_data->size; i++) {
		temp_y = getcury(wptr);
		if (months_data->data[i] != 0) {
			wmove(wptr, temp_y, BOX_OFFSET);
			wprintw(wptr, "%s\n", abbr_months[months_data->data[i] - 1]);
			scr_idx++;
		}
	}

	if (year == get_current_year() && current_mo_idx != -1) {
		wmove(wptr, BOX_OFFSET + current_mo_idx, BOX_OFFSET);
		cur_idx = current_mo_idx;
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	} else {
		wmove(wptr, BOX_OFFSET, BOX_OFFSET);
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	}

	box(wptr, 0, 0);
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		temp_y = getcury(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (temp_y - BOX_OFFSET + 1 < scr_idx) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y + 1, BOX_OFFSET, monlen, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				cur_idx++;
			}
			break;
		case('k'):
		case(KEY_UP):
			if (temp_y - 1 >= BOX_OFFSET) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y - 1, BOX_OFFSET, monlen, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				cur_idx--;
			}
			break;
		case('a'):
		case(KEY_F(ADD)):
			free(months_data);
			months_data = NULL;
			return -(ADD);
		case(KEY_RESIZE):
			free(months_data);
			months_data = NULL;

			/* This RESIZE macro has to be greater than 12, which I have no
			 * intention of changing. I hate this. However--I'm lazy. Plus,
			 * all of the resizes in this program suck. */
			return -(RESIZE);
		case('\n'):
		case('\r'):
			selected_month = months_data->data[cur_idx];
			free(months_data);
			months_data = NULL;
			return selected_month;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			free(months_data);
			months_data = NULL;
			return -(QUIT);
		default: 
			break;
		}
	}

	free(months_data);
	months_data = NULL;

	return selected_month;
}

Vec *get_matching_line_nums(FILE *fptr, int month, int year) {
	rewind(fptr);
	Vec *pl = malloc(sizeof(*pl) + (sizeof(long) * REALLOC_INCR));
	if (pl == NULL) {
		memory_allocate_fail();
	}

	pl->size = 0;
	pl->capacity = REALLOC_INCR;

	long linenumber = 0;
	int line_month, line_year;
	char linebuff[LINE_BUFFER];
	char *str;

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
			if (pl->size >= pl->capacity) {
				pl->capacity += REALLOC_INCR;
				Vec *tmp = 
					realloc(pl, sizeof(*pl) + (sizeof(long) * pl->capacity));
				if (tmp == NULL) {
					free(pl);
					memory_allocate_fail();
				}
				pl = tmp;
			}
			pl->data[pl->size] = linenumber;
			pl->size++;	
		}
		linenumber++;
	}

	return pl;
}

/* Returns all income records subtracted by expense records */
double get_expenditures_per_category(struct BudgetTokens *bt) {
	double total = 0;
	Vec *pi = get_records_by_any(bt->m, -1, bt->y, bt->catg, NULL, TT_INCOME, -1, NULL);
	Vec *pe = get_records_by_any(bt->m, -1, bt->y, bt->catg, NULL, TT_EXPENSE, -1, NULL);
	for (size_t i = 0; i < pi->size; i++) {
		total += get_record_amount(pi->data[i]);
	}
	for (size_t i = 0; i < pe->size; i++) {
		total += get_record_amount(pe->data[i]);
	}
	free(pe);
	free(pi);
	return total;
}

void print_catg_balances(WINDOW *wptr, int tt, double planned, double exp, int width) {
	// Safe cast, we know these strings aren't greater than INT_MAX
	char *full_inc_string = "Planned: $, Received: $";
	char *full_exp_string = "Planned: $, Remaining: $";
	char *short_inc_string = "Plan: $, Rcvd: $";
	char *short_exp_string = "Plan: $, Rem: $";
	char *abbreviated = "P$, R$";

	int full_inc_len = (int)strlen(full_inc_string);
	int full_exp_len = (int)strlen(full_exp_string);
	int short_inc_len = (int)strlen(short_inc_string);
	int short_exp_len = (int)strlen(short_exp_string);
	int abbreviated_len = (int)strlen(abbreviated);

	double remaining;

	if (exp > 0) {
		remaining = exp - planned;
	} else {
		remaining = planned + exp;
	}

	if (tt == TT_INCOME) {
		if (full_inc_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "Planned: $%.2f, Received: $%.2f", planned, exp);
		} else if (short_inc_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "Plan: $%.2f, Rcvd: $%.2f", planned, exp);
		} else if (abbreviated_len + finlen(planned) + finlen(exp) < width) {
			wprintw(wptr, "P$%.2f, R$%.2f", planned, exp);
		}
	} else if (tt == TT_EXPENSE) {
		if (full_exp_len + finlen(planned) + finlen(remaining) < width) {
			//wprintw(wptr, "Plan: $%.2f, Rem: $%.2f, Exp: $%.2f", planned, remaining, exp);
			wprintw(wptr, "Planned: $%.2f, Remaining: $%.2f", planned, remaining);
		} else if (short_exp_len + finlen(planned) + finlen(remaining) < width) {
			wprintw(wptr, "Plan: $%.2f, Rem: $%.2f", planned, remaining);
		} else if (abbreviated_len + finlen(planned) + finlen(remaining) < width) {
			wprintw(wptr, "P$%.2f, R$%.2f", planned, remaining);
		}
	}
}

/* Prints the value of each struct ColWidth memeber to the window wptr */
void debug_columns(WINDOW *wptr, struct ColWidth *cw) {
	wmove(wptr, 5, 10);
	wprintw(wptr, "DATE: %d CATG: %d, DESC: %d, TRNS: %d, AMNT: %d\n",
	cw->date, cw->catg,	cw->desc, cw->trns, cw->amnt);
	wrefresh(wptr);
	wgetch(wptr);
}

static bool shorten_string(WINDOW *wptr) {
	if (getmaxx(wptr) + BOX_OFFSET < MIN_COLUMNS + SHORTEN_THRESH) {
		return true;
	} else {
		return false;
	}
}

void nc_print_category_hr(WINDOW *wptr, struct ColWidth *cw,
						  struct BudgetTokens *bt, int y)
{
	char *etc = "..";
	int x = 0;
	int print_offset = 0;
	double e = get_expenditures_per_category(bt);
	wattron(wptr, A_REVERSE);

	/* Move cursor past the date columns */
	wmove(wptr, y, x += cw->date - print_offset);
	if ((int)strlen(bt->catg) > cw->catg - (int)strlen(etc)) {
		wprintw(wptr, "%.*s%s", cw->catg - (int)strlen(etc), bt->catg, etc);
	} else {
		wprintw(wptr, "%s", bt->catg);
	}

	/* Move cursor past the category column */
	wmove(wptr, y, x += cw->catg - print_offset);
	print_catg_balances(wptr, bt->transtype, bt->amount, e, cw->desc);

	wmove(wptr, y, x += cw->desc - print_offset);

	if (shorten_string(wptr)) {
		wprintw(wptr, "%s", bt->transtype == 0 ? "-" : "+");
	} else {
		wprintw(wptr, "%s", bt->transtype == 0 ? "Expenses" : "Income");
	}

	wattroff(wptr, A_REVERSE);
}

/* 
 * Prints record from ld, formatting in columns from cw, to a window pointed
 * to by wptr, at a Y-coordinate of y. Truncates desc and category strings
 * if the window is too small.
 */
void nc_print_record_hr(WINDOW *wptr, struct ColWidth *cw, 
						struct LineData *ld, int y)
{
	char *etc = ". ";
	int x = 0;
	bool shorten = shorten_string(wptr);
	wmove(wptr, y, x);
	if (shorten) {
		wprintw(wptr, "%d/%d", ld->month, ld->day);
	} else {
		wprintw(wptr, "%d/%d/%d", ld->month, ld->day, ld->year);
	}

	wmove(wptr, y, x += cw->date);
	if ((int)strlen(ld->category) > cw->catg - (int)strlen(etc)) {
		if (getmaxx(wptr) < MIN_COLUMNS) {
			wprintw(wptr, "%.*s%s", cw->catg - (int)strlen(etc), ld->category, etc);
		} else {
			wprintw(wptr, "%.*s%s", cw->catg - (int)strlen(etc), ld->category, etc);
		}
	} else {
		wprintw(wptr, "%s", ld->category);
	}

	wmove(wptr, y, x += cw->catg);
	if ((int)strlen(ld->desc) > cw->desc - (int)strlen(etc)) {
		wprintw(wptr, "%.*s%s", cw->desc - (int)strlen(etc), ld->desc, etc);
	} else {
		wprintw(wptr, "%s", ld->desc);
	}

	wmove(wptr, y, x += cw->desc);
	if (shorten) {
		wprintw(wptr, "%s", ld->transtype == 0 ? "-" : "+");
	} else {
		wprintw(wptr, "%s", ld->transtype == 0 ? "Expense" : "Income");
	}

	wmove(wptr, y, x += cw->trns);
	wprintw(wptr, "$%.2f", ld->amount);
}

/* Prints record from ld, in vertical format, 5 rows. */
void nc_print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off) {
	mvwprintw(wptr, 1, x_off, "Date--> %d/%d/%d", ld->month, ld->day, ld->year);
	mvwprintw(wptr, 2, x_off, "Cat.--> %s", ld->category);
	mvwprintw(wptr, 3, x_off, "Desc--> %s", ld->desc);
	mvwprintw(wptr, 4, x_off, "Type--> %s", ld->transtype == 0 ? "Expense" : "Income");
	mvwprintw(wptr, 5, x_off, "Amt.--> %.2f", ld->amount);
}

int nc_select_field_to_edit(WINDOW *wptr) {
	mvwchgat(wptr, 1, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
	keypad(wptr, true);
	int select = 1;
	int c = 0;

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Field to Edit");
	wrefresh(wptr);
	while (c != KEY_F(QUIT) && c != 'q') { 
		box(wptr, 0, 0);
		mvwxcprintw(wptr, 0, "Select Field to Edit");
		wrefresh(wptr);
		c = wgetch(wptr);

		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (select + 1 <= (INPUT_WIN_ROWS - BOX_OFFSET)) {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select++;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			} else {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select = 1;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case('k'):
		case(KEY_UP):
			if (select - 1 > 0) {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select--;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			} else {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select = INPUT_WIN_ROWS - BOX_OFFSET;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case('\n'):
		case('\r'):
			return select;
		case('q'):
		case(KEY_F(QUIT)):
			break;
		}
	}
	return 0;
}

void nc_edit_transaction(long b) {
//	struct LineData linedata, *ld = &linedata;
	struct LineData *ld = malloc(sizeof(*ld));
	enum EditRecordFields field;

	if (ld == NULL) {
		memory_allocate_fail();
		return;
	}

	WINDOW *wptr_edit = create_input_subwindow();
	FILE *fptr = open_record_csv("r+");
	fseek(fptr, b, SEEK_SET);
	unsigned int linenum = boff_to_linenum(b);

	char linebuff[LINE_BUFFER];
	char *line = fgets(linebuff, sizeof(linebuff), fptr);

	if (line == NULL) {
		exit(1);
	}

	tokenize_record(ld, &line);

	nc_print_record_vert(wptr_edit, ld, BOX_OFFSET);

	mvwprintw(wptr_edit, INPUT_WIN_ROWS - BOX_OFFSET, BOX_OFFSET, "%s", "Delete");
	box(wptr_edit, 0, 0);
	wrefresh(wptr_edit);

	field = nc_select_field_to_edit(wptr_edit);

	fclose(fptr);
	nc_exit_window(wptr_edit);

	switch(field) {
	case NO_SEL:
		break;
	case E_DATE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum + 1, 1, ld);
		break;
	case E_CATG:
		free(ld->category);
		ld->category = NULL;
		nc_edit_csv_record(linenum + 1, 2, ld);
		break;
	case E_DESC:
		nc_print_input_footer(stdscr);
		free(ld->desc);
		ld->desc = NULL;
		nc_edit_csv_record(linenum + 1, 3, ld);
		break;
	case E_TYPE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum + 1, 4, ld);
		break;
	case E_AMNT:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum + 1, 5, ld);
		break;
	case DELETE:
		nc_print_input_footer(stdscr);
		if (nc_confirm_input("Confirm Delete")) {
			if (delete_csv_record(linenum + 1) == 0) {
				nc_message("Successfully Deleted");
			}
		}
		break;
	default:
		return;
	}

	free_tokenized_record_strings(ld);
	free(ld);
	ld = NULL;
}

void nc_print_debug_line(WINDOW *wptr, struct ScrollCursor *sc) {
	mvwhline(wptr, getmaxy(wptr) - 1, 1, 0, getmaxx(wptr) - 2);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 55, "SELIDX: %d", sc->select_idx);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 40, "DATA: %d", sc->displayed);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 30, "DATA: %d", sc->catg_data);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 20, "NODE: %d", sc->catg_node);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 10, "CURS: %d", sc->cur_y);
	wrefresh(wptr);
}

void calculate_balance(struct Balances *pb, Vec *pbo) {
	FILE *fptr = open_record_csv("r");
	pb->income = 0.0;
	pb->expense = 0.0;
	int type;
	char linebuff[LINE_BUFFER];
	char *line;

	for (size_t i = 0; i < pbo->size; i++) {
		fseek(fptr, pbo->data[i], SEEK_SET);
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) {
			break;
		}
		seek_n_fields(&line, 5);
		type = atoi(strsep(&line, ","));
		if (type == 0) {
			pb->expense += atof(strsep(&line, ","));
		} else {
			pb->income += atof(strsep(&line, ","));
		}
	}
	fclose(fptr);
}

void nc_print_balances_text(WINDOW *wptr, Vec *psc) {
	struct Balances pb_, *pb = &pb_;
	calculate_balance(pb, psc);
	int total_len = intlen(pb->income) + intlen(pb->expense) + strlen("Expenses: $.00 Income: $.00");
	mvwprintw(wptr, 0, getmaxx(wptr) / 2 - total_len / 2, 
		   	  "Income: $%.2f Expenses: $%.2f", pb->income, pb->expense);
	wrefresh(wptr);
}

/*
 * Creates a sub window inside of wptr to display a line-by-line format of
 * the selected record at index i of pidx->data. Following the format style
 * from edit_transaction()
 */
int show_detail_subwindow(char *line) {
	WINDOW *wptr_detail = create_input_subwindow();
	int y = getmaxy(wptr_detail);
	box(wptr_detail, 0, 0);
	mvwxcprintw(wptr_detail, 0, "Details");
	struct LineData linedata_, *ld = &linedata_;
	tokenize_record(ld, &line);
	nc_print_record_vert(wptr_detail, ld, BOX_OFFSET);
	free_tokenized_record_strings(ld);
	nc_exit_window_key(wptr_detail);
	return y;
}

/*
 * After the detail sub window is closed, there is a gap of invisible lines
 * where the window was. This loops through each line and changes it attr
 * back to A_NORMAL and then re-reverse-video's the original line. Cursor
 * position is NOT changed.
 *
 * Refreshes n number of lines.
 */
void refresh_on_detail_close_uniform(WINDOW *wptr, WINDOW *wptr_parent, int n) 
{
	int temp_y, temp_x;
	getyx(wptr, temp_y, temp_x);
	wmove(wptr, 0, 0);

	for (int i = 0; i < n; i++) {
		mvwchgat(wptr, i, 0, -1, A_NORMAL, 0, NULL);
	}

	wmove(wptr, temp_y, temp_x);
	wchgat(wptr, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	mvwvline(wptr_parent, 1, 0, 0, getmaxy(wptr_parent) - 2);
	mvwvline(wptr_parent, 1, getmaxx(wptr_parent) - 1, 0, getmaxy(wptr_parent) - 2);
	wrefresh(wptr_parent);
}

void nc_scroll_prev(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw, 
					bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}

	if (catg) {
		struct BudgetTokens *bt = tokenize_budget_byte_offset(b);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		nc_print_category_hr(wptr, cw, bt, 0);
		free_budget_tokens(bt);
	} else {
		struct LineData linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		winsertln(wptr);
		nc_print_record_hr(wptr, cw, ld, 0);
		free_tokenized_record_strings(ld);
	}
}

void nc_scroll_next(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw,
					bool catg) 
{
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str;
	line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}

	if (catg) {
		struct BudgetTokens *bt = tokenize_budget_byte_offset(b);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		nc_print_category_hr(wptr, cw, bt, getmaxy(wptr) - 1);
		free_budget_tokens(bt);
	} else {
		struct LineData linedata_, *ld = &linedata_;
		tokenize_record(ld, &line_str);
		wmove(wptr, 0, 0);
		wdeleteln(wptr);
		wmove(wptr, getmaxy(wptr) - 1, 0);
		nc_print_record_hr(wptr, cw, ld, getmaxy(wptr) - 1);
		free_tokenized_record_strings(ld);
	}
}

/* Returns 1 if the text was scrolled down, 0 if a normal scroll occured */
int nc_scroll_prev_read_loop
(WINDOW *wptr, struct ScrollCursor *sc, struct ColWidth *cw, FILE *fptr, 
 Vec *psc)
{
	int retval = 0;

	mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
	sc->cur_y--;
	sc->select_idx--;

	if (sc->cur_y < 0) {
		sc->cur_y = -1;
	}
	
	if (sc->select_idx >= 0 && sc->displayed < psc->size && sc->cur_y == -1) {
		nc_scroll_prev(psc->data[sc->select_idx], fptr, wptr, cw, false);
		sc->cur_y = getcury(wptr);
		retval++;
	}
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured */
int nc_scroll_next_read_loop
(WINDOW *wptr, struct ScrollCursor *sc, struct ColWidth *cw, FILE *fptr, 
 Vec *psc)
{
	int retval = 0;

	mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
	sc->cur_y++;
	sc->select_idx++;

	if (sc->displayed < psc->size && sc->cur_y == getmaxy(wptr)) {
		nc_scroll_next(psc->data[sc->select_idx], fptr, wptr, cw, false);
		sc->cur_y = getcury(wptr);
		retval++;
	}
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	return retval;
}

int get_total_nodes(CategoryNode **nodes) {
	int n = 1;
	for (int i = 0; nodes[i]->next != NULL; i++) {
		n++;
	}
	return n;
}

void print_debug_node(WINDOW *wptr, CategoryNode *node) {
	wprintw(wptr, "Data sz: %lu, Next: %p, Prev: %p, FPI: %lu\n", 
		 node->data->size, 
		 (void *)node->next, 
		 (void *)node->prev, 
		 node->catg_fp);
}

void debug_category_nodes(WINDOW *wptr, CategoryNode **nodes) {
	size_t i = 0;
	wmove(wptr, 0, 0);
	while (1) {
		if (nodes[i]->next == NULL) {
			print_debug_node(wptr, nodes[i]);
			break;
		} else {
			print_debug_node(wptr, nodes[i]);
		}
		i++;
	}

	wgetch(wptr);
	wclear(wptr);
}

/*
 * Returns the value of the total number of rows to display in 
 * nc_read_budget_loop() to handle scrolling.
 *
 * The return value is calculated by adding up add CategoryNodes and their
 * data member's size.
 */
int get_total_displayed_rows(CategoryNode **nodes) {
	int rows = 0;
	int i = 0;

	while (1) {
		if (nodes[i]->next == NULL) {
			rows += 1 + nodes[i]->data->size;
			break;
		} else {
			rows += 1 + nodes[i]->data->size;
		}
		i++;
	}

	return rows;
}

/* Returns 1 if the text was scrolled up, 0 if a normal scroll occured, -1
 * if no scroll occured. */
int nc_scroll_prev_category
(WINDOW *wptr, CategoryNode **nodes, struct ScrollCursor *sc, 
 struct ColWidth *cw, FILE *rfptr, FILE *bfptr)
{
	int retval = -1;

	if (sc->catg_data < 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		sc->catg_node--;
		sc->catg_data = nodes[sc->catg_node]->data->size - 1;
		retval++;
	} else if (sc->catg_data >= 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data--;
		retval++;
	} else if (sc->catg_data == 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data = -1;
		sc->catg_node--;
		retval++;
	}

	sc->cur_y--;
	sc->select_idx--;
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (sc->select_idx >= 0 && sc->displayed < sc->total_rows && sc->cur_y < 0) {
		sc->cur_y = -1;
		if (sc->catg_data == -1) {
			nc_scroll_prev(nodes[sc->catg_node]->catg_fp, 
						   bfptr, wptr, cw, true);
//		} else if (sc->catg_data >= 0) {
		} else {
			nc_scroll_prev(nodes[sc->catg_node]->data->data[sc->catg_data], 
						   rfptr, wptr, cw, false);
		}
		retval++;
		sc->cur_y = getcury(wptr);
		mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	return retval;
}

/* Returns 1 if the text was scrolled down, 0 if a normal scroll occured, -1
 * if no scroll occured. */
int nc_scroll_next_category
(WINDOW *wptr, CategoryNode **nodes, struct ScrollCursor *sc, struct ColWidth *cw,
 FILE *rfptr, FILE *bfptr)
{
	int retval = -1;
	if (sc->catg_data < 0 && nodes[sc->catg_node]->data->size > 0) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		sc->catg_data = 0;

	/* Explicit cast of sc->catg_data to size_t is okay, the previous condition checks
	 * that it is a positive number.  vvvvvv */
		retval++;
	} else if (sc->catg_data >= 0 && (size_t)sc->catg_data < nodes[sc->catg_node]->data->size - 1) {
		mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		sc->catg_data++;
		retval++;
	} else if (sc->catg_data == nodes[sc->catg_node]->data->size - 1) {
		if (nodes[sc->catg_node]->data->size == 0) {
			mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, category_color(sc->catg_node), NULL); 
		} else {
			mvwchgat(wptr, sc->cur_y, 0, -1, A_NORMAL, 0, NULL); 
		}
		sc->catg_data = -1;
		sc->catg_node++;
		retval++;
	}

	sc->cur_y++;
	sc->select_idx++;
	mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (sc->displayed < sc->total_rows && sc->cur_y == getmaxy(wptr)) {
		if (sc->catg_data == -1) {
			nc_scroll_next(nodes[sc->catg_node]->catg_fp, 
						   bfptr, wptr, cw, true);
		} else {
			nc_scroll_next(nodes[sc->catg_node]->data->data[sc->catg_data],
						   rfptr, wptr, cw, false);
		}
		retval++;
		sc->cur_y = getcury(wptr);
		mvwchgat(wptr, sc->cur_y, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	}

	return retval;
}

void init_scroll_cursor(struct ScrollCursor *sc, CategoryNode **nodes) 
{
	sc->total_rows = get_total_displayed_rows(nodes);
	sc->displayed = 0;
	sc->select_idx = 0; // Tracks selection for indexing
	sc->cur_y = 0; // Tracks cursor position in window
	sc->catg_node = 0; // Tracks which node the cursor is on
	/* Tracks which member record the cursor is on, begins at -1 to mark that
	 * the first cursor position is a node. */
	sc->catg_data = -1;
	sc->sidebar_idx = 0;
}

struct Plannedvals *get_total_planned(CategoryNode **nodes) {
	struct Plannedvals *pv = malloc(sizeof(*pv));
	if (pv == NULL) {
		memory_allocate_fail();
	}

	pv->exp = 0.0;
	pv->inc = 0.0;

	int i = 0;
	while (1) {
		struct BudgetTokens *bt = tokenize_budget_byte_offset(nodes[i]->catg_fp);
		if (nodes[i]->next == NULL) {
			if (bt->transtype == 1) {
				pv->inc += bt->amount;
			} else {
				pv->exp += bt->amount;
			}
			free_budget_tokens(bt);
			bt = NULL;
			break;
		} else {
			if (bt->transtype == 1) {
				pv->inc += bt->amount;
			} else {
				pv->exp += bt->amount;
			}
			free_budget_tokens(bt);
			bt = NULL;
		}
		i++;
	}
	return pv;
}

double get_left_to_budget(CategoryNode **nodes) {
	struct Plannedvals *pv = get_total_planned(nodes);
	double ret = pv->inc - pv->exp;
	free(pv);
	pv = NULL;
	return ret;
}

void nc_print_initial_read_budget_loop(WINDOW *wptr, struct ScrollCursor *sc,
									   CategoryNode **nodes, struct ColWidth *cw,
									   FILE *fptr)
{
	int max_y = getmaxy(wptr);
	int total_nodes = get_total_nodes(nodes);
	char *line_str;
	char linebuff[LINE_BUFFER];
	struct LineData ld;

	/* 
	 * For each category print the budget line and the records that match 
	 * the category. Increment displayed to keep track of how many lines are
	 * displayed (this is to keep track of scrolling).
	 */
	for (int i = 0; sc->displayed < max_y && sc->displayed < sc->total_rows 
		 && i < total_nodes; i++) {

		struct BudgetTokens *bt = tokenize_budget_byte_offset(nodes[i]->catg_fp);
		nc_print_category_hr(wptr, cw, bt, sc->displayed);
		mvwchgat(wptr, sc->displayed, 0, -1, A_NORMAL, category_color(i), NULL); 
		sc->displayed++;

		for (size_t j = 0; sc->displayed < max_y && sc->displayed < sc->total_rows 
		     && j < nodes[i]->data->size; j++)
		{
			fseek(fptr, nodes[i]->data->data[j], SEEK_SET);
			line_str = fgets(linebuff, sizeof(linebuff), fptr);
			tokenize_record(&ld, &line_str);
			nc_print_record_hr(wptr, cw, &ld, sc->displayed);
			free_tokenized_record_strings(&ld);
			sc->displayed++;
		}
		free_budget_tokens(bt);
	}

	wmove(wptr, 0, 0);
	mvwchgat(wptr, sc->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 

	if (debug_flag) {
		curs_set(1);
	}

	wrefresh(wptr);
}

/* An optimized version of refreshing the screen. This function only refreshes
 * as many lines as is required to get the data back on the screen after a
 * subwindow closes. */
static void refresh_budget_loop
(WINDOW *data, CategoryNode **nodes, struct ScrollCursor *sc, struct ColWidth *cw,
 FILE *rfptr, FILE *bfptr, int subwin_y)
{
	int tmp_idx = sc->select_idx;
	int subw_y_upper = (getmaxy(stdscr) / 2) - (subwin_y / 2) - BOX_OFFSET;
	int subw_y_lower = (getmaxy(stdscr) / 2) + (subwin_y / 2) - BOX_OFFSET;

	if (sc->cur_y <= subw_y_upper) {
		while (sc->cur_y < subw_y_lower && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx > 0) {
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	} else if (sc->cur_y >= subw_y_lower) {
		while (sc->cur_y > subw_y_upper && sc->select_idx > 0) { 
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	} else {
		while (sc->cur_y > subw_y_upper && sc->select_idx > 0) { 
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->cur_y < subw_y_lower && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_next_category(data, nodes, sc, cw, rfptr, bfptr);
		}
		while (sc->select_idx != tmp_idx && sc->select_idx + 1 < sc->total_rows) {
			nc_scroll_prev_category(data, nodes, sc, cw, rfptr, bfptr);
		}
	}
}

void draw_scroll_indicator(WINDOW *wptr) {
	char *etc = "...";
	int y = getmaxy(wptr) - 1;
	int x = getmaxx(wptr) - (strlen(etc) + BOX_OFFSET);
	mvwprintw(wptr, y, x, "%s", etc);
	wrefresh(wptr);
}

int n_spaces(int max_x, char *str1, char *str2) {
	return (int)(max_x - (BOX_OFFSET * 2) - strlen(str1) - strlen(str2));
}

int show_help_subwindow(void) {
	int rows = 15;
	WINDOW *wptr = create_input_subwindow_n_rows(rows);
	int y = 1;
	int mx = getmaxx(wptr);
	int my = getmaxy(wptr);

	char line[mx];
	for (size_t i = 0; i < sizeof(line); i++) {
		line[i] = ' ';
	}
	line[sizeof(line) - 1] = '\0';

	char *key[] = {
		"a, A, F1",
		"e, E, F2",
		"r, R, F3",
		"q, Q, F4",
		"s, S, F5",
		"o, O, F6",
		"k, ARROW UP",
		"j, ARROW DN",
		"PAGE UP",
		"PAGE DN",
		"HOME",
		"END",
		"ENTER",
		"K, ^HOME",
		"?, h, H"
	};

	char *help[] = {
		"Add a category, record, or new budget",
		"Edit a record or category",
		"Select a new date to read data from",
		"Go back/quit",
		"Change sorted by method",
		"Show the yearly overview graphs",
		"Scroll up",
		"Scroll down",
		"Scroll up many",
		"Scroll down many",
		"Scroll to beginning",
		"Scroll to end",
		"Select date/view record details",
		"Move a category to the beginning",
		"Show this menu"
	};

	assert(sizeof(key) == sizeof(help));

	for (size_t i = 0; i < sizeof(key) / sizeof(char *); i++) {
		mvwprintw(wptr, y++, BOX_OFFSET, "%s%.*s%s\n", help[i], n_spaces(mx, help[i], key[i]), line, key[i]);
	}

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Help");
	wrefresh(wptr);
	nc_exit_window_key(wptr);
	return my;
}

void refresh_displayed_counter
(WINDOW *wptr, struct ScrollCursor *sc, struct DispCursor *dc)
{
	mvwhline(wptr, getmaxy(wptr) - 1, BOX_OFFSET, 0, getmaxx(wptr) - (BOX_OFFSET + 1));
	wrefresh(wptr);

	// Calc length of, for example, "32-50 of 80 records"
	char *disp = " -  of  records";
	int x_offset = 
		strlen(disp) + intlen(sc->displayed) + intlen(sc->total_rows) + BOX_OFFSET;

	if (sc->displayed < sc->total_rows) {
		mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - x_offset,
			"%d-%d of %d records", dc->first, dc->last, sc->total_rows);
		wrefresh(wptr);
	}
}

/*
 * Main loop for the user to interact with when selecting the read menu option.
 * If sorted by anything other than 'Category', nc_read_loop will be used.
 *
 * Prints scrollable data to the window pointed to by wptr, sorted by Category.
 * Categories will have their own row in the data with a user-modifiable value
 * retrived from BUDGET_DIR.
 */

void nc_read_budget_loop
(struct ReadWins *wins, FILE *rfptr, FILE *bfptr, struct SelRecord *sr,
 Vec *psc, CategoryNode **nodes, int yr, int mo)
{
	struct ColWidth cw_, *cw = &cw_;
	struct ScrollCursor sc_, *sc = &sc_;
	struct DispCursor dc_, *dc = &dc_;
	dc->first = 1;

	char *line;
	char linebuff[LINE_BUFFER];
	int c = 0;
	int subwin_y;
	int scroll_ret;

	init_scroll_cursor(sc, nodes);
	calculate_columns(cw, getmaxx(wins->data) + BOX_OFFSET);

	if (debug_flag) {
		debug_columns(wins->parent, cw);
	}

	nc_print_read_footer(stdscr);
	nc_print_initial_read_budget_loop(wins->data, sc, nodes, cw, rfptr);
	draw_read_window_borders_and_text(wins, sc, psc);
	dc->last = sc->displayed;

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(wins->parent, sc, dc);

		if (debug_flag) {
			nc_print_debug_line(wins->parent, sc);
		}
		c = wgetch(wins->data);

		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (sc->select_idx + 1 < sc->total_rows) {
				scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}

			break;
		case('k'):
		case(KEY_UP):
			if (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;
		case('K'):
		case(KEY_SHOME): // "SHIFT + HOME"
			if (sc->catg_data == -1 && sc->catg_node != 0) {
				mv_category_to_top(nodes, sc->catg_node);
				sr->flag = RESIZE;
				sr->index = 0;
				return;
			}
			break;
		case('?'):
			subwin_y = show_help_subwindow();
			init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			refresh_budget_loop(wins->data, nodes, sc, cw, rfptr, bfptr, subwin_y);
			c = 0;
			break;
		case('\n'):
		case('\r'):
			if (sc->catg_data >= 0) {
				fseek(rfptr, nodes[sc->catg_node]->data->data[sc->catg_data], SEEK_SET);
				line = fgets(linebuff, sizeof(linebuff), rfptr);
				subwin_y = show_detail_subwindow(line);
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
				refresh_budget_loop(wins->data, nodes, sc, cw, rfptr, bfptr, subwin_y);
				c = 0;
			} else {
				c = 0;
			}
			break;

		case(KEY_NPAGE): // PAGE DOWN
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx + 1 < sc->total_rows) {
					scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
					dc->first += scroll_ret;
					dc->last += scroll_ret;
				}
			}
			break;
		case(KEY_PPAGE): // PAGE UP
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx > 0) {
					scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
					dc->first -= scroll_ret;
					dc->last -= scroll_ret;
				}
			}
			break;

		case(KEY_EOL):
		case(KEY_END):
			while (sc->select_idx + 1 < sc->total_rows) {
				scroll_ret = nc_scroll_next_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;
		case(KEY_BEG):
		case(KEY_HOME):
			while (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_category(wins->data, nodes, sc, cw, rfptr, bfptr);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('['):
			if (sc->sidebar_idx > 0) {
				sc->sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case(']'):
			if (nodes[sc->sidebar_idx]->next != NULL) {
				sc->sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case('A'):
		case('a'):
		case(KEY_F(ADD)):
			sr->flag = ADD;
			return;
		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			if (sc->catg_data < 0) { 
				sr->flag = EDIT_CATG;
				sr->opt = nodes[sc->catg_node]->data->size;
				sr->index = sc->catg_node;
				//sr->index = nodes[sc->catg_node]->catg_fp;
			} else {
				sr->flag = EDIT;
				sr->index = nodes[sc->catg_node]->data->data[sc->catg_data];
			}
			return;
		case('R'):
		case('r'):
		case(KEY_F(READ)):
			sr->flag = READ;
			sr->index = 0;
			return;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			sr->flag = QUIT;
			sr->index = 0;
			return;
		case('S'):
		case('s'):
		case(KEY_F(SORT)):
			sr->flag = SORT;
			sr->index = 0;
			return;
		case('O'):
		case('o'):
		case(KEY_F(OVERVIEW)):
			sr->flag = OVERVIEW;
			sr->index = 0;
			return;
		case(KEY_RESIZE):
			sr->flag = RESIZE;
			sr->index = 0;
			return;
		}
	}

	sr->flag = NO_SELECT;
	sr->index = 0;
	return;
}

void init_scroll_cursor_no_category(struct ScrollCursor *sc)
{
	sc->total_rows = 0;
	sc->displayed = 0;
	sc->select_idx = 0; // Tracks selection for indexing
	sc->cur_y = 0; // Tracks cursor position in window
	sc->catg_node = 0; // Tracks which node the cursor is on
	/* Tracks which member record the cursor is on, begins at -1 to mark that
	 * the first cursor position is a node. */
	sc->catg_data = -1;
	sc->sidebar_idx = 0;
}

/* Print initial lines based on screen size for nc_read_loop */
void nc_print_initial_read_loop(WINDOW *wptr, struct ScrollCursor *sc,
								struct ColWidth *cw, FILE *fptr, Vec *psc)
{
	char *line_str;
	char linebuff[LINE_BUFFER];
	struct LineData ld;

	for (int i = 0; i < getmaxy(wptr) && sc->displayed < psc->size; i++) {
		fseek(fptr, psc->data[i], SEEK_SET);
		line_str = fgets(linebuff, sizeof(linebuff), fptr);
		tokenize_record(&ld, &line_str);
		nc_print_record_hr(wptr, cw, &ld, i);
		free_tokenized_record_strings(&ld);
		sc->displayed++;
	}

	wmove(wptr, 0, 0);
	mvwchgat(wptr, sc->select_idx, 0, -1, A_REVERSE, REVERSE_COLOR, NULL); 
	if (debug_flag) {
		curs_set(1);
	}
	wrefresh(wptr);
}

static size_t get_n_nodes(CategoryNode **nodes) {
	size_t n = 0;
	while (nodes[n]->next != NULL) {
		n++;
	}
	return n;
}

/*
 * Main read loop. Populates member values in the struct pointed to 
 * by sr on a MenuKeys press. Prints lines by seeking FPI to the byte offset
 * of psc->data. Sort occurs before this function in nc_read_setup.
 */
void nc_read_loop
(struct ReadWins *wins, FILE *fptr, struct SelRecord *sr, Vec *psc,
 CategoryNode **nodes)
{
	struct ColWidth cw_, *cw = &cw_;
	struct ScrollCursor sc_, *sc = &sc_;
	struct DispCursor dc_, *dc = &dc_;
	dc->first = 1;
	char linebuff[LINE_BUFFER];
	int c = 0;
	int scroll_ret;

	init_scroll_cursor_no_category(sc);
	calculate_columns(cw, getmaxx(wins->data) + BOX_OFFSET);
	nc_print_read_footer(stdscr);
	nc_print_initial_read_loop(wins->data, sc, cw, fptr, psc);
	sc->total_rows = psc->size;
	dc->last = sc->displayed;
	draw_read_window_borders_and_text(wins, sc, psc);

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wins->data);
		refresh_displayed_counter(wins->parent, sc, dc);

		if (debug_flag) {
			//nc_print_debug_line(wins->parent, psc->data[sc->select_idx], sc->select_idx, sc->cur_y);
		}
		c = wgetch(wins->data);

		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (sc->select_idx + 1 < psc->size) {
				scroll_ret = nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;
		case('k'):
		case(KEY_UP):
			if (sc->select_idx > 0) {
				scroll_ret = nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('\n'):
		case('\r'):
			fseek(fptr, psc->data[sc->select_idx], SEEK_SET);
			char *line = fgets(linebuff, sizeof(linebuff), fptr);
			show_detail_subwindow(line);
			refresh_on_detail_close_uniform(wins->data, wins->parent, sc->displayed);
			c = 0;
			break;

		case(KEY_NPAGE): // PAGE DOWN
			for(int i = 0; i < 10; i++) {
				if (sc->select_idx + 1 < psc->size) {
					nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
					dc->first += scroll_ret;
					dc->last += scroll_ret;
				}
			}
			break;
		case(KEY_PPAGE): // PAGE UP
			for (int i = 0; i < 10; i++) {
				if (sc->select_idx > 0) {
					nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
					dc->first -= scroll_ret;
					dc->last -= scroll_ret;
				}
			}
			break;

		case(KEY_END):
			while (sc->select_idx + 1 < psc->size) {
				nc_scroll_next_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first += scroll_ret;
				dc->last += scroll_ret;
			}
			break;
		case(KEY_HOME):
			while (sc->select_idx > 0) {
				nc_scroll_prev_read_loop(wins->data, sc, cw, fptr, psc);
				dc->first -= scroll_ret;
				dc->last -= scroll_ret;
			}
			break;

		case('['):
			if (sc->sidebar_idx > 0) {
				sc->sidebar_idx--;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case(']'):
			if (nodes[sc->sidebar_idx]->next != NULL) {
				sc->sidebar_idx++;
				init_sidebar_body(wins->sidebar_body, nodes, sc->sidebar_idx);
			}
			break;
		case('A'):
		case('a'):
		case(KEY_F(ADD)):
			sr->flag = ADD;
			sr->index = psc->data[sc->select_idx];
			return;
		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			sr->flag = EDIT;
			sr->index = psc->data[sc->select_idx];
			return;
		case('R'):
		case('r'):
		case(KEY_F(READ)):
			sr->flag = READ;
			sr->index = 0;
			return;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			sr->flag = QUIT;
			sr->index = 0;
			return;
		case('S'):
		case('s'):
		case(KEY_F(SORT)):
			sr->flag = SORT;
			sr->index = 0;
			return;
		case('O'):
		case('o'):
		case(KEY_F(OVERVIEW)):
			sr->flag = OVERVIEW;
			sr->index = 0;
			return;
		case(KEY_RESIZE):
			sr->flag = RESIZE;
			sr->index = 0;
			return;

		}
	}

	sr->flag = NO_SELECT;
	sr->index = 0;
	return;
}

void free_category_nodes(CategoryNode **nodes) {
	int i = 0;
	while (1) {
		if (nodes[i]->next == NULL) {
			free(nodes[i]->data);
			free(nodes[i]);
			break;
		}
		free(nodes[i]->data);
		free(nodes[i]);
		i++;
	}

	free(nodes);
}

/*
 * Initializes CategoryNode.data. The data is a Vec which contains all of
 * the file position byte offsets for the records that match the CategoryNode's
 * category.
 */
void init_category_nodes(CategoryNode *node, Vec *chunk, int m, int y) {
	struct BudgetTokens *pbt = tokenize_budget_byte_offset(node->catg_fp);
	Vec *pr = get_records_by_any(m, -1, y, pbt->catg, NULL, -1, -1, chunk);
	node->data = pr;
	free_budget_tokens(pbt);
}

/*
 * Returns a pointer to a pointer to the first CategoryNode in a doubly 
 * linked list of CategoryNodes.
 */
CategoryNode **create_category_nodes(int m, int y) {
	Vec *pcbo = get_budget_catg_by_date_bo(m, y);
	Vec *chunk = get_records_by_mo_yr(m, y);
	unsigned long n = pcbo->size;
	CategoryNode **pnode = malloc(sizeof(CategoryNode *) * n);
	if (pnode == NULL) {
		memory_allocate_fail();
	}

	for (size_t i = 0; i < n; i++) {
		pnode[i] = malloc(sizeof(CategoryNode));
		if (pnode[i] == NULL) {
			memory_allocate_fail();
		}

		pnode[i]->catg_fp = pcbo->data[i];

		if (i == 0) {
			pnode[0]->prev = NULL;
		} else if (i > 0) {
			pnode[i]->prev = pnode[i - 1];
			pnode[i - 1]->next = pnode[i];		
		}

		if (i == n - 1) {
			pnode[i]->next = NULL;
		}

		init_category_nodes(pnode[i], chunk, m, y);
	}

	free(chunk);
	free(pcbo);
	return pnode;
}

struct MenuParams *init_add_main_menu(void) {
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * 3));
	if (mp == NULL) {
		memory_allocate_fail();
	}
	mp->items = 3;
	mp->title = "Select Data Type to Add";
	mp->strings[0] = "Add Transaction";
	mp->strings[1] = "Add Category";
	mp->strings[2] = "Create New Budget";

	return mp;
}

struct MenuParams *init_add_menu(void) {
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * 2));
	if (mp == NULL) {
		memory_allocate_fail();
	}
	mp->items = 2;
	mp->title = "Select Data Type to Add";
	mp->strings[0] = "Add Transaction";
	mp->strings[1] = "Add Category";

	return mp;
}

int nc_read_setup_input_year(WINDOW *wptr) {
	int yr = nc_read_select_year(wptr);
	if (yr == -(NO_RCRD)) {
		mvwxcprintw(wptr, getmaxy(wptr) / 2, 
			  "No records exist, add (F1) to get started");
		wgetch(wptr);
		return -(NO_RCRD);
	} else if (yr < 0) {
		return yr;
	};
	return yr;
}

/* Draws a border around the window 'wptr' with "T" shaped characters to
 * mesh seamlessly with the window border of the sidebar. */
void draw_parent_box_with_sidebar(WINDOW *wptr) {
	wborder(wptr, 0, 0, 0, 0, 0, ACS_TTEE, 0, ACS_BTEE);
	wrefresh(wptr);
}

/* Draws all of the window borders, then the border text on top. Call this
 * function any time the borders/text need to be updated. */
void draw_read_window_borders_and_text
(struct ReadWins *wins, struct ScrollCursor *sc, Vec *psc)
{
	// Draw borders in order for correct intersecting lines
	if (wins->sidebar_parent != NULL || wins->sidebar_body != NULL) {
		draw_sidebar_parent_border(wins->sidebar_parent);
		draw_body_border(wins->sidebar_body);
	} else {
		box(wins->parent, 0, 0);
		nc_print_balances_text(wins->parent, psc);
	}

//	// For calculating the length of, for example, "32 out of 50 records shown"
//	char *disp = "/ Records Displayed";
//	int x_offset = strlen(disp) + intlen(sc->displayed) + 1 + intlen(sc->total_rows) + 1;
//
//	if (sc->displayed < sc->total_rows) {
//		mvwprintw(wins->parent, getmaxy(wins->parent) - 1, getmaxx(wins->data) - x_offset, "%d/%d Records Displayed", sc->displayed, sc->total_rows);
//		wrefresh(wins->parent);
//	}
}

void get_dates(struct SelRecord *sr, struct Datevals *dates) {
	WINDOW *wptr = newwin(LINES - 1, 0, 0, 0);
	box(wptr, 0, 0);

	if (!month_or_year_exists(dates->month, dates->year)) {
		dates->month = 0;
		dates->year = 0;
	}

	if (!dates->year) {
		dates->year = nc_read_setup_input_year(wptr);
		if (dates->year < 0) {
			dates->month = -1;
			sr->flag = -(dates->year);
			nc_exit_window(wptr);
			return;
		}
	}

	if (!dates->month) {
		dates->month = nc_read_select_month(wptr, dates->year);
		if (dates->month < 0) {
			sr->flag = -(dates->month);
			nc_exit_window(wptr);
			return;
		}
	}
	
	nc_exit_window(wptr);
}

void free_windows(struct ReadWins *wins) {
	nc_exit_window(wins->data);
	nc_exit_window(wins->parent);
	if (wins->sidebar_parent != NULL) {
		nc_exit_window(wins->sidebar_parent);
		nc_exit_window(wins->sidebar_body);
	}
	free(wins);
}

void debug_fields(void) {
	move(0,0);
	FILE *bfptr = open_budget_csv("r");
	struct BudgetHeader *bh = parse_budget_header(bfptr);

	printw("Budget Fields: %d, %d, %d, %d, %d\n",
	 bh->month,
	 bh->year,
	 bh->catg,
	 bh->transtype, 
	 bh->value);

	free(bh);
	fclose(bfptr);
	FILE *fptr = open_record_csv("r");
	struct RecordHeader *rh = parse_record_header(fptr);

	printw("Record Fields: %d, %d, %d, %d, %d, %d, %d\n", 
	 rh->month,
	 rh->day,
	 rh->year,
	 rh->catg,
	 rh->desc,
	 rh->transtype,
	 rh->value);

	free(rh);
	fclose(fptr);
	getch();
}

void nc_print_date(WINDOW *wptr, int yr, int mo) {
	int y = 0;
	int x = BOX_OFFSET;
	mvwprintw(wptr, y, x, "%s %d", abbr_months[mo - 1], yr);
	wrefresh(wptr);
}

void nc_print_sort_text(WINDOW *wptr, int sort) {
	char *text;
	sort == SORT_DATE ? (text = "Date") : (text = "Category");
	int y = 0;
	int x = getmaxx(wptr) - strlen(text) - strlen("Sort By: ") - BOX_OFFSET;
	mvwprintw(wptr, y, x, "Sort By: %s", text);
	wrefresh(wptr);
}

void nc_read_setup_default(struct ReadRet *rret) {
	nc_read_setup(0, 0, SORT_CATG, rret);
}

void nc_read_setup_year(int sel_year, struct ReadRet *rret) {
	nc_read_setup(sel_year, 0, SORT_CATG, rret);
}

void nc_read_setup(int sel_year, int sel_month, int sort, struct ReadRet *rret) {
	nc_print_main_menu_footer(stdscr);
	if (debug_flag) {
		nc_print_debug_flag(stdscr);
	}
	refresh();

	struct SelRecord sr_, *sr = &sr_;
	struct Datevals dv_, *dates = &dv_;
	dates->month = sel_month;
	dates->year = sel_year;
	sr->flag = -1;

	get_dates(sr, dates);
	if (dates->year < 0 || dates->month < 0) {
		goto err_select_date_fail;
	}

	CategoryNode **nodes;
	FILE *fptr = open_record_csv("r");
	Vec *pidx = index_csv(fptr);
	Vec *plines;
	Vec *psc;
	size_t n_records;
	char *ret;
	bool sidebar_exists;
	// To hold the return value of wgetch()/getch()
	int c;

	if (debug_flag) {
		debug_fields();
	}

	struct ReadWins *wins = create_read_windows();
	if (wins->sidebar_parent == NULL || wins->sidebar_body == NULL) {
		sidebar_exists = false;
	} else {
		sidebar_exists = true;
	}

	/* If plines->size is zero, whatever function cannot return to an
	 * nc_read_setup with month and year parameters, as these dates no longer
	 * hold any records. */
	plines = get_matching_line_nums(fptr, dates->month, dates->year);
	if (plines == NULL) {
		free(pidx);
		fclose(fptr);
		return;
	}

	n_records = plines->size;
	if (plines->size == 0 && sort == SORT_DATE) {
		sort = SORT_CATG;
	}

	if (sort == SORT_DATE) {
		psc = sort_by_date(fptr, pidx, plines);	
	} else {
		psc = sort_by_category(fptr, pidx, plines, dates->year, dates->month);
	}

	nodes = create_category_nodes(dates->month, dates->year);
	if (debug_flag) {
		debug_category_nodes(wins->data, nodes);
	}

	if (sidebar_exists) {
		init_sidebar_parent(wins->sidebar_parent, psc, get_left_to_budget(nodes));
		init_sidebar_body(wins->sidebar_body, nodes, 0);
		draw_parent_box_with_sidebar(wins->parent);
	} else {
		box(wins->parent, 0, 0);
	}

	print_column_headers(wins->parent, BOX_OFFSET);
	nc_print_date(wins->parent, dates->year, dates->month);
	nc_print_sort_text(wins->parent, sort);

	if (sort == SORT_DATE) {
		nc_read_loop(wins, fptr, sr, psc, nodes);
	} else if (sort == SORT_CATG) {
		FILE *bfptr = open_budget_csv("r");
		nc_read_budget_loop(wins, fptr, bfptr, sr, psc, nodes, dates->year, dates->month);
		fclose(bfptr);
	}

	free(psc);
	psc = NULL;
	free(plines);
	plines = NULL;
	if (sr->flag != EDIT_CATG) {
		free_category_nodes(nodes);
		nodes = NULL;
	}
	free_windows(wins);
	wins = NULL;
	free(pidx);
	pidx = NULL;
	fclose(fptr);
	fptr = NULL;

	rret->mo = dates->month;
	rret->yr = dates->year;
	rret->sort = sort;

err_select_date_fail:
	switch(sr->flag) {
	case(NO_SELECT):
		rret->flag = RRET_DEFAULT;
//		nc_read_setup_default();
		break;
	case(ADD):
		nc_print_input_footer(stdscr);
		if (dates->year < 0 || dates->month < 0) {
			struct MenuParams *mp = init_add_main_menu();
			c = nc_input_menu(mp);
			switch (c) {
			case 0:
				nc_add_transaction_default();
				break;
			case 1:
				ret = nc_add_budget_category(0, 0);
				free(ret);
				break;
			case 2:
				nc_create_new_budget();
				break;
			}
			free(mp);
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		} else {
			struct MenuParams *mp = init_add_menu();
			int c = nc_input_menu(mp);
			if (c == 0) {
				nc_add_transaction(dates->year, dates->month);
			} else if (c == 1) {
				ret = nc_add_budget_category(dates->year, dates->month);
				free(ret);
			}
			free(mp);
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		}
		break;
	case(EDIT):
		nc_print_quit_footer(stdscr);
		nc_edit_transaction(sr->index);
		rret->flag = RRET_BYDATE;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(READ):
		rret->flag = RRET_DEFAULT;
//		nc_read_setup_default(rret);
		break;
	case(QUIT):
		rret->flag = RRET_QUIT;
		break;
	case(SORT):
		if (sort == SORT_CATG && n_records == 0) {
			nc_message("Cannot sort by date, no records exist");
			sort = SORT_CATG;
		} else if (sort == SORT_CATG) {
			sort = SORT_DATE;
		} else {
			sort = SORT_CATG;
		}
		rret->flag = RRET_BYDATE;
		rret->sort = sort;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(OVERVIEW):
		nc_overview_setup(dates->year);
		rret->flag = RRET_BYDATE;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(EDIT_CATG):
		nc_edit_category(sr->index, sr->opt, nodes); 
		free_category_nodes(nodes);
		nodes = NULL;
		if (n_records > 0) {
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		} else {
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		}
		break;
	case(RESIZE):
		while (test_terminal_size() == -1) {
			getch();
		}
		if (dates->month > 0 && dates->year > 0) {
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		} else {
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		}
		break;
	case(NO_RCRD):
		c = getch();
		switch(c) {
		case(KEY_F(1)):
		case('a'):
		case('A'):
			sr->flag = ADD;
			goto err_select_date_fail;
			break;
		case(KEY_F(4)):
		case('q'):
		case('Q'):
			rret->flag = RRET_QUIT;
			break;
		default:
			rret->flag = RRET_QUIT;
			break;
		}
		break;
	default:
		rret->flag = RRET_QUIT;
		break;
	}
	refresh();
}

int delete_csv_record(int linetodelete) {
	if (linetodelete == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_record_file(tmpfptr, fptr);
	return 0;
}

void edit_transaction(void) {
	int target;
	int humantarget;
	struct LineData linedata, *ld = &linedata;

	FILE *fptr = open_record_csv("r+");
	assert(ftell(fptr) == 0);

	legacy_read_csv();
	
	Vec *pidx = index_csv(fptr);

	do {
		puts("Enter line number");
		humantarget = input_n_digits(sizeof(size_t) + 1, 2);
	} while (humantarget <= 0 || humantarget > pidx->size);

	target = humantarget - 1;

	if (debug_flag) {
		printf("TARGET: %d\n", target);
		printf("TARGET OFFSET: %ld\n", pidx->data[target]);
	}

	fseek(fptr, pidx->data[target], SEEK_SET);

	if (debug_flag) {
		printf("COMMANDED SEEK OFFSET: %ld\n", pidx->data[target]);
	}
	
	char linebuff[LINE_BUFFER];
	char *str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		puts("failed to read line");
		exit(1);
	}

	tokenize_record(ld, &str);

	struct LineData *pLd = malloc(sizeof(*ld));
	if (pLd == NULL) {
		free(pidx);
		fclose(fptr);
		fptr = NULL;
		memory_allocate_fail();
	}

	memcpy(pLd, ld, sizeof(*ld));

	print_record_vert(ld);
	
	int fieldtoedit;
	do {
		puts("Enter field to edit or press \"0\" to delete this transaction");
		fieldtoedit = input_n_digits(1, 1); // Only input 1 digit
	} while (fieldtoedit > 5 || fieldtoedit < 0);

	switch(fieldtoedit) {
	case 0:
		if (delete_csv_record(humantarget) == 0) {
			puts("Successfully Deleted Transaction");
		}
		break;
	case 1:
		edit_csv_record(humantarget, pLd, 1);
		break;
	case 2:
		edit_csv_record(humantarget, pLd, 2);
		break;
	case 3:
		edit_csv_record(humantarget, pLd, 3);
		break;
	case 4:
		edit_csv_record(humantarget, pLd, 4);
		break;
	case 5:
		edit_csv_record(humantarget, pLd, 5);
		break;
	default:
		return;
	}

	free_tokenized_record_strings(pLd);
	free(pLd);
	pLd = NULL;
	free(pidx);
	pidx = NULL;
	fclose(fptr);
	fptr = NULL;
}

int nc_main_menu(WINDOW *wptr) {
	struct ReadRet rr_, *rret = &rr_;
	struct Datevals *dv;

	enum add_sel {
		ADD_TRNS = 0,
		ADD_CATG,
		ADD_BUDG
	} add_sel;

	char *ret;
	int c = 0;

	while (c != KEY_F(QUIT) && c != 'q') {
		nc_print_welcome(wptr);
		nc_print_main_menu_footer(wptr);
		if (debug_flag) {
			nc_print_debug_flag(wptr);
		}
		wrefresh(wptr);
		c = getch();

		switch(c) {
		case('A'):
		case('a'):
		case (KEY_F(ADD)):
			wclear(wptr);
			struct MenuParams *mp = init_add_main_menu();
			add_sel = nc_input_menu(mp);
			free(mp);
			switch(add_sel) {
				case ADD_TRNS:
					nc_add_transaction_default();
					break;
				case ADD_CATG:
					ret = nc_add_budget_category(0, 0);
					free(ret);
					break;
				case ADD_BUDG:
					dv = nc_create_new_budget();
					if (dv != NULL) {
						nc_read_setup(dv->year, dv->month, SORT_CATG, rret);
						free(dv);
					}
					break;
				default:
					break;
			}
			break;

		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			wclear(wptr);
			break;

		case('R'):
		case('r'):
		case(KEY_F(READ)):
			wclear(wptr);
			nc_read_setup_default(rret);
			while(rret->flag != RRET_QUIT) {
				switch(rret->flag) {
				case(RRET_DEFAULT):
					nc_read_setup_default(rret);
					break;
				case(RRET_BYDATE):
					nc_read_setup(rret->yr, rret->mo, rret->sort, rret);
					break;
				default:
					nc_read_setup_default(rret);
					break;
				}
			}
			break;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			wclear(wptr);
			return 1;

		case(KEY_RESIZE):
			wclear(wptr);
			break;
		case('H'):
		case('h'):
		case('?'):
			show_help_subwindow();
			break;
		}
	}
	endwin();
	return 0;
}

void main_menu(void) {
	int choice;
	printf("Make a selection:\n");
	printf("a - Add Transaction\n");
	printf("e - Edit Transaction\n"); 
	printf("r - Read CSV\n");
	printf("s - DEBUG Sort CSV\n");
	printf("q - Quit\n");

	char *userstr = user_input(STDIN_SMALL_BUFF); // Must be free'd
	
	while (userstr == NULL) {
		main_menu();
	}

	if ((choice = upper(userstr)) == 0) {
		puts("Invalid character");
		free(userstr);
		userstr = NULL;
		main_menu();
	}
	
	free(userstr);
	userstr = NULL;

	switch (choice) {
	case('A'):
		printf("-*-ADD TRANSACTION-*-\n");
		add_transaction();
		break;
	case('E'):
		printf("-*-EDIT TRANSACTION-*-\n");
		edit_transaction();
		break;
	case('R'):
		printf("-*-READ CSV-*-\n");
		legacy_read_csv();
		break;
	case('Q'):
		printf("Quiting\n");
		exit(0);
	default:
		puts("Invalid character");
		printf("\n");
		main_menu();
	}
	return;
}

void fix_budget_header(void) {
	int linetodelete = 0;
	FILE *fptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		} else {
			fputs("month,year,category,transtype,value\n", tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_budget_file(tmpfptr, fptr);
}

void fix_record_header(void) {
	int linetodelete = 0;
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		} else {
			fputs("month,day,year,category,description,transtype,value\n", tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_record_file(tmpfptr, fptr);
}

/* Verifies that the files needed to run termbudget exist and writes headers */
int verify_files_exist(void) {
	FILE *rfptr = open_record_csv("a");
	if (rfptr == NULL) {
		perror("Failed to open/create record file");
		return -1;
	}
	fseek(rfptr, 0, SEEK_END);
	if (ftell(rfptr) == 0) {
		fputs("month,day,year,category,description,transtype,value\n", rfptr);
	}
	fclose(rfptr);

	if (!validate_record_header()) {
		printf("data.csv header failed validation, has it been edited?");
		fix_record_header();
		getchar();
	}

	FILE *bfptr = open_budget_csv("a");
	if (bfptr == NULL) {
		perror("Failed to open/create budget file");
		return -1;
	}
	fseek(bfptr, 0, SEEK_END);
	if (ftell(bfptr) == 0) {
		fputs("month,year,category,transtype,value\n", bfptr);
	}
	fclose(bfptr);

	if (!validate_budget_header()) {
		printf("budget.csv header failed validation, has it been edited?");
		fix_budget_header();
		getchar();
	}

	return 0;
}

void print_usage(void) {
	printf("\x1b[1mUsage: termbudget [OPTIONS]\x1b[0m\n\n");
	printf("OPTIONS:\n");
	printf("-c               CLI MODE\n");
	printf("-d               DEBUG\n");
	printf("-v               NO VERIFY, does not verify csv records formatting.\n");
	printf("--convert FILE   Converts FILE to termbudget compatible CSV\n");
}

int main(int argc, char **argv) {
	if (verify_files_exist() == -1) {
		exit(1);
	}

	debug_flag = 0;
	cli_flag = 0;
	verify_flag = 1;

	char opt[LINE_BUFFER];
	int corrected = 0;
	size_t count;

	if (argc > 1) {
		strncpy(opt, argv[1], LINE_BUFFER);
		for (size_t i = 1; i < strlen(opt); i++) { // Args start at 1
			if (opt[0] == '-') {
				switch(opt[i]) {
				case('d'):
					debug_flag = 1;
					break;
				case('c'):
					cli_flag = 1;
					break;
				case('v'):
					verify_flag = 0;
					break;
				case('-'):
					if (strcmp(argv[1], "--convert") == 0) {
						count = convert_chase_csv(argv[i + 1]);
						printf("Converted %ld records", count);
						getc(stdin);
						exit(0);
					}
					break;
				default:
					print_usage();
					exit(0);
					break;
				}
			}
		}
	}

	if (verify_flag) {
		assert(record_len_verification());
		corrected = verify_categories_exist_in_budget();
	}

	if (debug_flag && verify_flag) {
		printf("Corrected %d records\n", corrected);
		printf("Press enter to continue");
		getc(stdin);
	}

	if (!cli_flag) {
		stdscr = nc_init_stdscr();
		int flag = 0;
		while (flag == 0) {
			flag = nc_main_menu(stdscr);
		}
	} else {
		while (1) {
			main_menu();
		}
	}
	endwin();
	return 0;
}
