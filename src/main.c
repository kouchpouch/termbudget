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

#define CSV_DIR "./data.csv"
#define CSV_BAK_DIR "./data.csv.bak"
#define CSV_FIELDS 7
#define TEMP_FILE_DIR "./tmp.txt"
#define MIN_INPUT_CHAR 2
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
#define REALLOC_THRESHOLD 64
#define INPUT_MSG_Y_OFFSET 2

#define MAX_LEN_AMOUNT 11
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4

#define CURRENT_YEAR 2026 // FIX This is to not be hard coded

static bool debug;
static bool cli_mode;

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
	float amount;
	int linenum;
	int offset;
};

struct DynamicInts {
	int lines;
	int data[];
};

struct Categories {
	int count;
	char *categories[];
};

struct SelectedRecord {
	int flag;
	int index;
};

void nc_read_setup(int sel_year, int sel_month);
int nc_confirm_record(struct LineData *ld);
void print_record_hr(WINDOW *wptr, struct ColumnWidth *cw, struct LineData *ld, int y);
void print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off);
struct Categories *list_categories(int month, int year);
struct DynamicInts *index_csv();
int move_temp_to_main(FILE* tempfile, FILE* mainfile);
int delete_csv_record(int linetodelete);

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
		mvwxcprintw(wptr, max_y - 2, "Input is too long");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		goto FAIL;
	}

	if (length < 1) {
		mvwxcprintw(wptr, max_y - 2, "Input is too short");
		goto FAIL;
	}

	if (strstr(buffer, ",")) {
		mvwxcprintw(wptr, max_y - 2, "No commas allowed");
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
		if (debug == true) {
			mvwxcprintw(wptr, getmaxy(wptr), "nc_user_input() failed");
		}
	}

	if ((int)strlen(str) < min_len) {
		mvwxcprintw(wptr, getmaxy(wptr) - 2, "Input is too short");
		wrefresh(wptr);
		free(str);
		str = NULL;
		return -1;
	}

	for (int i = 0; i < (int)strlen(str); i++) {
		if (!isdigit(*(str + i)) && (*(str + i) != '\n' || *(str + i) != '\0')) {
			mvwxcprintw(wptr, getmaxy(wptr) - 2, 
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

float input_amount(void) {
	puts("$ Amount:");
	char *str = user_input(MAX_LEN_AMOUNT);
	while (str == NULL) {
		puts("Invalid");
		str = user_input(MAX_LEN_AMOUNT);
	}
	float amount = atof(str);
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
		mvwxcprintw(wptr_input, getmaxy(wptr_input) - 2, "Invalid Month");
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
			mvwxcprintw(wptr_input, getmaxy(wptr_input) - 2, "Not a valid day");
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

char *nc_select_category(int month, int year) {
	struct Categories *pc = list_categories(month, year);
	int new_win_y = pc->count + 2;
	int new_win_x = getmaxx(stdscr) >= 64 ? 64 : getmaxx(stdscr);
	int new_begin_y = getmaxy(stdscr) / 2 - (pc->count + 1) / 2;
	int new_begin_x = getmaxx(stdscr) / 2 - new_win_x / 2;
	WINDOW *wptr_input = newwin(new_win_y, new_win_x, new_begin_y, new_begin_x);
	box(wptr_input, 0, 0);
	mvwxcprintw(wptr_input, 0, "Select Category");
	mvwxcprintw(wptr_input, getmaxy(wptr_input) - 1, "Press 'c' to Create a Category");
	wrefresh(wptr_input);

	if (pc->count == 0) {
		goto MANUAL;
	}

	int linenum = 1;
	if (pc->count > MIN_ROWS) { // FIX Add scrolling to this one too
		mvwxcprintw(wptr_input, 1, "Not enough rows");
	} else {
		for (int i = 0; i < pc->count; i++) {
			mvwxcprintw(wptr_input, linenum, pc->categories[i]);
			linenum++;
		}
	}

	int x_off = 2;
	int nx = new_win_x - (x_off * 2);
	mvwchgat(wptr_input, 1, x_off, new_win_x - (x_off * 2), A_REVERSE, 0, NULL);

	int cur = 1;
	int select = -1;
	int c = 0;

	char *manual_entry;
	keypad(wptr_input, true);

	while (c != '\n' && c != '\r') {
		wrefresh(wptr_input);
		c = wgetch(wptr_input);
		switch(c) {
			case('j'):
			case(KEY_DOWN):
				if (cur + 1 <= pc->count) {
					mvwchgat(wptr_input, cur, x_off, nx, A_NORMAL, 0, NULL);
					cur++;
					mvwchgat(wptr_input, cur, x_off, nx, A_REVERSE, 0, NULL);
				}
				break;
			case('k'):
			case(KEY_UP):
				if (cur - 1 >= 1) {
					mvwchgat(wptr_input, cur, x_off, nx, A_NORMAL, 0, NULL);
					cur--;
					mvwchgat(wptr_input, cur, x_off, nx, A_REVERSE, 0, NULL);
				}
				break;
			case('c'):
			MANUAL:
				for (int i = 0; i < pc->count; i++) {
					free(pc->categories[i]);
				}
				free(pc);
				nc_exit_window(wptr_input);
				manual_entry = nc_input_string("Enter Category");
				if (manual_entry == NULL) {
					return NULL;
				}
				return manual_entry;
			case('\n'):
			case('\r'):
			case(KEY_ENTER):
				select = cur - 1;
				break;
			case('q'):
			case(KEY_F(4)):
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
		nc_exit_window(wptr_input);

		return tmp; // Will return NULL if stdup failed, callee checks
	}

CLEANUP:
	for (int i = 0; i < pc->count; i++) {
		free(pc->categories[i]);
	}
	free(pc);
	nc_exit_window(wptr_input);
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
		if (t == 'q' || t == KEY_F(4)) {
			return -1;
		}
	}

	nc_exit_window(wptr_input);

	/* Have these statements here to explictly return a 0 or 1 without
	 * having to do an integer/char conversion */
	if (t == '1') return 0;
	if (t == '2') return 1;
	
	return -1;
}

float nc_input_amount(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Amount");
	keypad(wptr_input, true);

	char *str = nc_user_input(MAX_LEN_AMOUNT, wptr_input);
	while (str == NULL) {
		str = nc_user_input(MAX_LEN_AMOUNT, wptr_input);
	}

	nc_exit_window(wptr_input);

	float amount = atof(str);
	free(str);

	return amount;
}

//---------------------------------------------------------------------------//
//--------------------------USER INPUT ABOVE---------------------------------//
//---------------------------------------------------------------------------//

FILE *open_csv(char *mode) {
	FILE *fptr = fopen(CSV_DIR, mode);
	if (fptr == NULL) {
		puts("Failed to open file");
		exit(1);
	} else {
		return fptr;
	}
}

FILE *open_temp_csv(void) {
	FILE *tmpfptr = fopen("tmp.txt", "w+");
	if (tmpfptr == NULL) {
		puts("Failed to open file");
		exit(1);
	}
	return tmpfptr;
}

/* Reads the header of the file until a newline is found */
int seek_beyond_header(FILE *fptr) {
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

struct Categories *list_categories(int month, int year) {
	FILE *fptr = open_csv("r");
	char *line;
	char *token;
	char buff[LINE_BUFFER];
	struct Categories *pc = malloc(sizeof(struct Categories));
	pc->count = 0;

	seek_beyond_header(fptr);

	while ((line = fgets(buff, sizeof(buff), fptr)) != NULL) {
		if (month != atoi(strsep(&line, ","))) {
			goto DUPLICATE;
		}

		strsep(&line, ","); // Skip the day
		
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
		memset(buff, 0, sizeof(buff)); // Reset the Buffer
	}

	fclose(fptr);
	fptr = NULL;
	return pc; // Struct and each index of categories must be free'd
}

/* Adds a record to the CSV on line linetoadd */
void add_csv_record(int linetoadd, struct LineData *ld) {
	FILE *fptr = open_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char buff[LINE_BUFFER];
	char *line;
	int linenum = 0;

	do {
		line = fgets(buff, sizeof(buff), fptr);
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
	if (move_temp_to_main(tmpfptr, fptr) == 0) {
		puts("Complete");
	}
}

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
void nc_add_transaction(int year, int month) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	struct DynamicInts *pidx = index_csv();

	FILE *fptr = open_csv("r+");
	fseek(fptr, 0L, SEEK_END);

	year > 0 ? (uld->year = year) : (uld->year = nc_input_year());
	month > 0 ? (uld->month = month) : (uld->month = nc_input_month());
	uld->day = nc_input_day(uld->month, uld->year);
	uld->category = nc_select_category(uld->month, uld->year);
	uld->desc = nc_input_string("Enter Description");
	uld->transtype = nc_input_transaction_type();
	uld->amount = nc_input_amount();

	int result = nc_confirm_record(uld);

	if (result == 0) {
		goto CLEANUP;
	}

	int resultline = sort_csv(uld->month, uld->day, uld->year);

	if (resultline < 0) {
		if (debug == true) {
			printw("Something has gone horribly wrong");
			refresh();
			getch();	
		}
		goto CLEANUP;
	}

	add_csv_record(resultline, uld);

CLEANUP:
	if (debug == true) puts("CLEANUP");
	free(pidx);
	free(uld->category);
	free(uld->desc);
	fclose(fptr);
	nc_read_setup(uld->year, uld->month);
}

void add_transaction(void) {
	struct LineData userlinedata_, *uld = &userlinedata_;
	struct DynamicInts *pidx = index_csv();
	int year, month, day, resultline, transaction;
	char *categorystr;
	char *descstr;
	float amount;

	FILE *fptr = open_csv("r+");
	fseek(fptr, 0L, SEEK_END);

	year = input_year();
	month = input_month();
	day = input_day(month, year);
	categorystr = input_category(month, year);
	if (categorystr == NULL) goto CLEANUP;
	descstr = input_str_retry("Description:");
	transaction = input_transaction_type();
	amount = input_amount();

	puts("Verify Data is Correct:");
	printf(
		"Date-->  %d/%d/%d\n"
		"Cat.-->  %s\n"
		"Desc-->  %s\n"
		"Type-->  %s\n"
		"Amt.-->  $%.2f\n",
		month,
		day,
		year,
		categorystr,
		descstr,
		transaction == 0 ? "Expense" : "Income",
		amount
	);
	printf("Y/N:  ");

	int result = confirm_input();
	if (result == 1) {
		if (debug == true) puts("TRUE");
		uld->month = month;
		uld->day = day;
		uld->year = year;
		uld->category = categorystr;
		uld->desc = descstr;
		uld->transtype = transaction;
		uld->amount = amount;
	} else if (result == 0) {
		if (debug == true) puts("FALSE");
		goto CLEANUP;
	} else {
		puts("Invalid answer");
		goto CLEANUP;
	}

	resultline = sort_csv(uld->month, uld->day, uld->year);
	if (resultline < 0) {
		puts("Failed to find where to add this record");
		goto CLEANUP;
	}
	printf("Result line: %d\n", resultline);
	add_csv_record(resultline, uld);

CLEANUP:
	if (debug == true) puts("CLEANUP");
	free(pidx);
	fclose(fptr);
	free(categorystr);
	free(descstr);
}

struct DynamicInts *index_csv(void) {
	struct DynamicInts *pidx = 
		malloc(sizeof(struct DynamicInts) + 0 * sizeof(int));
	if (pidx == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}

	pidx->lines = 0;
	FILE *fptr = open_csv("r");
	assert(ftell(fptr) == 0);
	char charbuff[LINE_BUFFER];

	while (1) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->lines++;
	}

	struct DynamicInts *tmp = realloc(pidx, sizeof(*pidx) + (pidx->lines * sizeof(int)));

	if (tmp == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}
	pidx = tmp;

	rewind(fptr);
	assert(ftell(fptr) == 0);

	for (int i = 0; i < pidx->lines; i++) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pidx->data[i] = ftell(fptr);
	}

//  Every line offset list
//	for (int i = 0; i < pidx->lines; i++) {
//		printf("Offset is %d at line %d\n", pidx->data[i], i+1);
//	}

	fclose(fptr);
	fptr = NULL;
	return pidx;
}

int nc_confirm_record(struct LineData *ld) {
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 0, "Confirm Record");
	print_record_vert(wptr, ld, 2);
	mvwxcprintw(wptr, getmaxy(wptr) - 2, "(Y)es  /  (N)o");
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(4) && c != 'q') {
		c = wgetch(wptr);
		switch(c) {
			case('y'):
			case('Y'):
				return 1;
			case('n'):
			case('N'):
			case(KEY_F(4)):
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

	char buff[LINE_BUFFER * 2];
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
			add_csv_record(sort_csv(ld->month, ld->day, ld->year), ld);
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
		line = fgets(buff, sizeof(buff), fptr);
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
	if (move_temp_to_main(tmpfptr, fptr) == 0) {
		puts("Edit Complete");
	}

	if (field == 2) free(ld->category);
	if (field == 3) free(ld->desc);
	return 0;

FAIL:
	return -1;
}

struct LineData *tokenize_str(struct LineData *pLd, char *str) {
	char **psavestr = &str;
	char *ptoken;
	for (int i = 0; i < CSV_FIELDS; i++) {
		ptoken = strsep(psavestr, ",");
		if (ptoken == NULL) break;
		switch (i) {
			case 0:
				pLd->month = atoi(ptoken);
				break;
			case 1:
				pLd->day = atoi(ptoken);
				break;
			case 2:
				pLd->year = atoi(ptoken);
				break;
			case 3:
				ptoken[strcspn(ptoken, "\n")] = '\0';
				pLd->category = ptoken;
				break;
			case 4:
				ptoken[strcspn(ptoken, "\n")] = '\0';
				pLd->desc = ptoken;
				break;
			case 5:
				pLd->transtype = atoi(ptoken);
				break;
			case 6:
				pLd->amount = atof(ptoken);
				break;
		}
	}
	return pLd;
}

int *list_records_by_year(FILE *fptr) {
	char buff[LINE_BUFFER];
	char *str;
	int *years = calloc(1, sizeof(int));
	int year;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		free(years);
		return NULL;
	}

	str = fgets(buff, sizeof(buff), fptr); // Read first year into index 0
	if (str == NULL) {
		free(years);
		return NULL;
	}
	(void)strsep(&str, ","); // month
	(void)strsep(&str, ","); // day, throwaway
	years[i] = atoi(strsep(&str, ",")); // year

	while((str = fgets(buff, sizeof(buff), fptr)) != NULL) {
		(void)strsep(&str, ","); // month
		(void)strsep(&str, ","); // day, throwaway
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
	char buff[LINE_BUFFER];
	char *str;
	int *months = calloc(12, sizeof(int));
	int year;
	int month;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	while((str = fgets(buff, sizeof(buff), fptr)) != NULL) {
		month = atoi(strsep(&str, ","));
		strsep(&str, ",");
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
	//	legacy CLI code
//	puts("Months With Records:");
//	for (int i = 0; i < 12; i++) {
//		if (months[i] != 0) {
//			printf("%d ", months[i]);
//		}
//	}
//	printf("\n");
	return months;
}

void legacy_read_csv(void) {
	int useryear;
	int usermonth;
	float income = 0;
	float expenses = 0;
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

	while (1) {
		linenum++;
		if (fgets(linebuff, sizeof(linebuff), fptr) == NULL) {
			break;
		}
		ld = tokenize_str(ld, linebuff);
		ld->linenum = linenum;
		if (ld->month == usermonth && ld->year == useryear) {
			month_record_exists = true;
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
			if(ld->transtype == 1) {
				income+=ld->amount;
			} else if (ld->transtype == 0) {
				expenses+=ld->amount;
			}
		}
	}

	if (month_record_exists) {

		/* Let's make a simple bar graph showing the income vs expense */
		char income_bar[10];
		char expense_bar[10];

		for (int i = 0; i < sizeof(income_bar); i++) {
			income_bar[i] = '#';
			expense_bar[i] = '#';
		}

		if (income > expenses) {
			float diff = expenses / income;
			diff *= 10;
			for (int i = 0; i < sizeof(expense_bar); i++) {
				i < (int)diff ? 
				(expense_bar[i] = '#') : (expense_bar[i] = '-');
			}
		} else {
			float diff = income / expenses;
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

		printf("Expense: $%.2f [", expenses);
		for (int i = 0; i < sizeof(expense_bar); i++) {
			printf("%c", expense_bar[i]);
		}
		printf("]\n");

		printf("Total:   $%.2f\n", income - expenses);
	} else {
		printf("No records match the entered date\n");
	}
	fclose(fptr);
	fptr = NULL;
}

void print_overview(int income, int expense) {
	/* Create a new window on the side to display the overview with
	 * graphs */
//	if (month_record_exists) {
//		/* Let's make a simple bar graph showing the income vs expense */
//		char income_bar[10];
//		char expense_bar[10];
//
//		for (int i = 0; i < sizeof(income_bar); i++) {
//			income_bar[i] = '#';
//			expense_bar[i] = '#';
//		}
//
//		if (income > expenses) {
//			float diff = expenses / income;
//			diff *= 10;
//			for (int i = 0; i < sizeof(expense_bar); i++) {
//				i < (int)diff ? 
//				(expense_bar[i] = '#') : (expense_bar[i] = '-');
//			}
//		} else {
//			float diff = income / expenses;
//			diff *= 10;
//			for (int i = 0; i < sizeof(income_bar); i++) {
//				i < (int)diff ? 
//				(income_bar[i] = '#') : (income_bar[i] = '-');
//			}
//		}
//
//		printf("Income:  $%.2f [", income);
//		for (int i = 0; i < sizeof(income_bar); i++) {
//			printf("%c", income_bar[i]);
//		}
//		printf("]\n");
//
//		printf("Expense: $%.2f [", expenses);
//		for (int i = 0; i < sizeof(expense_bar); i++) {
//			printf("%c", expense_bar[i]);
//		}
//		printf("]\n");
//
//		printf("Total:   $%.2f\n", income - expenses);
//	} else {
//		printf("No records match the entered date\n");
//	}
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
	while (c != '\r' && c != '\n' && c != KEY_F(4)) {
		c = wgetch(wptr);
		temp_x = getcurx(wptr);
		switch (c) {
			case ('h'):
			case (KEY_LEFT):
				if (temp_x - 5 >= print_x) {
					mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
					mvwchgat(wptr, print_y, temp_x - 5, 4, A_REVERSE, 0, NULL);
					wrefresh(wptr);
					scr_idx--;
				}
				break;
			case ('l'):
			case (KEY_RIGHT):
				if (temp_x + 5 <= (i * 4) + i) {
					mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
					mvwchgat(wptr, print_y, temp_x + 5, 4, A_REVERSE, 0, NULL);
					wrefresh(wptr);
					scr_idx++;
				}
				break;
			case (KEY_RESIZE):
				// FIX Right now I have no idea how to handle this
				break;
			case ('\n'):
			case ('\r'):
				selected_year = years_arr[scr_idx];
				break;
			case (KEY_F(4)):
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

	int print_y = 2;
	int print_x = 2;

	int temp_y = 0;
//	int temp_x = 0;
	
	int scr_idx = 0;
	int cur_idx = 0;

	wmove(wptr, print_y, print_x);
	for (int i = 0; i < 12; i++) {
		temp_y = getcury(wptr);
		if (months_arr[i] != 0) {
			wmove(wptr, temp_y, print_x);
			wprintw(wptr, "%s\n", months[months_arr[i] - 1]);
			scr_idx++;
		}
	}

	wmove(wptr, print_y, print_x);
	wchgat(wptr, 3, A_REVERSE, 0, NULL);
	box(wptr, 0, 0);
	wrefresh(wptr);

	int c = 0;
	while (c != '\r' && c != '\n' && c != KEY_F(4)) {
		c = wgetch(wptr);
		temp_y = getcury(wptr);
//		getyx(wptr, temp_y, temp_x);
		switch (c) {
			case ('j'):
			case (KEY_DOWN):
				if (temp_y - print_y + 1 < scr_idx) {
					mvwchgat(wptr, temp_y, print_x, 3, A_NORMAL, 0, NULL);
					mvwchgat(wptr, temp_y + 1, print_x, 3, A_REVERSE, 0, NULL);
					wrefresh(wptr);
					cur_idx++;
				}
				break;
			case ('k'):
			case (KEY_UP):
				if (temp_y - 1 >= print_y) {
					mvwchgat(wptr, temp_y, print_x, 3, A_NORMAL, 0, NULL);
					mvwchgat(wptr, temp_y - 1, print_x, 3, A_REVERSE, 0, NULL);
					wrefresh(wptr);
					cur_idx--;
				}
				break;
			case (KEY_RESIZE):
				// FIX Right now I have no idea how to handle this
				break;
			case ('\n'):
			case ('\r'):
				selected_month = months_arr[cur_idx];
				wrefresh(wptr);
				break;
			case (KEY_F(4)):
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

struct DynamicInts *get_matching_line_nums(FILE *fptr, int month, int year) {
	rewind(fptr);
	struct DynamicInts *lines = 
		malloc(sizeof(struct DynamicInts) + (REALLOC_THRESHOLD * sizeof(int)));
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
		if (cli_mode == true) {
			puts("Failed to read header");
		}
	}

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		line_month = atoi(strsep(&str, ","));
		(void)atoi(strsep(&str, ","));
		line_year = atoi(strsep(&str, ","));
		if (year == line_year && month == line_month) {
			if (realloc_counter == REALLOC_THRESHOLD - 1) {
				realloc_counter = 0;
				void *temp = realloc(lines, sizeof(struct DynamicInts) + 
					((lines->lines) + REALLOC_THRESHOLD) * sizeof(int));
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
	if (lines->lines % REALLOC_THRESHOLD != 0) {	
		void *temp = realloc(lines, sizeof(struct DynamicInts) + 
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
void print_record_hr(
	WINDOW *wptr, 
	struct ColumnWidth *cw, 
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
void print_record_vert(WINDOW *wptr, struct LineData *ld, int x_off) {
	mvwprintw(wptr, 1, x_off, "Date--> %d/%d/%d", ld->month, ld->day, ld->year);
	mvwprintw(wptr, 2, x_off, "Cat.--> %s", ld->category);
	mvwprintw(wptr, 3, x_off, "Desc--> %s", ld->desc);
	mvwprintw(wptr, 4, x_off, "Type--> %s", ld->transtype == 0 ? "Expense" : "Income");
	mvwprintw(wptr, 5, x_off, "Amt.--> %.2f", ld->amount);
}

int nc_select_field_to_edit(WINDOW* wptr) {
	mvwchgat(wptr, 1, 0, -1, A_REVERSE, 0, NULL);
	keypad(wptr, true);
	int select = 1;
	int c = 0;

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Field to Edit");
	wrefresh(wptr);
	while(c != KEY_F(4) && c != '\n' && c != '\r') {
		box(wptr, 0, 0);
		mvwxcprintw(wptr, 0, "Select Field to Edit");
		wrefresh(wptr);
		c = wgetch(wptr);

		switch(c) {
			case('j'):
			case(KEY_DOWN):
				if (select + 1 <= 6) {
					mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
					select++;
					mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
				}
				break;
			case('k'):
			case(KEY_UP):
				if (select - 1 > 0) {
					mvwchgat(wptr, select, 0, -1, A_NORMAL, 0, NULL);
					select--;
					mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL);
				}
				break;
			case('\n'):
			case('\r'):
				return select;
			case('q'):
			case(KEY_F(4)):
				break;
		}
	}
	return 0;
}

void nc_edit_transaction(int linenum) {
	struct LineData linedata, *ld = &linedata;
	struct DynamicInts *pidx = index_csv();

	WINDOW *wptr_edit = create_input_subwindow();
	FILE* fptr = open_csv("r+");
	fseek(fptr, pidx->data[linenum], SEEK_SET);

	char linebuff[LINE_BUFFER];
	char *line = fgets(linebuff, sizeof(linebuff), fptr);

	if (line == NULL) {
		exit(1);
	}

	ld = tokenize_str(ld, line);

	struct LineData *pLd = malloc(sizeof(*ld));

	if (pLd == NULL) {
		perror("Failed to allocate memory");
		free(pidx);
		fclose(fptr);
		fptr = NULL;
		return;
	}

	memcpy(pLd, ld, sizeof(*ld));

	print_record_vert(wptr_edit, ld, 2);

	mvwprintw(wptr_edit, 6, 2, "%s", "Delete");

	box(wptr_edit, 0, 0);
	wrefresh(wptr_edit);

	int field_to_edit = nc_select_field_to_edit(wptr_edit);

	fclose(fptr);

	nc_exit_window(wptr_edit);

	switch(field_to_edit) {
		case 0:
			break;
		case 1:
			edit_csv_record(linenum + 1, pLd, 1);
			break;
		case 2:
			edit_csv_record(linenum + 1, pLd, 2);
			break;
		case 3:
			edit_csv_record(linenum + 1, pLd, 3);
			break;
		case 4:
			edit_csv_record(linenum + 1, pLd, 4);
			break;
		case 5:
			edit_csv_record(linenum + 1, pLd, 5);
			break;
		case 6:
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
	ld = tokenize_str(ld, line);
	print_record_vert(wptr_detail, ld, 2);
	nc_exit_window_key(wptr_detail);
}

/*
 * After the detail sub window is closed, there is a gap of invisible lines
 * where the window was. This loops through each line and changes it attr
 * back to A_NORMAL and then re-reverse-video's the original line. Cursor
 * position is NOT changed.
 */
void refresh_on_detail_close(WINDOW *wptr, int n) {
	int temp_y, temp_x;
	getyx(wptr, temp_y, temp_x);
	wmove(wptr, 0, 0);
	for (int i = 0; i < n; i++) {
		mvwchgat(wptr, i, 0, -1, A_NORMAL, 0, NULL);
	}
	wmove(wptr, temp_y, temp_x);
	wchgat(wptr, -1, A_REVERSE, 0, NULL); 
}

/*
 * Main read loop. Populates variables in the struct pointed to by sr
 * if a record is highlighted and the user selects add or edit
 */
void nc_read_loop(
	WINDOW *wptr_parent, 
	WINDOW *wptr, 
	FILE *fptr, 
	struct SelectedRecord *sr,
	struct DynamicInts *pidx, 
	struct DynamicInts *plines) {

	struct ColumnWidth column_width, *cw = &column_width;
	struct LineData linedata_, *ld = &linedata_;
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	cw->max_x = max_x + 2; // 2 Offset
	int displayed_lines = 0;
	char *line_str;
	char linebuff[LINE_BUFFER];

	calculate_columns(cw);

	/* Print enough lines to fill the window but not more */
	int i = 0;
	while (i < max_y && displayed_lines < plines->lines) {
		fseek(fptr, pidx->data[plines->data[displayed_lines]], SEEK_SET);
		line_str = fgets(linebuff, sizeof(linebuff), fptr);
		ld = tokenize_str(ld, line_str);
		print_record_hr(wptr, cw, ld, i);
		displayed_lines++;
		i++;
	}

	wrefresh(wptr);

	/* Move cursor to first line of data and set that line to reverse video */
	int select = 0; // To keep track of selection for indexing
	int cur_y = 0; // Keep track of cursor position in window
	wmove(wptr, 0, 0);
	mvwchgat(wptr, select, 0, -1, A_REVERSE, 0, NULL); 
	if (debug == true) curs_set(1);
	wrefresh(wptr);

	//int temp_y, temp_x;

	int c = 0;
	while (c != KEY_F(4) && c != '\n' && c != '\r') {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch(c) {
			case('j'):
			case(KEY_DOWN):
				if (select + 1 < plines->lines) {
					mvwchgat(wptr, cur_y, 0, -1, A_NORMAL, 0, NULL); 
					cur_y++;
					select++;

					/* Handle case where there's more records than the window
					 * can display at once */
					if (displayed_lines < plines->lines && cur_y == max_y) {
						fseek(fptr, pidx->data[plines->data[select]], SEEK_SET);
						line_str = fgets(linebuff, sizeof(linebuff), fptr);
						ld = tokenize_str(ld, line_str);

						wmove(wptr, 0, 0);
						wdeleteln(wptr);
						wmove(wptr, max_y - 1, 0);
						print_record_hr(wptr, cw, ld, max_y - 1);
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

					if (cur_y < 0) cur_y = -1;
					if (displayed_lines < plines->lines && cur_y == -1 && select >= 0) {
						fseek(fptr, pidx->data[plines->data[select]], SEEK_SET);
						line_str = fgets(linebuff, sizeof(linebuff), fptr);
						ld = tokenize_str(ld, line_str);
						wmove(wptr, 0, 0);
						winsertln(wptr);
						print_record_hr(wptr, cw, ld, 0);
						cur_y = getcury(wptr);
					}
					mvwchgat(wptr, cur_y, 0, -1, A_REVERSE, 0, NULL); 
				}
				break;

			case('\n'):
			case('\r'):
				fseek(fptr, pidx->data[plines->data[select]], SEEK_SET);
				char *line = fgets(linebuff, sizeof(linebuff), fptr);
				show_detail_subwindow(line);
				refresh_on_detail_close(wptr, displayed_lines);
				mvwvline(wptr_parent, 1, 0, 0, getmaxy(wptr_parent) - 2);
				mvwvline(wptr_parent, 1, getmaxx(wptr_parent) - 1, 0, getmaxy(wptr_parent) - 2);
//				box(wptr_parent, 0, 0);
				wrefresh(wptr_parent);
				c = 0;
				break;
			case('a'):
			case(KEY_F(1)):
				sr->flag = 1;
				sr->index = plines->data[select];
				return;
			case('e'):
			case(KEY_F(2)):
				sr->flag = 2;
				sr->index = plines->data[select];
				return;
			case('r'):
			case(KEY_F(3)):
				sr->flag = 3;
				sr->index = 0;
			case('q'):
			case(KEY_F(4)):
				sr->flag = 0;
				sr->index = 0;
				return;
		}
	}
	sr->flag = 0;
	sr->index = 0;
	return; // no selection
}

void nc_read_setup(int sel_year, int sel_month) {
	/* LINES - 1 to still display the footer under wptr_read */

	struct DynamicInts *pidx = index_csv();
	struct SelectedRecord selectedrecord_ , *sr = &selectedrecord_;
	FILE *fptr = open_csv("r");

	WINDOW *wptr_read = newwin(LINES - 1, 0, 0, 0);
	if (wptr_read == NULL) {
		perror("Failed to create ncurses window");
		fclose(fptr);
		return;
	}

	int x_off = 2;
	int max_y, max_x;
	getmaxyx(wptr_read, max_y, max_x);

	WINDOW *wptr_lines;
	struct DynamicInts *plines;

	wptr_lines = create_lines_subwindow(max_y - 1, max_x, 1, x_off);
	wrefresh(wptr_lines);
	if (!sel_year) sel_year = nc_read_select_year(wptr_read, fptr);
	if (sel_year == -1) {
		mvwxcprintw(wptr_read, max_y / 2, 
			  "No records exist, add (F1) to get started");
		nc_exit_window_key(wptr_read);
		fclose(fptr);
		return;
	} else if (sel_year == 0) { // User exit
		free(pidx);
		nc_exit_window(wptr_read);
		fclose(fptr);
		return;
	}

	if (!sel_month) sel_month = nc_read_select_month(wptr_read, fptr, sel_year);
	if (sel_month < 0) {
		free(pidx);
		fclose(fptr);	
		return;
	}

	plines = get_matching_line_nums(fptr, sel_month, sel_year);
	if (plines == NULL) {
		free(pidx);
		fclose(fptr);
		return;
	}

	wclear(wptr_read);

	print_column_headers(wptr_read, x_off);

	box(wptr_read, 0, 0);
	mvwprintw(wptr_read, 0, x_off, "%d %s", sel_year, months[sel_month - 1]);
	wrefresh(wptr_read);

	nc_read_loop(wptr_read, wptr_lines, fptr, sr, pidx, plines);

	wclear(wptr_read);
	wclear(wptr_lines);
	free(pidx);
	pidx = NULL;
	free(plines);
	plines = NULL;
	fclose(fptr);
	fptr = NULL;
	nc_exit_window(wptr_lines);
	nc_exit_window(wptr_read);

	switch(sr->flag) {
		case(0): // 0 is no selection
			nc_read_setup(0, 0);
			break;
		case(1):
			nc_add_transaction(sel_year, sel_month);
			break;
		case(2):
			nc_edit_transaction(sr->index);
			nc_read_setup(sel_year, sel_month);
			break;
		case(3):
			nc_read_setup(sel_year, sel_month);
		default:
			break;
	}
	refresh();
}

int move_temp_to_main(FILE* tempfile, FILE* mainfile) {
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
	if (rename("tmp.txt", CSV_DIR) == -1) {
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
	FILE *tmpfptr = fopen("tmp.txt", "w+");
	if (fptr == NULL) {
		puts("Failed to open file");
		return -1;
	}
	char buff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(buff, sizeof(buff), fptr);
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

	FILE* fptr = open_csv("r+");
	assert(ftell(fptr) == 0);

	legacy_read_csv();
	
	struct DynamicInts *pcsvindex = index_csv();

	do {
		puts("Enter a line number");
		humantarget = input_n_digits(sizeof(long long) + 1, 2);
	} while (humantarget <= 0 || humantarget > pcsvindex->lines);

	target = humantarget - 1;

	if (debug == true) {
		printf("TARGET: %d\n", target);
		printf("TARGET OFFSET: %d\n", pcsvindex->data[target]);
	}

	fseek(fptr, pcsvindex->data[target], SEEK_SET);

	if (debug == true) {
		printf("COMMANDED SEEK OFFSET: %d\n", pcsvindex->data[target]);
	}
	
	char linebuff[LINE_BUFFER];
	char *str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		puts("failed to read line");
		exit(1);
	}

	ld = tokenize_str(ld, str);

	struct LineData *pLd = malloc(sizeof(*ld));
	if (pLd == NULL) {
		puts("Failed to allocate memory");
		free(pcsvindex);
		fclose(fptr);
		fptr = NULL;
		exit(1);
	}

	memcpy(pLd, ld, sizeof(*ld));

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

int nc_get_selection(WINDOW* wptr) {
	nc_print_welcome(wptr);
	nc_print_footer(wptr);
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(4)) {
		nc_print_welcome(wptr);
		nc_print_footer(wptr);
		wrefresh(wptr);
		c = getch();
		switch (c) {
			case (KEY_F(1)): // Add
				wclear(wptr);
				nc_add_transaction(0, 0);
				break;
			case (KEY_F(2)): // Edit
				wclear(wptr);
				break;
			case (KEY_F(3)):
				wclear(wptr);
				nc_read_setup(0, 0);
				break;
			case (KEY_F(4)): // Quit
				wclear(wptr);
				return 1;
		}
	}
	endwin();
	return 0;
}

void get_selection(void) {
	int choice;
	printf("Make a selection:\n");
	printf("a - Add Transaction\n");
	printf("e - Edit Transaction\n"); 
	printf("r - Read CSV\n");
	printf("s - DEBUG Sort CSV\n");
	printf("q - Quit\n");

	char *userstr = user_input(STDIN_SMALL_BUFF); // Must be free'd
	
	while (userstr == NULL) {
		get_selection();
	}

	if ((choice = upper(userstr)) == 0) {
		puts("Invalid character");
		free(userstr);
		userstr = NULL;
		get_selection();
	}
	
	free(userstr);
	userstr = NULL;

	switch (choice) {
		case 'A':
			printf("-*-ADD TRANSACTION-*-\n");
			add_transaction();
			break;
		case 'E':
			printf("-*-EDIT TRANSACTION-*-\n");
			edit_transaction();
			break;
		case 'R':
			printf("-*-READ CSV-*-\n");
			legacy_read_csv();
			break;
		case 'Q':
			printf("Quiting\n");
			exit(0);
		default:
			puts("Invalid character");
			printf("\n");
			get_selection();
	}
	return;
}

int main(int argc, char **argv) {
	FILE* fptr = open_csv("a"); // Make sure the CSV exists
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
			flag = nc_get_selection(stdscr);
		}
	} else {
		while (1) {
			get_selection();
		}
	}
	endwin();
}
