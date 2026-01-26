#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <ncurses.h>
#include "helper.h"
#include "sorter.h"
#include "tui.h"
#include "parser.h"
#include "main.h"

#define MAX_LEN_AMOUNT 9
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4
#define MIN_INPUT_CHAR 2
#define INPUT_MSG_Y_OFFSET 2

#define CURRENT_YEAR 2026 // FIX This is to not be hard coded

bool debug;
bool cli_mode;

enum MenuKeys {
	NO_SELECT = 0,
	ADD = 1,
	EDIT = 2,
	READ = 3,
	QUIT = 4,
	SORT = 5,
	OVERVIEW = 6,
	RESIZE = 13,
} menukeys;

enum SortBy {
	DATE = 0,
	CATEGORY = 1
} sortby;

const char *months[] = {
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

struct FlexArr {
	int lines;
	int data[];
};

struct Categories {
	int count;
	char *categories[];
};

struct SelRecord {
	unsigned int flag;
	int index;
};

struct Balances {
	double income;
	double expense;
};

void nc_read_setup_default();
void calculate_balance(struct Balances *pb, struct FlexArr *pbo);
void nc_read_setup(int sel_year, int sel_month, int sort);
int nc_confirm_record(struct LineData *ld);
void nc_print_record_hr(WINDOW *wptr, struct ColWidth *cw, struct LineData *ld, int y);
void nc_print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off);
struct Categories *list_categories(int month, int year);
struct FlexArr *index_csv();
int move_temp_to_main(FILE *tempfile, FILE *mainfile);
int delete_csv_record(int linetodelete);
struct FlexArr *get_matching_line_nums(FILE *fptr, int month, int year);

char *user_input(int n) {
	size_t buffersize = n + 1;
	char *buffer = (char *)malloc(buffersize);

	if (buffer == NULL) {
		puts("Failed to allocate memory");
		return buffer;
	}
	if (fgets(buffer, buffersize, stdin) == NULL) {
		printf("Invalid Input\n");
		goto FAIL;
	}

	int length = strnlen(buffer, buffersize);

	if (buffer[length - 1] != '\n' && buffer[length - 1] != '\0') {
		printf("Input is too long, try again\n");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		goto FAIL;
	}
	if (length < MIN_INPUT_CHAR) {
		puts("Input is too short");
		goto FAIL;
	}
	if (strstr(buffer, ",")) {
		puts("No commas allowed, we're using a CSV, after all!");
		goto FAIL;
	}
	return buffer; // Must be free'd

FAIL:
	free(buffer);
	buffer = NULL;
	return buffer;
}

char *nc_user_input(int n, WINDOW *wptr) {
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);

	int buffersize = n + 1;
	
	int center = max_x / 2 - n / 2;

	/* Print a line of underscores to accept the user input */
	for (int i = 0; i < n; i++) {
		mvwprintw(wptr, max_y - 4, center + i, "%c", '_');
	}

	keypad(wptr, true);
	wmove(wptr, max_y - 4, center);
	echo();
	curs_set(1);
	wrefresh(wptr);

	char temp[buffersize];
	wgetnstr(wptr, temp, n);

	noecho();

	char *buffer = (char *)malloc(buffersize);
	if (buffer == NULL) {
		return buffer;
	}

	strncpy(buffer, temp, buffersize);

	int length = strnlen(buffer, buffersize);

	if (buffer[length] != '\0') {
		mvwxcprintw(wptr, max_y - BOX_OFFSET, "Input is too long");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		goto FAIL;
	}

	if (length < 1) {
		mvwxcprintw(wptr, max_y - BOX_OFFSET, "Input is too short");
		goto FAIL;
	}

	if (strstr(buffer, ",")) {
		mvwxcprintw(wptr, max_y - BOX_OFFSET, "No commas allowed");
		goto FAIL;
	}

	curs_set(0);
	return buffer; // Must be free'd

FAIL:
	curs_set(0);
	free(buffer);
	buffer = NULL;
	return buffer;
}

int input_n_digits(int max_len, int min_len) {
	char *str = user_input(max_len + 1);

	while (str == NULL) {
		str = user_input(max_len + 1);
	}

	if ((int)strlen(str) <= min_len) {
		puts("Input is too short");
		free(str);
		str = NULL;
		return -1;
	}

	for (int i = 0; i < (int)strlen(str); i++) {
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

int nc_input_n_digits(WINDOW *wptr, int max_len, int min_len) {
	char *str = nc_user_input(max_len, wptr);
	while (str == NULL) {
		str = nc_user_input(max_len, wptr);
		if (debug) {
			mvwxcprintw(wptr, getmaxy(wptr), "nc_user_input() failed");
		}
	}

	if ((int)strlen(str) < min_len) {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Input is too short");
		wrefresh(wptr);
		free(str);
		str = NULL;
		return -1;
	}

	for (int i = 0; i < (int)strlen(str); i++) {
		if (!isdigit(*(str + i)) && (*(str + i) != '\n' || *(str + i) != '\0')) {
			mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, 
			   "Invalid character, must be digits");
			wrefresh(wptr);
			free(str);
			str = NULL;
			return -1;
		}
	}

	wrefresh(wptr);

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

int nc_confirm_input(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Confirm");
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET + 2, "(Y/N)");
	wrefresh(wptr_input);

	char confirm = wgetch(wptr_input);
	confirm = (char)upper(&confirm);

	nc_exit_window(wptr_input);

	if (confirm == 'Y') {
		return 1;	
	} else if (confirm == 'N') {
		return 0;
	}

	return -1;
}

int input_month(void) {
	int month;
	puts("Enter Month");
	while((month = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Enter a Vaid Month");
	}
	return month;
}

int input_year(void) {
	int year;
	if (cli_mode == true) {
		puts("Enter Year");
	}
	while((year = input_n_digits(MAX_LEN_YEAR, MIN_LEN_YEAR)) == -1);
	return year;
}

int input_day(int month, int year) {
	int day;
	puts("Enter Day");

	while((day = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1 ||
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

	while((t = input_n_digits(1, 1)) == -1 || (t != 1 && t != 2)) {
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

	if (pc->count > 0) {
		puts("Categories:");
		for (int i = 0; i < pc->count; i++) {
			printf("%s ", pc->categories[i]);
		}
		printf("\n");
	} else {
		puts("No categories exist for this month");
	}

/* This label is so ghetto */
RETRY:
	cat_exists = false;
	str = input_str_retry("Enter Category:");

	for (int i = 0; i < pc->count; i++) {
		if (strcmp(str, pc->categories[i]) == 0) {
			cat_exists = true;
			break;
		}
	}

	if (cat_exists != true) {
		puts("That category doesn't exist for this month, create it? [y/n]");
		if (confirm_input() != 1) {
			free(str);
			str = NULL;
			goto RETRY;
		}
	}

	for (int i = 0; i < pc->count; i++) {
		free(pc->categories[i]);
	}
	free(pc);
	return str;
}

int nc_input_month(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Month");
	wrefresh(wptr_input);

	int month;
	month = nc_input_n_digits(wptr_input, MAX_LEN_DAY_MON, 1);
	while(month <= 0 || month > 12) {
		clear_input_error_message(wptr_input);
		mvwxcprintw(wptr_input, getmaxy(wptr_input) - BOX_OFFSET, "Invalid Month");
		month = nc_input_n_digits(wptr_input, MAX_LEN_DAY_MON, 1);
	} 

	nc_exit_window(wptr_input);

	return month;
}

int nc_input_year(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Year");
	wrefresh(wptr_input);

	int year;
	do {
		year = nc_input_n_digits(wptr_input, MAX_LEN_YEAR, MIN_LEN_YEAR);
	} while(year < 0);

	nc_exit_window(wptr_input);

	return year;
}

int nc_input_day(int month, int year) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Day");
	wrefresh(wptr_input);

	int day = nc_input_n_digits(wptr_input, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	while(day == -1 || dayexists(day, month, year) == false) {
		clear_input_error_message(wptr_input);
		if (dayexists(day, month, year) == false) { 
			mvwxcprintw(wptr_input, getmaxy(wptr_input) - BOX_OFFSET, "Not a valid day");
			wrefresh(wptr_input);
		}
		day = nc_input_n_digits(wptr_input, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	}

	nc_exit_window(wptr_input);

	return day;
}

char *nc_input_string(char *msg) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, msg);
	char *str;
	do {
		str = nc_user_input(STDIN_LARGE_BUFF, wptr_input);
	} while(str == NULL);
	wrefresh(wptr_input);
	nc_exit_window(wptr_input);
	return str;
}

void nc_scroll_next_category(WINDOW *wptr) {
	;
}

char *nc_select_category(int month, int year) {
	struct Categories *pc = list_categories(month, year);
	WINDOW *wptr_parent = create_category_select_parent(pc->count);
	WINDOW *wptr = create_category_select_subwindow(wptr_parent);

	if (debug) {
		curs_set(1);
	}

	if (pc->count == 0) {
		goto MANUAL;
	}

	int displayed = 0;
	/* Print intital data based on window size */
	for (int i = 0; i < getmaxy(wptr) && i < pc->count; i++) {
		mvwxcprintw(wptr, i, pc->categories[i]);
		displayed++;
	}

	if (debug) {
		mvwprintw(wptr_parent, 0, 0, "N CATG: %d N DISP: %d", pc->count, displayed);
	}

	wrefresh(wptr);

	mvwchgat(wptr, 0, 0, -1, A_REVERSE, 0, NULL);

	int cur = 0; // Y=0 is the box and title, datalines start at 1.
	int select = 0;
	int c = 0;

	int max_y = getmaxy(wptr);
	char *manual_entry;
	keypad(wptr, true);

	while (c != '\n' && c != '\r') {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (select + 1 < pc->count) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur++;
				select++;

				if (displayed < pc->count && cur == max_y) {
					wmove(wptr, 0, 0);
					wdeleteln(wptr);
					mvwxcprintw(wptr, max_y - 1, pc->categories[select]);
					cur = max_y - 1;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, 0, NULL);
			}
			break;
		case('k'):
		case(KEY_UP):
			if (select - 1 >= 0) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur--;
				select--;

				if (select >= 0 && displayed < pc->count && cur == -1) {
					wmove(wptr, 0, 0);
					winsertln(wptr);
					mvwxcprintw(wptr, 0, pc->categories[select]);
					cur = 0;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, 0, NULL);
			}
			break;
		case('c'):
MANUAL:
			for (int i = 0; i < pc->count; i++) {
				free(pc->categories[i]);
			}
			free(pc);
			nc_exit_window(wptr_parent);
			nc_exit_window(wptr);
			nc_print_input_footer(stdscr);
			manual_entry = nc_input_string("Enter Category");
			return manual_entry;
		case('\n'):
		case('\r'):
		case(KEY_ENTER):
			break;
		case('q'):
		case(KEY_F(QUIT)):
			goto CLEANUP;
			break;
		}
	}

	if (select >= 0) {
		char *tmp = strdup(pc->categories[select]); // Must be free'd

		for (int i = 0; i < pc->count; i++) {
			free(pc->categories[i]);
		}
		free(pc);
		nc_exit_window(wptr_parent);
		nc_exit_window(wptr);

		return tmp; // Will return NULL if stdup failed, callee checks
	}

CLEANUP:
	for (int i = 0; i < pc->count; i++) {
		free(pc->categories[i]);
	}
	free(pc);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr);
	return NULL;
}

int nc_input_transaction_type(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Choose Expense/Income");
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET + 2, "(1)Expense (2)Income");
	keypad(wptr_input, true);
	int t = 0;

	while(t != '1' && t != '2') {
		t = wgetch(wptr_input);	
		if (t == 'q' || t == KEY_F(QUIT)) {
			return -1;
		}
	}

	nc_exit_window(wptr_input);

	/* Have these statements here to explictly return a 0 or 1 without
	 * having to do an integer/char conversion */
	if (t == '1') {
		return 0;
	}
	if (t == '2') {
		return 1;
	}
	
	return -1;
}

double nc_input_amount(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Amount");
	keypad(wptr_input, true);

	char *str = nc_user_input(MAX_LEN_AMOUNT, wptr_input);
	while (str == NULL) {
		str = nc_user_input(MAX_LEN_AMOUNT, wptr_input);
	}

	nc_exit_window(wptr_input);

	double amount = atof(str);
	free(str);

	return amount;
}

//---------------------------------------------------------------------------//
//--------------------------USER INPUT ABOVE---------------------------------//
//---------------------------------------------------------------------------//

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

struct Categories *list_categories(int month, int year) {
	FILE *fptr = open_csv("r");
	char *line;
	char *token;
	char linebuff[LINE_BUFFER];
	struct Categories *pc = malloc(sizeof(struct Categories));
	pc->count = 0;

	seek_beyond_header(fptr);

	while ((line = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		if (month != atoi(strsep(&line, ","))) {
			goto DUPLICATE;
		}

		seek_n_fields(&line, 1);
		
		if (year != atoi(strsep(&line, ","))) {
			goto DUPLICATE;
		}

		token = strsep(&line, ",");
		
		if (token == NULL) break;
		token[strcspn(token, "\n")] = '\0';

		if (pc->count != 0) { // Duplicate Check
			for (int i = 0; i < pc->count; i++) {
				if (strcmp(pc->categories[i], token) == 0) {
					goto DUPLICATE;
				}
			}
		}

		struct Categories *temp = realloc(pc, sizeof(struct Categories) + 
									((pc->count + 1) * sizeof(char *)));

		if (temp == NULL) {
			exit(1);
		} else {
			pc = temp;
		}

		pc->categories[pc->count] = strdup(token);

		pc->count++;

DUPLICATE:
		memset(linebuff, 0, sizeof(linebuff)); // Reset the Buffer
	}

	fclose(fptr);
	fptr = NULL;
	return pc; // Struct and each index of categories must be free'd
}

struct FlexArr *get_byte_offsets_date(int y, int m) {
	int realloc_counter = 0;	
	struct FlexArr *pbo = malloc(sizeof(*pbo) + (sizeof(int) * REALLOC_FLAG));
	if (pbo == NULL) {
		return NULL;
	}
	pbo->lines = 0;
	FILE *fptr = open_csv("r");
	struct FlexArr *pidx = index_csv();
	struct FlexArr *plines = get_matching_line_nums(fptr, m, y);

	for (int i = 0; i < plines->lines; i++) {
		if (realloc_counter >= REALLOC_FLAG - 1) {
			realloc_counter = 0;
			struct FlexArr *tmp = realloc(pbo, sizeof(*pbo) +
								 ((pbo->lines + REALLOC_FLAG) *
		  						 sizeof(int)));
			if (tmp == NULL) {
				free(pbo);
				return NULL;
			}
			pbo = tmp;
		}
		pbo->data[i] = pidx->data[plines->data[i]];
		pbo->lines++;
		realloc_counter++;
	}
	free(pidx);
	free(plines);
	fclose(fptr);
	return pbo;
}

struct FlexArr *sort_by_date(FILE *fptr, struct FlexArr *pidx,
							 struct FlexArr *plines)
{
	int realloc_counter = 0;
	struct FlexArr *psbd = malloc(sizeof(*psbd) + (sizeof(int) * REALLOC_FLAG));
	if (psbd == NULL) {
		return NULL;
	}
	psbd->lines = 0;
	rewind(fptr);

	for (int i = 0; i < plines->lines; i++) {
		if (realloc_counter >= REALLOC_FLAG - 1) {
			realloc_counter = 0;
			struct FlexArr *tmp = realloc(psbd, sizeof(*psbd) + 
								 ((psbd->lines + REALLOC_FLAG) * 
								 sizeof(int)));
			if (tmp == NULL) {
				free(psbd);
				return NULL;	
			}
			psbd = tmp;
		}

		psbd->data[i] = pidx->data[plines->data[i]];
		psbd->lines++;
		realloc_counter++;
	}

	return psbd;
}

/* Returns an array of integers representing the byte offsets of records 
 * sorted by category */
struct FlexArr *sort_by_category(FILE *fptr, struct FlexArr *pidx, 
								 struct FlexArr *plines, int yr, int mo)
{
	int realloc_counter = 0;
	struct FlexArr *prsc = malloc(sizeof(*prsc) + (sizeof(int) * REALLOC_FLAG));
	prsc->lines = 0;
	rewind(fptr);
	struct Categories *pc = list_categories(mo, yr);
	char linebuff[LINE_BUFFER];
	char *line;
	char *token;

	for (int i = 0; i < pc->count; i++) { // Loop through each category
		for (int j = 0; j < plines->lines; j++) { // Loop through each record

			if (realloc_counter >= REALLOC_FLAG - 1) {
				realloc_counter = 0;
				struct FlexArr *tmp = 
					realloc(prsc, sizeof(*prsc) + ((prsc->lines + REALLOC_FLAG) 
			 				* sizeof(char *)));
				if (tmp == NULL) {
					free(prsc);
					prsc = NULL;
					goto ERR_NULL;
				}
				prsc = tmp;
			}

			fseek(fptr, pidx->data[plines->data[j]], SEEK_SET);
			line = fgets(linebuff, sizeof(linebuff), fptr);
			if (line == NULL) {
				free(prsc);
				prsc = NULL;
				goto ERR_NULL;
			}

			seek_n_fields(&line, 3);

			token = strsep(&line, ",");
			
			if (token == NULL) {
				free(prsc);
				prsc = NULL;
				goto ERR_NULL;
			}

			if (strcmp(token, pc->categories[i]) == 0) {
				prsc->data[prsc->lines] = pidx->data[plines->data[j]];
				prsc->lines++;
				realloc_counter++;
			}
		}
//		prsc->data[prsc->lines] = 0;
//		prsc->lines++;
//		realloc_counter++;
	}

ERR_NULL:
	for (int i = 0; i < pc->count; i++) {
		free(pc->categories[i]);
	}
	free(pc);
	return prsc;
}

/* Adds a record to the CSV on line linetoadd */
void add_csv_record(int linetoadd, struct LineData *ld) {
	FILE *fptr = open_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER];
	char *line;
	int linenum = 0;

	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
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
	} while(line != NULL);
	
	move_temp_to_main(tmpfptr, fptr);
}

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
void nc_add_transaction(int year, int month) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	struct FlexArr *pidx = index_csv();
	nc_print_input_footer(stdscr);

	year > 0 ? (uld->year = year) : (uld->year = nc_input_year());
	month > 0 ? (uld->month = month) : (uld->month = nc_input_month());
	uld->day = nc_input_day(uld->month, uld->year);
	uld->category = nc_select_category(uld->month, uld->year);
	if (uld->category == NULL) {
		goto CLEANUP;
	}
	uld->desc = nc_input_string("Enter Description");
	uld->transtype = nc_input_transaction_type();
	uld->amount = nc_input_amount();

	int result = nc_confirm_record(uld);

	if (result == 0) {
		goto CLEANUP;
	}

	unsigned int resultline = sort_csv_new(uld->month, uld->day, uld->year);

	add_csv_record(resultline, uld);

CLEANUP:
	if (debug) puts("CLEANUP");
	free(pidx);
	free(uld->category);
	free(uld->desc);
	nc_read_setup(uld->year, uld->month, 0);
}

void add_transaction(void) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	struct FlexArr *pidx = index_csv();

	uld->year = input_year();
	uld->month = input_month();
	uld->day = input_day(uld->month, uld->year);
	uld->category = input_category(uld->month, uld->year);
	if (uld->category == NULL) goto CLEANUP;
	uld->desc = input_str_retry("Description:");
	uld->transtype = input_transaction_type();
	uld->amount = input_amount();

	puts("Verify Data is Correct:");
	print_record_vert(uld);
	printf("Y/N:  ");

	int result = confirm_input();
	if (result == 1) {
		if (debug) puts("TRUE");
	} else if (result == 0) {
		if (debug) puts("FALSE");
		goto CLEANUP;
	} else {
		puts("Invalid answer");
		goto CLEANUP;
	}

	unsigned int resultline = sort_csv_new(uld->month, uld->day, uld->year);
	if (resultline < 0) {
		puts("Failed to find where to add this record");
		goto CLEANUP;
	}

	if (debug) printf("Result line: %d\n", resultline);

	add_csv_record(resultline, uld);

CLEANUP:
	if (debug) puts("CLEANUP");
	free(pidx);
	free(uld->category);
	free(uld->desc);
}

struct FlexArr *index_csv(void) {
	struct FlexArr *pidx = 
		malloc(sizeof(struct FlexArr) + 0 * sizeof(int));
	if (pidx == NULL) {
		perror("Failed to allocate memory");
		exit(1);
	}

	pidx->lines = 0;
	FILE *fptr = open_csv("r");
	char linebuff[LINE_BUFFER];

	while (1) {
		char *test = fgets(linebuff, sizeof(linebuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->lines++;
	}

	struct FlexArr *tmp = realloc(pidx, sizeof(*pidx) + (pidx->lines * sizeof(int)));

	if (tmp == NULL) {
		perror("Failed to allocate memory");
		exit(1);
	}
	pidx = tmp;

	rewind(fptr);
	assert(ftell(fptr) == 0);

	for (int i = 0; i < pidx->lines; i++) {
		char *test = fgets(linebuff, sizeof(linebuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->data[i] = ftell(fptr);
	}

	fclose(fptr);
	fptr = NULL;
	return pidx;
}

int nc_confirm_record(struct LineData *ld) {
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
			return 1;
		case('n'):
		case('N'):
		case(KEY_F(QUIT)):
		case('q'):
		case('Q'):
			return 0;
		default:
			break;
		}
	}

	nc_exit_window_key(wptr);
	return 0;
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
		if (cli_mode) {
			ld->year = input_year();
			ld->month = input_month();
			ld->day = input_day(ld->month, ld->year);
		} else {
			ld->year = nc_input_year();
			ld->month = nc_input_month();
			ld->day = nc_input_day(ld->month, ld->year);
			if (nc_confirm_record(ld) <= 0) {
				goto FAIL;
			}
		}

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		delete_csv_record(linetoreplace - 1);
		add_csv_record(sort_csv_new(ld->month, ld->day, ld->year), ld);
		return 0;

	case 2:
		if (cli_mode) {
			ld->category = input_category(ld->month, ld->year);
		} else {
			ld->category = nc_select_category(ld->month, ld->year);
			if (ld->category == NULL) {
				goto FAIL;
			}
		}
		break;
	case 3:
		if (cli_mode) {
			ld->desc = input_str_retry("Enter Description");	
		} else {
			ld->desc = nc_input_string("Enter Description");
		}
		break;
	case 4:
		if (cli_mode) {
			ld->transtype = input_transaction_type();
		} else {
			ld->transtype = nc_input_transaction_type();
			if (ld->transtype == -1) {
				goto FAIL;
			}
		}
		break;
	case 5:
		if (cli_mode) {
			ld->amount = input_amount();
		} else {
			ld->amount = nc_input_amount();
		}
		break;
	default:
		puts("Not a valid choice, exiting");
		return -1;
	}

	if (!cli_mode) {
		if (nc_confirm_record(ld) <= 0) {
			if (field == 2) free(ld->category);
			if (field == 3) free(ld->desc);
			goto FAIL;
		}
	}

	FILE *fptr = open_csv("r");
	FILE *tmpfptr = open_temp_csv();

	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
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
	} while(line != NULL);

	/* move_temp_to_main() closes the file pointers */
	move_temp_to_main(tmpfptr, fptr);

	if (field == 2) free(ld->category);
	if (field == 3) free(ld->desc);
	return 0;

FAIL:
	return -1;
}

/* Populates ld members with tokens from str */
void tokenize_str(struct LineData *ld, char **str) {
	char *token;
	for (int i = 0; i < CSV_FIELDS; i++) {
		token = strsep(str, ",");
		if (token == NULL) break;
		switch(i) {
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
			ld->category = token;
			break;
		case 4:
			token[strcspn(token, "\n")] = '\0';
			ld->desc = token;
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

int *list_records_by_year(FILE *fptr) {
	char linebuff[LINE_BUFFER];
	char *str;
	int *years = calloc(1, sizeof(int));
	int year;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		free(years);
		return NULL;
	}

	str = fgets(linebuff, sizeof(linebuff), fptr); // Read first year into index 0
	if (str == NULL) {
		free(years);
		return NULL;
	}
	seek_n_fields(&str, 2);
	years[i] = atoi(strsep(&str, ",")); // year

	while((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		seek_n_fields(&str, 2);
		year = atoi(strsep(&str, ",")); // year
		if (year != years[i]) {
			i++;
			int *tmp = realloc(years, (i + 1) * sizeof(int));
			if (tmp == NULL) {
				free(years);
				puts("Failed to reallocate memory");
				exit(1);
			}
			years = tmp;
			years[i] = year;
		}
	}
	int *tmp = realloc(years, (i + 2) * sizeof(int));
	if (tmp == NULL) {
		free(years);
		puts("Failed to reallocate memory");
		exit(1);
	}
	years = tmp;
	/* Place a zero at the end of the array to mark the end */
	years[i + 1] = 0; 

	printf("\n");
	return years;
}

int *list_records_by_month(FILE *fptr, int matchyear) {
	char linebuff[LINE_BUFFER];
	char *str;
	int *months = calloc(12, sizeof(int));
	int year;
	int month;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	while((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		month = atoi(strsep(&str, ","));
		seek_n_fields(&str, 1);
		year = atoi(strsep(&str, ","));
		if (matchyear == year) {
			if (months[0] == 0) {
				months[0] = month;
			} else if (month != months[i]) {
				i++;
				months[i] = month;
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

void nc_print_overview_graphs(WINDOW *wptr, int *months, int year) {
	double ratios[12] = {0.0}; // Holds each month's income/expense ratio
	double maxvals[12] = {0.0};
	struct Balances pb_, *pb = &pb_;
	int space = calculate_overview_columns(wptr);
	int mo = 1;

	enum GraphRatios {
		NO_INCOME = -1,
		NO_EXPENSE = -2,
	};

	for (int i = 0; i < 12 && mo <= 12; i++, mo++) {
		if (months[i] == mo) {
			struct FlexArr *pbo = get_byte_offsets_date(year, months[i]);
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

	if (debug) {
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

		for (int j = 0; j < inc_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_GREEN, NULL);
		}

		cur += bar_width;

		for (int j = 0; j < exp_bar_len; j++) {
			mvwchgat(wptr, last_quarter_row(wptr) - 2 - j, cur, bar_width, 
					 A_REVERSE, COLOR_RED, NULL);
		}

		cur += space - bar_width;
	}
}

void nc_print_overview_balances(WINDOW *wptr, int *months, int year) {
	int tmpx = 0;
	int space = calculate_overview_columns(wptr);
	int y = last_quarter_row(wptr) + 2;
	int cur = (getmaxx(wptr) - space * 11) / 2;
	int mo = 1;
	struct Balances pb_, *pb = &pb_;
	for (int i = 0; i < 12 && mo <= 12; i++, mo++) {
		tmpx = 0;
		if (months[i] == mo) {
			struct FlexArr *pbo = get_byte_offsets_date(year, months[i]);
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

	if (debug) {
		wprintw(wptr, "INIT CUR: %d ", cur);
		wprintw(wptr, "SPACE: %d ", space);
		wprintw(wptr, "SPACEx11: %d ", space * 11);
		wprintw(wptr, "MAX X: %d ", getmaxx(wptr));
	}

	for (int i = 0; i < 12; i++) {
		wmove(wptr, y, cur);
		wprintw(wptr, "%s", months[i]);
		cur += space;
	}
}

unsigned int nc_overview_loop(WINDOW *wptr, int *months, int year) {
	unsigned int flag = 0;
	int c;
	int space = calculate_overview_columns(wptr);
	if (space > 0) {
		nc_print_overview_months(wptr);
		nc_print_overview_balances(wptr, months, year);
		nc_print_overview_graphs(wptr, months, year);
		wrefresh(wptr);
		c = wgetch(wptr);
	} else {
		wprintw(wptr, "Terminal is too small");
		wrefresh(wptr);
		c = wgetch(wptr);
	}

	switch(c) {
	case(KEY_RESIZE):
		flag = RESIZE;
		break;
	case(KEY_F(QUIT)):
		flag = QUIT;
		break;
	default:
		flag = NO_SELECT;
		break;
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
	wrefresh(wptr_parent);
	
	FILE *fptr = open_csv("r");
	int *months = list_records_by_month(fptr, year);
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
		nc_read_setup_default();
		break;
	}
}

/* Prints a 2 bar graphs showing the difference between income and expense */
void print_bar_graph(double expense, double income) {
	char income_bar[10];
	char expense_bar[10];

	for (int i = 0; i < sizeof(income_bar); i++) {
		income_bar[i] = '#';
		expense_bar[i] = '#';
	}

	if (income > expense) {
		double diff = expense / income;
		diff *= 10;
		for (int i = 0; i < sizeof(expense_bar); i++) {
			i < (int)diff ? 
			(expense_bar[i] = '#') : (expense_bar[i] = '-');
		}
	} else {
		double diff = income / expense;
		diff *= 10;
		for (int i = 0; i < sizeof(income_bar); i++) {
			i < (int)diff ? 
			(income_bar[i] = '#') : (income_bar[i] = '-');
		}
	}

	printf("Income:  $%.2f [", income);
	for (int i = 0; i < sizeof(income_bar); i++) {
		printf("%c", income_bar[i]);
	}
	printf("]\n");

	printf("Expense: $%.2f [", expense);
	for (int i = 0; i < sizeof(expense_bar); i++) {
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
	int i = 0;
	char linebuff[LINE_BUFFER] = {0};
	FILE *fptr = open_csv("r");
	int *yearsarr;
	bool month_record_exists = false;
	bool year_record_exists = false;

	struct LineData linedata_, *ld = &linedata_;

	yearsarr = list_records_by_year(fptr);
	rewind(fptr);

	while (year_record_exists == false) {
		if (i > 0) puts("No records match that year");
		i = 0;
		useryear = input_year();
		while (yearsarr[i] != 0) {
			if (useryear == yearsarr[i]) {
				year_record_exists = true;
				break;
			}
			i++;
		}
	}

	i = 0;
	free(yearsarr);
	yearsarr = NULL;
	rewind(fptr);

	int *monthsarr = list_records_by_month(fptr, useryear);
	i = 0;

	while (month_record_exists == false) {
		if (i > 0) puts("No records match that month");
		i = 0;
		usermonth = input_month();
		while (monthsarr[i] != 0) {
			if (usermonth == monthsarr[i]) {
				month_record_exists = true;
				break;
			}
			i++;
		}
	}
	free(monthsarr);
	monthsarr = NULL;
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
		tokenize_str(ld, &line);
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
	}

	if (month_record_exists) {
		print_bar_graph(expense, income);
	} else {
		printf("No records match the entered date\n");
	}
	fclose(fptr);
	fptr = NULL;
}

int nc_read_select_year(WINDOW *wptr, FILE *fptr) {
	rewind(fptr);

	keypad(wptr, true);	

	int *years_arr = list_records_by_year(fptr);
	if (years_arr == NULL) {
		return -1;
	}
	int selected_year = 0;
	int print_y = 1;
	int print_x = 2;

	box(wptr, 0, 0);
	wrefresh(wptr);
	wmove(wptr, print_y, print_x);

	int scr_idx = 0;
	int i = 0;
	int flag = -1;
	while (years_arr[i] != 0) {
		if (years_arr[i] == CURRENT_YEAR) {
			flag = i;
		}
		wprintw(wptr, "%d ", years_arr[i]);
		wrefresh(wptr);
		i++; // i stores the number of years with records
	}

	/* Initially highlight the current year and move cursor to it */
	int init_rv_x = print_x;
	if (flag > 0) {
		init_rv_x += (4 * flag) + flag;
		scr_idx = flag;
	}
	
	mvwchgat(wptr, print_y, init_rv_x, 4, A_REVERSE, 0, NULL);

	wrefresh(wptr);

	int c = 0;
	int temp_x;
	while (c != '\r' && c != '\n' && c != KEY_F(QUIT)) {
		c = wgetch(wptr);
		temp_x = getcurx(wptr);
		switch(c) {
		case('h'):
		case(KEY_LEFT):
			if (temp_x - 5 >= print_x) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x - 5, 4, A_REVERSE, 0, NULL);
				wrefresh(wptr);
				scr_idx--;
			}
			break;
		case('l'):
		case(KEY_RIGHT):
			if (temp_x + 5 <= (i * 4) + i) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x + 5, 4, A_REVERSE, 0, NULL);
				wrefresh(wptr);
				scr_idx++;
			}
			break;
		case('a'):
		case(KEY_F(ADD)):
			free(years_arr);
			return ADD;
		case(KEY_RESIZE):
			free(years_arr);
			return RESIZE;
		case('\n'):
		case('\r'):
			selected_year = years_arr[scr_idx];
			break;
//		case('o'):
//		case(KEY_F(OVERVIEW)):
//			selected_year = years_arr[scr_idx];
//			break;
		case('q'):
		case(KEY_F(QUIT)):
			free(years_arr);
			return 0;
		default: 
			break;
		}
	}

	wrefresh(wptr);

	free(years_arr);
	years_arr = NULL;

	return selected_year;
}

int nc_read_select_month(WINDOW *wptr, FILE* fptr, int year) {
	rewind(fptr);

	int *months_arr = list_records_by_month(fptr, year);
	int selected_month = 0;
	int monlen = strlen(months[0]);

	int temp_y = 0;
	int scr_idx = 0;
	int cur_idx = 0;

	wmove(wptr, BOX_OFFSET, BOX_OFFSET);
	for (int i = 0; i < 12; i++) {
		temp_y = getcury(wptr);
		if (months_arr[i] != 0) {
			wmove(wptr, temp_y, BOX_OFFSET);
			wprintw(wptr, "%s\n", months[months_arr[i] - 1]);
			scr_idx++;
		}
	}

	wmove(wptr, BOX_OFFSET, BOX_OFFSET);
	wchgat(wptr, monlen, A_REVERSE, 0, NULL);
	box(wptr, 0, 0);
	wrefresh(wptr);

	int c = 0;
	while (c != '\r' && c != '\n' && c != KEY_F(QUIT) && c != KEY_RESIZE) {
		c = wgetch(wptr);
		temp_y = getcury(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (temp_y - BOX_OFFSET + 1 < scr_idx) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y + 1, BOX_OFFSET, monlen, A_REVERSE, 0, NULL);
				wrefresh(wptr);
				cur_idx++;
			}
			break;
		case('k'):
		case(KEY_UP):
			if (temp_y - 1 >= BOX_OFFSET) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y - 1, BOX_OFFSET, monlen, A_REVERSE, 0, NULL);
				wrefresh(wptr);
				cur_idx--;
			}
			break;
		case(KEY_RESIZE):
			/* 
			 * This RESIZE macro has to be greater than 12, which I have no
			 * intention of changing. I hate this. However--I'm lazy. Plus,
			 * all of the resizes in this program suck.
			 */
			selected_month = RESIZE;
			break;
		case('\n'):
		case('\r'):
			selected_month = months_arr[cur_idx];
			wrefresh(wptr);
			break;
		case(KEY_F(QUIT)):
			nc_exit_window(wptr);
			free(months_arr);
			months_arr = NULL;
			return -1;
		default: 
			break;
		}
	}

	free(months_arr);
	months_arr = NULL;

	return selected_month;
}

struct FlexArr *get_matching_line_nums(FILE *fptr, int month, int year) {
	rewind(fptr);
	struct FlexArr *lines = 
		malloc(sizeof(struct FlexArr) + (REALLOC_FLAG * sizeof(int)));
	if (lines == NULL) {
		exit(1);
	}

	lines->lines = 0;

	int linenumber = 0;
	int line_month, line_year;
	int realloc_counter = 0;
	char linebuff[LINE_BUFFER];
	char *str;

	if (seek_beyond_header(fptr) == -1) {
		perror("Unable to read header");
		free(lines);
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
			if (realloc_counter == REALLOC_FLAG - 1) {
				realloc_counter = 0;
				void *temp = realloc(lines, sizeof(struct FlexArr) + 
					((lines->lines) + REALLOC_FLAG) * sizeof(int));
				if (temp == NULL) {
					free(lines);
					exit(1);
				}
				lines = temp;
			}
			lines->data[lines->lines] = linenumber;
			lines->lines++;	
			realloc_counter++;
		}
		linenumber++;
	}

	/* Shrink back down if oversized alloc */
	if (lines->lines % REALLOC_FLAG != 0) {	
		void *temp = realloc(lines, sizeof(struct FlexArr) + 
					   (lines->lines * sizeof(int)));
		if (temp == NULL) {
			free(lines);
			exit(1);
		}
		lines = temp;
	}

	if (lines->lines > 0) {
		return lines;
	} else {
		free(lines);
		lines = NULL;
		return NULL;
	}
	return NULL;
}


/* 
 * Prints record from ld, formatting in columns from cw, to a window pointed
 * to by wptr, at a Y-coordinate of y. Truncates desc and category strings
 * if the window is too small.
 */
void nc_print_record_hr(
	WINDOW *wptr, 
	struct ColWidth *cw, 
	struct LineData *ld, 
	int y) {

	char *etc = ". ";
	int x = 0;
	wmove(wptr, y, x);
	if (getmaxx(wptr) < MIN_COLUMNS) {
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
	if (getmaxx(wptr) < MIN_COLUMNS) {
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
	mvwchgat(wptr, 1, 0, -1, A_REVERSE, 0, NULL);
	keypad(wptr, true);
	int select = 1;
	int c = 0;

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Field to Edit");
	wrefresh(wptr);
	while(c != KEY_F(QUIT) && c != '\n' && c != '\r') {
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
				mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
			} else {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select = 1;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
			}
			break;
		case('k'):
		case(KEY_UP):
			if (select - 1 > 0) {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select--;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
			} else {
				mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
				select = INPUT_WIN_ROWS - BOX_OFFSET;
				mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
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

void nc_edit_transaction(unsigned int linenum) {
	struct LineData linedata, *ld = &linedata;
	struct FlexArr *pidx = index_csv();

	WINDOW *wptr_edit = create_input_subwindow();
	FILE *fptr = open_csv("r+");
	fseek(fptr, pidx->data[linenum], SEEK_SET);

	char linebuff[LINE_BUFFER];
	char *line = fgets(linebuff, sizeof(linebuff), fptr);

	if (line == NULL) {
		exit(1);
	}

	tokenize_str(ld, &line);

	struct LineData *pLd = malloc(sizeof(*ld));

	if (pLd == NULL) {
		perror("Failed to allocate memory");
		free(pidx);
		fclose(fptr);
		fptr = NULL;
		return;
	}

	memcpy(pLd, ld, sizeof(*ld));

	nc_print_record_vert(wptr_edit, ld, BOX_OFFSET);

	mvwprintw(wptr_edit, INPUT_WIN_ROWS - BOX_OFFSET, BOX_OFFSET, "%s", "Delete");

	box(wptr_edit, 0, 0);
	wrefresh(wptr_edit);

	int field_to_edit = nc_select_field_to_edit(wptr_edit);

	fclose(fptr);

	nc_exit_window(wptr_edit);

	switch(field_to_edit) {
	case 0:
		break;
	case 1:
		nc_print_input_footer(stdscr);
		edit_csv_record(linenum + 1, pLd, 1);
		break;
	case 2:
		edit_csv_record(linenum + 1, pLd, 2);
		break;
	case 3:
		nc_print_input_footer(stdscr);
		edit_csv_record(linenum + 1, pLd, 3);
		break;
	case 4:
		nc_print_input_footer(stdscr);
		edit_csv_record(linenum + 1, pLd, 4);
		break;
	case 5:
		nc_print_input_footer(stdscr);
		edit_csv_record(linenum + 1, pLd, 5);
		break;
	case 6:
		nc_print_input_footer(stdscr);
		if (nc_confirm_input() == 1) {
			if (delete_csv_record(linenum + 1) == 0) {
				nc_message("Successfully Deleted");
			}
		}
		break;
	default:
		return;
	}

	free(pLd);
	pLd = NULL;
	free(pidx);
	pidx = NULL;
}

void nc_print_debug_line(WINDOW *wptr, int line) {
	mvwhline(wptr, getmaxy(wptr) - 1, 1, 0, getmaxx(wptr) - 2);
	mvwprintw(wptr, getmaxy(wptr) - 1, getmaxx(wptr) - 20, "FILE POS: %d", line);
	wrefresh(wptr);
}

void calculate_balance(struct Balances *pb, struct FlexArr *pbo) {
	FILE *fptr = open_csv("r");
	pb->income = 0.0;
	pb->expense = 0.0;
	int type;
	char linebuff[LINE_BUFFER];
	char *line;

	for (int i = 0; i < pbo->lines; i++) {
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

void nc_print_balances_text(WINDOW *wptr, struct FlexArr *psc) {
	struct Balances pb_, *pb = &pb_;
	calculate_balance(pb, psc);
	int total_len = intlen(pb->income) + intlen(pb->expense) + strlen("Expenses: $.00 Income: $.00");
	mvwprintw(wptr, 0, getmaxx(wptr) / 2 - total_len / 2, 
		   	  "Expenses: $%.2f Income: $%.2f", pb->expense, pb->income);
	wrefresh(wptr);
}

/*
 * Creates a sub window inside of wptr to display a line-by-line format of
 * the selected record at index i of pidx->data. Following the format style
 * from edit_transaction()
 */
void show_detail_subwindow(char *line) {
	WINDOW *wptr_detail = create_input_subwindow();
	box(wptr_detail, 0, 0);
	mvwxcprintw(wptr_detail, 0, "Details");
	struct LineData linedata_, *ld = &linedata_;
	tokenize_str(ld, &line);
	nc_print_record_vert(wptr_detail, ld, BOX_OFFSET);
	nc_exit_window_key(wptr_detail);
}

/*
 * After the detail sub window is closed, there is a gap of invisible lines
 * where the window was. This loops through each line and changes it attr
 * back to A_NORMAL and then re-reverse-video's the original line. Cursor
 * position is NOT changed.
 */
void refresh_on_detail_close(WINDOW *wptr, WINDOW *wptr_parent, int n) {
	int temp_y, temp_x;
	getyx(wptr, temp_y, temp_x);
	wmove(wptr, 0, 0);

	for (int i = 0; i < n; i++) {
		mvwchgat(wptr, i, 0, -1, A_NORMAL, 0, NULL);
	}

	wmove(wptr, temp_y, temp_x);
	wchgat(wptr, -1, A_REVERSE, 0, NULL); 
	mvwvline(wptr_parent, 1, 0, 0, getmaxy(wptr_parent) - 2);
	mvwvline(wptr_parent, 1, getmaxx(wptr_parent) - 1, 0, getmaxy(wptr_parent) - 2);
	wrefresh(wptr_parent);
}

void nc_scroll_prev(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw) {
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}
	struct LineData linedata_, *ld = &linedata_;
	tokenize_str(ld, &line_str);

	wmove(wptr, 0, 0);
	winsertln(wptr);
	nc_print_record_hr(wptr, cw, ld, 0);
}

void nc_scroll_next(long b, FILE *fptr, WINDOW *wptr, struct ColWidth *cw) {
	fseek(fptr, b, SEEK_SET);
	char linebuff[LINE_BUFFER];
	char *line_str;
	line_str = fgets(linebuff, sizeof(linebuff), fptr);
	if (line_str == NULL) {
		return;
	}
	struct LineData linedata_, *ld = &linedata_;
	tokenize_str(ld, &line_str);

	wmove(wptr, 0, 0);
	wdeleteln(wptr);
	wmove(wptr, getmaxy(wptr) - 1, 0);
	nc_print_record_hr(wptr, cw, ld, getmaxy(wptr) - 1);
}

/*
 * Main read loop. Populates member values in the struct pointed to 
 * by sr on a MenuKeys press. Prints lines by seeking FPI to the byte offset
 * of psc->data. Sort occurs before this function in nc_read_setup.
 */
void nc_read_loop(WINDOW *wptr_parent, WINDOW *wptr, FILE *fptr, 
				  struct SelRecord *sr, struct FlexArr *psc)
{
	struct ColWidth cw_, *cw = &cw_;
	struct LineData ld_, *ld = &ld_;
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	int displayed = 0;
	char *line_str;
	char linebuff[LINE_BUFFER];

	calculate_columns(cw, max_x + BOX_OFFSET);
	nc_print_balances_text(wptr_parent, psc);
	nc_print_read_footer(stdscr);

	/* Print initial lines based on screen size */
	for (int i = 0; i < max_y && displayed < psc->lines; i++) {
		fseek(fptr, psc->data[i], SEEK_SET);
		line_str = fgets(linebuff, sizeof(linebuff), fptr);
		tokenize_str(ld, &line_str);
		nc_print_record_hr(wptr, cw, ld, i);
		displayed++;
	}

	/* Move cursor to first line of data and set that line to reverse video */
	int select = 0; // To keep track of selection for indexing
	int cur_y = 0; // Keep track of cursor position in window
	wmove(wptr, 0, 0);
	mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL); 

	if (debug) {
		curs_set(1);
	}

	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wptr);
		if (debug) {
			nc_print_debug_line(wptr_parent, psc->data[select]);
		}
		c = wgetch(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (select + 1 < psc->lines) {
				mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
				cur_y++;
				select++;

				if (displayed < psc->lines && cur_y == max_y) {
					nc_scroll_next(psc->data[select], fptr, wptr, cw);
					cur_y = getcury(wptr);
				}
				mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
			}
			break;

		case('k'):
		case(KEY_UP):
			if (select - 1 >= 0) {
				mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
				cur_y--;
				select--;

				if (cur_y < 0) {
					cur_y = -1;
				}
				
				if (select >= 0 && displayed < psc->lines && cur_y == -1) {
					nc_scroll_prev(psc->data[select], fptr, wptr, cw);
					cur_y = getcury(wptr);
				}
				mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
			}
			break;

		case('\n'):
		case('\r'):
			fseek(fptr, psc->data[select], SEEK_SET);
			char *line = fgets(linebuff, sizeof(linebuff), fptr);
			show_detail_subwindow(line);
			refresh_on_detail_close(wptr, wptr_parent, displayed);
			c = 0;
			break;

		case(KEY_NPAGE): // PAGE DOWN
			for(int i = 0; i < 10; i++) {
				if (select + 1 < psc->lines) {
					mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
					cur_y++;
					select++;

					if (displayed < psc->lines && cur_y == max_y) {
						nc_scroll_next(psc->data[select], fptr, wptr, cw);
						cur_y = getcury(wptr);
					}
					mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
				} else {
					break;
				}
			}
			break;

		case(KEY_PPAGE): // PAGE UP
			for (int i = 0; i < 10; i++) {
				if (select - 1 >= 0) {
					mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
					cur_y--;
					select--;

					if (cur_y < 0) cur_y = -1;
					
					if (displayed < psc->lines && cur_y == -1 && select >= 0) {
						nc_scroll_prev(psc->data[select], fptr, wptr, cw);
						cur_y = getcury(wptr);
					}
					mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
				} else {
					break;
				}
			}
			break;

		case(KEY_END):
			while(select + 1 < psc->lines) {
				mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
				cur_y++;
				select++;

				if (displayed < psc->lines && cur_y == max_y) {
					nc_scroll_next(psc->data[select], fptr, wptr, cw);
					cur_y = getcury(wptr);
				}
				mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
			}
			break;

		case(KEY_HOME):
			while(select - 1 >= 0) {
				mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
				cur_y--;
				select--;

				if (cur_y < 0) cur_y = -1;
				
				if (displayed < psc->lines && cur_y == -1 && select >= 0) {
					nc_scroll_prev(psc->data[select], fptr, wptr, cw);
					cur_y = getcury(wptr);
				}
				mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
			}
			break;

		case('A'):
		case('a'):
		case(KEY_F(ADD)):
			sr->flag = ADD;
			sr->index = psc->data[select];
			return;
		case('E'):
		case('e'):
		case(KEY_F(EDIT)):
			sr->flag = EDIT;
			sr->index = psc->data[select];
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

void nc_read_cleanup(WINDOW *wp, WINDOW *wc, struct FlexArr *psc,
					 struct FlexArr *plines, struct FlexArr *pidx,
					 FILE *fptr)
{
	nc_exit_window(wp);
	nc_exit_window(wc);
	free(psc);
	free(plines);
	free(pidx);
	fclose(fptr);
}

void nc_read_setup_default() {
	nc_read_setup(0, 0, 0);
}

void nc_read_setup_year(int sel_year) {
	nc_read_setup(sel_year, 0, 0);
}

void nc_read_setup(int sel_year, int sel_month, int sort) {
	/* LINES - 1 to still display the footer under wptr_parent */
	nc_print_main_menu_footer(stdscr);
	if (debug) {
		nc_print_debug_flag(stdscr);
	}
	refresh();

	struct FlexArr *pidx = index_csv();
	struct SelRecord sr_ , *sr = &sr_;
	sr->flag = -1;
	FILE *fptr = open_csv("r");

	WINDOW *wptr_parent = newwin(LINES - 1, 0, 0, 0);
	if (wptr_parent == NULL) {
		perror("Failed to create ncurses window");
		fclose(fptr);
		return;
	}

	int max_y, max_x;
	getmaxyx(wptr_parent, max_y, max_x);

	WINDOW *wptr_data;
	struct FlexArr *plines;

	wptr_data = create_lines_subwindow(max_y - 1, max_x, 1, BOX_OFFSET);
	wrefresh(wptr_data);

	if (!sel_year) sel_year = nc_read_select_year(wptr_parent, fptr);
	if (sel_year == 0) {
		goto SELECT_DATE_FAIL;
	} else if (sel_year == RESIZE) {
		sr->flag = 0;
		goto SELECT_DATE_FAIL;
	} else if (sel_year == -1) {
		mvwxcprintw(wptr_parent, max_y / 2, 
			  "No records exist, add (F1) to get started");
		wgetch(wptr_parent);
		goto SELECT_DATE_FAIL;
	}

	if (!sel_month) {
		sel_month = nc_read_select_month(wptr_parent, fptr, sel_year);
	}
	if (sel_month < 0) {
		sr->flag = QUIT;
		goto SELECT_DATE_FAIL;
	} else if (sel_month == RESIZE) {
		sr->flag = RESIZE;
		sel_month = 0;
		goto SELECT_DATE_FAIL;
	}
	wclear(wptr_parent);

	plines = get_matching_line_nums(fptr, sel_month, sel_year);
	if (plines == NULL) {
		free(pidx);
		fclose(fptr);
		return;
	}

	struct FlexArr *psc;
	char *sort_text;

	switch(sort) {
	case(DATE):
		psc = sort_by_date(fptr, pidx, plines);	
		sort_text = "Date";
		break;
	case(CATEGORY):
		psc = sort_by_category(fptr, pidx, plines, sel_year, sel_month);
		sort_text = "Category";
		break;
	default:
		psc = sort_by_date(fptr, pidx, plines);	
		sort_text = "Date";
		break;
	}

	print_column_headers(wptr_parent, BOX_OFFSET);
	box(wptr_parent, 0, 0);
	mvwprintw(wptr_parent, 0, BOX_OFFSET, "%d %s", sel_year, months[sel_month - 1]);
	mvwprintw(wptr_parent, 0, 
			  max_x - strlen(sort_text) - strlen("Sort By: ") - BOX_OFFSET, 
			  "Sort By: %s", sort_text);
	wrefresh(wptr_parent);

	nc_read_loop(wptr_parent, wptr_data, fptr, sr, psc);

	free(psc);
	psc = NULL;
	free(plines);
	plines = NULL;

SELECT_DATE_FAIL:
	free(pidx);
	pidx = NULL;
	fclose(fptr);
	fptr = NULL;

	nc_exit_window(wptr_data);
	nc_exit_window(wptr_parent);
//	nc_print_main_menu_footer(stdscr);

	switch(sr->flag) {
	case(NO_SELECT): // 0 is no selection
		nc_read_setup_default();
		break;
	case(ADD):
		nc_print_input_footer(stdscr);
		nc_add_transaction(sel_year, sel_month);
		break;
	case(EDIT):
		nc_print_quit_footer(stdscr);
		nc_edit_transaction(boff_to_linenum(sr->index));
		nc_read_setup(sel_year, sel_month, 0);
		break;
	case(READ):
		nc_read_setup_default();
		break;
	case(QUIT):
		break;
	case(SORT):
		(sort == 0) ? (sort = 1) : (sort = 0);
		nc_read_setup(sel_year, sel_month, sort);
		break;
	case(OVERVIEW):
		nc_overview_setup(sel_year);
		break;
	case(RESIZE):
		while (test_terminal_size() == -1) {
			getch();
		}
		if (sel_month) {
			nc_read_setup(sel_year, sel_month, 0);
		} else {
			nc_read_setup_year(sel_year);
		}
		break;
	default:
		break;
	}
	refresh();
}

int move_temp_to_main(FILE *tempfile, FILE *mainfile) {
	if (fclose(mainfile) == -1) {
		puts("Failed to close main file");
		return -1;
	} else {
		mainfile = NULL;
	}
	if (fclose(tempfile) == -1) {
		puts("Failed to close temporary file");
		return -1;
	} else {
		tempfile = NULL;
	}
	if (rename(CSV_DIR, CSV_BAK_DIR) == -1) {
		puts("Failed to move main file");	
		return -1;
	}
	if (rename(TEMP_FILE_DIR, CSV_DIR) == -1) {
		puts("Failed to move temporary file");	
		return -1;
	}
	return 0;
}

int delete_csv_record(int linetodelete) {
	if (linetodelete == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	FILE *fptr = open_csv("r");
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
	} while(line != NULL);
	move_temp_to_main(tmpfptr, fptr);
	return 0;
}

void edit_transaction(void) {
	int target;
	int humantarget;
	struct LineData linedata, *ld = &linedata;

	FILE *fptr = open_csv("r+");
	assert(ftell(fptr) == 0);

	legacy_read_csv();
	
	struct FlexArr *pcsvindex = index_csv();

	do {
		puts("Enter line number");
		humantarget = input_n_digits(sizeof(long long) + 1, 2);
	} while (humantarget <= 0 || humantarget > pcsvindex->lines);

	target = humantarget - 1;

	if (debug) {
		printf("TARGET: %d\n", target);
		printf("TARGET OFFSET: %d\n", pcsvindex->data[target]);
	}

	fseek(fptr, pcsvindex->data[target], SEEK_SET);

	if (debug) {
		printf("COMMANDED SEEK OFFSET: %d\n", pcsvindex->data[target]);
	}
	
	char linebuff[LINE_BUFFER];
	char *str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		puts("failed to read line");
		exit(1);
	}

	tokenize_str(ld, &str);

	struct LineData *pLd = malloc(sizeof(*ld));
	if (pLd == NULL) {
		puts("Failed to allocate memory");
		free(pcsvindex);
		fclose(fptr);
		fptr = NULL;
		exit(1);
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

	free(pLd);
	pLd = NULL;
	free(pcsvindex);
	pcsvindex = NULL;
	fclose(fptr);
	fptr = NULL;
}

int nc_main_menu(WINDOW *wptr) {
	nc_print_welcome(wptr);
	nc_print_main_menu_footer(wptr);
	if (debug) {
		nc_print_debug_flag(wptr);
	}
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(QUIT)) {
		nc_print_welcome(wptr);
		nc_print_main_menu_footer(wptr);
		if (debug) {
			nc_print_debug_flag(wptr);
		}
		wrefresh(wptr);
		c = getch();
		switch(c) {
		case('a'):
		case (KEY_F(ADD)): // Add
			wclear(wptr);
			nc_add_transaction(0, 0);
			break;
		case('e'):
		case(KEY_F(EDIT)): // Edit
			wclear(wptr);
			break;
		case('r'):
		case(KEY_F(READ)):
			wclear(wptr);
			nc_read_setup_default();
			break;
		case('q'):
		case(KEY_F(QUIT)): // Quit
			wclear(wptr);
			return 1;
		case(KEY_RESIZE):
			wclear(wptr);
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

int main(int argc, char **argv) {
	FILE *fptr = open_csv("a"); // Make sure the CSV exists
	fseek(fptr, 0, SEEK_END);
	if (ftell(fptr) == 0) {
		fputs("month,day,year,category,description,transtype,value\n", fptr);
	}

	debug = false;
	cli_mode = false;

	if (argc > 0) {
		for (int i = 1; i < argc; i++) { // Args start at 1
			if (strcmp(argv[i], "-d") == 0) {
				debug = true;
			}
			if (strcmp(argv[i], "-c") == 0) {
				cli_mode = true;
			}
		}
	}
	
	fclose(fptr);

	if (cli_mode == false) {
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
}
