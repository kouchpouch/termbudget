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
#include "read_init.h"
#include "fileintegrity.h"
#include "filemanagement.h"
#include "helper.h"
#include "sorter.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
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

//struct ReadRet {
//	int flag;
//	int yr;
//	int mo;
//	int sort;
//};

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

//struct SelRecord {
//	unsigned int flag;
//	long index;
//	long opt; // Optional flag
//};

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
int mv_tmp_to_record_file(FILE *tempfile, FILE *mainfile);
int delete_csv_record(int linetodelete);
Vec *get_matching_line_nums(FILE *fptr, int month, int year);
char *nc_add_budget_category(int yr, int mo);
//void draw_parent_box_with_sidebar(WINDOW *wptr);
void draw_scroll_indicator(WINDOW *wptr);
void draw_read_window_borders_and_text(struct ReadWins *wins, Vec *psc);

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
	struct Categories *pc = get_categories(month, year);

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
	size_t line = boff_to_linenum_budget(b) + 1;
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
			prc = get_categories(months->data[j], years->data[i]);
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

/* Prints record from ld, in vertical format, 5 rows. */
void nc_print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off) {
	mvwprintw(wptr, 1, x_off, "Date--> %d/%d/%d", ld->month, ld->day, ld->year);
	mvwprintw(wptr, 2, x_off, "Cat.--> %s", ld->category);
	mvwprintw(wptr, 3, x_off, "Desc--> %s", ld->desc);
	mvwprintw(wptr, 4, x_off, "Type--> %s", ld->transtype == 0 ? "Expense" : "Income");
	mvwprintw(wptr, 5, x_off, "Amt.--> %.2f", ld->amount);
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

/* For a la carte budget category creation, returns a malloc'd char * which
 * is free'd by the caller */
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

int nc_edit_csv_record(int replaceln, int edit_field, struct LineData *ld) {
	if (replaceln == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	replaceln += 1;

	enum EditRecordFields field = edit_field;
	char linebuff[LINE_BUFFER];
	char *line;
	int linenum = 0;

	switch(field) {
	case E_DATE:
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

	case E_CATG:
		ld->category = nc_select_category(ld->month, ld->year);
		if (ld->category == NULL) {
			goto err_fail;
		}
		break;

	case E_DESC:
		ld->desc = nc_input_string("Enter Description");
		if (ld->desc == NULL) {
			goto err_fail;
		}
		break;

	case E_TYPE:
		ld->transtype = nc_input_transaction_type();
		if (ld->transtype < 0) {
			goto err_fail;
		}
		break;

	case E_AMNT:
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
		if (field == E_CATG) {
			free(ld->category);
			ld->category = NULL;
		}
		if (field == E_DESC) {
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

	if (field == E_CATG) {
		free(ld->category);
		ld->category = NULL;
	}
	if (field == E_DESC) {
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

/* Prints the value of each struct ColWidth memeber to the window wptr */
void debug_columns(WINDOW *wptr, struct ColWidth *cw) {
	wmove(wptr, 5, 10);
	wprintw(wptr, "DATE: %d CATG: %d, DESC: %d, TRNS: %d, AMNT: %d\n",
	cw->date, cw->catg,	cw->desc, cw->trns, cw->amnt);
	wrefresh(wptr);
	wgetch(wptr);
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
		nc_edit_csv_record(linenum, E_DATE, ld);
		break;
	case E_CATG:
		free(ld->category);
		ld->category = NULL;
		nc_edit_csv_record(linenum, E_CATG, ld);
		break;
	case E_DESC:
		nc_print_input_footer(stdscr);
		free(ld->desc);
		ld->desc = NULL;
		nc_edit_csv_record(linenum, E_DESC, ld);
		break;
	case E_TYPE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, E_TYPE, ld);
		break;
	case E_AMNT:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, E_AMNT, ld);
		break;
	case DELETE:
		nc_print_input_footer(stdscr);
		if (nc_confirm_input("Confirm Delete")) {
			if (delete_csv_record(linenum) == 0) {
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

void add_main_no_date(void) {
	char *ret;
	int c;
	struct MenuParams *mp = init_add_main_menu();
	c = nc_input_menu(mp);
	free(mp);
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
}

void add_main_with_date(struct Datevals *dv) {
	char *ret;
	int c;
	struct MenuParams *mp = init_add_menu();
	c = nc_input_menu(mp);
	free(mp);
	if (c == 0) {
		nc_add_transaction(dv->year, dv->month);
	} else if (c == 1) {
		ret = nc_add_budget_category(dv->year, dv->month);
		free(ret);
	}
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

	assert(pidx->size < INT_MAX);

	do {
		puts("Enter line number");
		humantarget = input_n_digits(sizeof(size_t) + 1, 2);
	} while (humantarget <= 0 || humantarget > (int)pidx->size);

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
