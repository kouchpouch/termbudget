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
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
#define AMOUNT_BUFFER 16
#define MAX_LEN_DAYMON 3 // With \0
#define MAX_LEN_YEAR 5 // With \0 This is kind of ugly, no?
#define MIN_LEN_DAYMON MAX_LEN_DAYMON - 1
#define CURRENT_YEAR 2026 // FIX This is to not be hard coded

const bool debug = false;
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

struct Linedata {
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

struct Categories *list_categories(int month, int year);
struct DynamicInts *index_csv();
int move_temp_to_main(FILE* tempfile, FILE* mainfile);
int delete_csv_record(int linetodelete);

char *user_input(size_t buffersize) {
	int minchar = 2;
	char *buffer = (char *)malloc(buffersize);
	if (buffer == NULL) {
		puts("Failed to allocate memory");
		return buffer;
	}

	if (fgets(buffer, buffersize, stdin) == NULL) {
		printf("Invalid Input\n");
		free(buffer);
		buffer = NULL;
		return buffer;
	}
	int length = strnlen(buffer, buffersize);

	if (buffer[length - 1] != '\n') {
		printf("Input is too long, try again\n");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		free(buffer);
		buffer = NULL;
		return buffer;
	}
	if (length < minchar) {
		puts("Input is too short");
		free(buffer);
		buffer = NULL;
		return buffer;
	}
	if (strstr(buffer, ",")) {
		puts("No commas allowed, we're using a CSV, after all!");
		free(buffer);
		buffer = NULL;
		return buffer;
	}
	return buffer; // Must be free'd
}

int input_n_digits(int max_len, int min_len) {
	size_t bytesize = ((sizeof(char) * max_len) + 1);
	char *str = user_input(bytesize);

	while (str == NULL) {
		puts("Invalid Entry");
		str = user_input(bytesize);
	}

	if ((int)strlen(str) < min_len) {
		puts("Input is too short");
		goto FAIL;
	}

	for (int i = 0; i < (int)strlen(str); i++) {
		if (!isdigit(*(str + i)) && *(str + i) != '\n') {
			printf("Invalid character \"%c\", must be digit\n", *(str + i));
			goto FAIL;
		}
	}
	int digits = atoi(str);
	free(str);
	str = NULL;
	return digits;

FAIL:
	free(str);
	str = NULL;
	return -1;
}

int confirm_input() {
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

int input_month() {
	int month;
	puts("Enter Month");
	while((month = input_n_digits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Enter a Vaid Month");
	}
	return month;
}

int input_year() {
	int year;
	puts("Enter Year");
	while((year = input_n_digits(MAX_LEN_YEAR, MAX_LEN_YEAR)) == -1);
	return year;
}

int input_day(int month, int year) {
	int day;
	puts("Enter Day");

	while((day = input_n_digits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1 ||
			dayexists(day, month, year) == false) {
		if (dayexists(day, month, year) == false) { 
			puts("Invalid Day");
		}
	}
	return day;
}

int input_transaction_type() {
	int t;

	/* 0 is an expense and 1 is income in the CSV */
	puts("Enter 1 or 2");
	puts("1. Expense");
	puts("2. Income");

	while((t = input_n_digits(2, 2)) == -1 || (t != 1 && t != 2)) {
		puts("Invalid");
	}
	return t - 1; // sub 1 to convert human readable to CSV format
}

float input_amount() {
	puts("$ Amount:");
	char *str = user_input(AMOUNT_BUFFER);
	while (str == NULL) {
		puts("Invalid");
		str = user_input(AMOUNT_BUFFER);
	}
	float amount = atof(str);
	free(str);
	str = NULL;
	return amount;
}

/* Takes a user's input and displays msg, on failure to read the user's
 * input the user's input is read again. The newline character is removed */
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

/* Temp csv will always open with mode w+ */
FILE *open_temp_csv() {
	FILE *tmpfptr = fopen("tmp.txt", "w+");
	if (tmpfptr == NULL) {
		puts("Failed to open file");
		exit(1);
	}
	return tmpfptr;
}

/* Reads the header of the file until a newline is found */
void seek_beyond_header(FILE *fptr) {
	char c = 0;	
	while (c != '\n') {
		c = getc(fptr);
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
	return pc;
}

/* Adds a record to the CSV on line linetoadd */
void add_csv_record(int linetoadd, struct Linedata *ld) {
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

void add_transaction() {
	struct Linedata userlinedata_, *uld = &userlinedata_;
	struct DynamicInts *pcsvindex = index_csv();
	int year, month, day, resultline, transaction;
	char *categorystr;
	char *descstr;
	float amount;

	FILE* fptr = open_csv("r+");
	fseek(fptr, 0L, SEEK_END);

	year = input_year();
	month = input_month();
	day = input_day(month, year);
	categorystr = input_category(month, year);
	if (categorystr == NULL) goto CLEANUP;
//	categorystr = input_str_retry("Category:"); // Old way, to be removed
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
		// Free heap and exit back to get_selection()
	} else {
		puts("Invalid answer");
		goto CLEANUP;
	}

	resultline = sort_csv(uld->month, uld->day, uld->year, pcsvindex->lines);
	if (resultline < 0) {
		puts("Failed to find where to add this record");
		goto CLEANUP;
	}
	printf("Result line: %d\n", resultline);
	add_csv_record(resultline, uld);

CLEANUP:
	if (debug == true) puts("CLEANUP");
	free(pcsvindex);
	fclose(fptr);
	free(categorystr);
	free(descstr);
}

struct DynamicInts *index_csv() {
	struct DynamicInts *pcsvindex = 
		malloc(sizeof(struct DynamicInts) + 0 * sizeof(int));
	if (pcsvindex == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}

	pcsvindex->lines = 0;
	FILE *fptr = open_csv("r");
	assert(ftell(fptr) == 0);
	char charbuff[LINE_BUFFER];

	while (1) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pcsvindex->lines++;
	}

//	printf("%d Lines\n", pcsvindex->lines); 
	// Now we know the # of lines,
	// realloc the struct to hold the offset data
//	if (debug == true) {
//		printf("NUMBER OF LINES: %d\n", pcsvindex->lines);
//	}
	struct DynamicInts *tmp = realloc(pcsvindex, sizeof(*pcsvindex) + 
						       (pcsvindex->lines * sizeof(int)));
	if (tmp == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}
	pcsvindex = tmp;

	rewind(fptr);
	assert(ftell(fptr) == 0);

	for (int i = 0; i < pcsvindex->lines; i++) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pcsvindex->data[i] = ftell(fptr);
	}

//  Every line offset list
//	for (int i = 0; i < pcsvindex->lines; i++) {
//		printf("Offset is %d at line %d\n", pcsvindex->data[i], i+1);
//	}

	fclose(fptr);
	fptr = NULL;
	return pcsvindex;
}

struct Linedata *tokenize_str(struct Linedata *pLd, char *str) {
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
				pLd->category = ptoken;
				break;
			case 4:
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
	int buffsize = sizeof(buff);
	char *str;
	int *years = calloc(1, sizeof(int));
	int year;
	int i = 0;

	seek_beyond_header(fptr);
	str = fgets(buff, buffsize, fptr); // Read first year into index 0
	if (str == NULL) {
		puts("Failed to read line");
		free(years);
		exit(1);
	}
	(void)strsep(&str, ","); // month
	(void)strsep(&str, ","); // day, throwaway
	years[i] = atoi(strsep(&str, ",")); // year

	while((str = fgets(buff, buffsize, fptr)) != NULL) {
		(void)strsep(&str, ","); // month
		(void)strsep(&str, ","); // day, throwaway
		year = atoi(strsep(&str, ",")); // year
		if (year != years[i]) {
			i++;
			int *tmp = reallocarray(years, i + 1, sizeof(int));
			if (tmp == NULL) {
				free(years);
				puts("Failed to reallocate memory");
				exit(1);
			}
			years = tmp;
			years[i] = year;
		}
	}
	int *tmp = reallocarray(years, i + 2, sizeof(int));
	if (tmp == NULL) {
		free(years);
		puts("Failed to reallocate memory");
		exit(1);
	}
	years = tmp;
	years[i + 1] = 0; 
	// Place a zero at the end of the array so we can loop
	// through and not go past the end
//	puts("Years With Records:");
//	for (int j = 0; j < i + 1; j++) {
//		printf("%d ", years[j]);
//	}
	printf("\n");
	return years;
}

int *list_records_by_month(FILE *fptr, int matchyear) {
	char buff[LINE_BUFFER];
	int buffsize = sizeof(buff);
	char *str;
	int *months = calloc(12, sizeof(int));
	int year;
	int month;
	int i = 0;

	seek_beyond_header(fptr);

	while((str = fgets(buff, buffsize, fptr)) != NULL) {
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

[[deprecated("Moved to read_data_to_screen()")]] void legacy_read_csv(void) {
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

	struct Linedata linedata_, *ld = &linedata_;

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

	seek_beyond_header(fptr);

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
}

int nc_read_select_year(WINDOW *wptr, FILE *fptr) {
	rewind(fptr);

	keypad(wptr, true);	

	int *years_arr = list_records_by_year(fptr);
	int selected_year = 0;
	int print_y = 1;
	int print_x = 2; // Offset by 2 to the right for styling
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);

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
		init_rv_x += (4 * flag) + 1;
		scr_idx = flag;
	}
	
	mvwchgat(wptr, print_y, init_rv_x, 4, A_REVERSE, 0, NULL);

	wrefresh(wptr);

	int c = 0;
	int temp_y, temp_x;
	getyx(wptr, temp_y, temp_x);

	while (c != '\r' && c != '\n' && c != KEY_F(4)) {
		c = wgetch(wptr);
		getyx(wptr, temp_y, temp_x);
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
				if (temp_x + 5 <= (i * 4) + 1) {
					mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
					mvwchgat(wptr, print_y, temp_x + 5, 4, A_REVERSE, 0, NULL);
					wrefresh(wptr);
					scr_idx++;
				}
				break;
			case (KEY_RESIZE):
			//	FIX Right now I have no idea how to handle this
				break;
			case ('\n'):
			case ('\r'):
				selected_year = years_arr[scr_idx];
				break;
			case (KEY_F(4)):
				nc_exit_window(wptr);
				free(years_arr);
				return -1;
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

	int print_y = 2; // Print below years
	int print_x = 2;

	int temp_y = 0;
	int temp_x = 0;
	
	int scr_idx = 0;
	int cur_idx = 0;

	wmove(wptr, print_y, print_x);
	for (int i = 0; i < 12; i++) {
		getyx(wptr, temp_y, temp_x);
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
		getyx(wptr, temp_y, temp_x);
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
			//	FIX Right now I have no idea how to handle this
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

//	wgetch(wptr); // Just to hang the terminal

	free(months_arr);
	months_arr = NULL;

	return selected_month;
}

struct DynamicInts *get_matching_line_nums(FILE *fptr, int month, int year) {
	rewind(fptr);
	// Initial alloc of 64 integers
	int realloc_increment = 64;
	struct DynamicInts *lines = 
		malloc(sizeof(struct DynamicInts) + (realloc_increment * sizeof(int)));
	if (lines == NULL) {
		exit(1);
	}

	assert(ftell(fptr) == 0);

	lines->lines = 0;

	int linenumber = 0;
	int line_month, line_year;
	int realloc_counter = 0;
	char linebuff[LINE_BUFFER];
	char *str;

	seek_beyond_header(fptr);

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		line_month = atoi(strsep(&str, ","));
		(void)atoi(strsep(&str, ","));
		line_year = atoi(strsep(&str, ","));
		if (year == line_year && month == line_month) {
			if (realloc_counter == realloc_increment - 1) {
				realloc_counter = 0;
				void *temp = realloc(lines, sizeof(struct DynamicInts) + 
					((lines->lines) + realloc_increment) * sizeof(int));
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

	// Shrink back down if oversized alloc
	if (lines->lines % realloc_increment != 0) {	
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

WINDOW *create_lines_sub_window(int max_y, int max_x, int y_off, int x_off) {
	WINDOW *wptr = newwin(max_y - y_off * 2, max_x - x_off * 2, y_off, x_off);
	keypad(wptr, true);
	return wptr;
}

void print_data_to_sub_window(WINDOW *wptr, FILE *fptr, 
	struct DynamicInts *pidx, struct DynamicInts *plines) {
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	int i = 0;
	int j = 0;
	char *line_str;
	char linebuffer[LINE_BUFFER];

	/* Print enough lines to fill the window but not more */

	while (i < max_y && j < plines->lines) {
		fseek(fptr, pidx->data[plines->data[j]], SEEK_SET);
		j++;
		i++;
		line_str = fgets(linebuffer, sizeof(linebuffer), fptr);
		wprintw(wptr, "%s", line_str);
	}

	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(4)) {
		c = wgetch(wptr);
		switch(c) {
			case('k'):
			case(KEY_DOWN):
				break;
			case('j'):
			case(KEY_UP):
				break;
			case(KEY_F(4)):
				break;
		}
	}
}

void read_data_to_screen(void) {
	/* LINES - 1 to still display the footer under wptr_read */

	struct DynamicInts *pidx = index_csv();

	WINDOW *wptr_read = newwin(LINES - 1, 0, 0, 0);
	keypad(wptr_read, true);
	curs_set(0);

	int max_y, max_x;
	getmaxyx(wptr_read, max_y, max_x);

	float income = 0;
	float expenses = 0;
	int linenum = 0;
	char linebuff[LINE_BUFFER] = {0};
	FILE *fptr = open_csv("r");

	struct Linedata linedata_, *ld = &linedata_;

	int sel_year = nc_read_select_year(wptr_read, fptr);
	if (sel_year < 0) {
		fclose(fptr);
		return;
	}

	int sel_month = nc_read_select_month(wptr_read, fptr, sel_year);
	if (sel_month < 0) {
		fclose(fptr);	
		return;
	}

	struct DynamicInts *plines = 
		get_matching_line_nums(fptr, sel_month, sel_year);
	if (plines == NULL) {
		fclose(fptr);
		return;
	}

	box(wptr_read, 0, 0);
	mvwprintw(wptr_read, 0, 2, "%d %s", 
		   sel_year, months[sel_month - 1]);
	wrefresh(wptr_read);

	WINDOW *wptr_lines = create_lines_sub_window(max_y, max_x, 1, 2);

	print_data_to_sub_window(wptr_lines, fptr, pidx, plines);

	rewind(fptr);

	wclear(wptr_read);

//	while (1) {
//		linenum++;
//
//		if (fgets(linebuff, sizeof(linebuff), fptr) == NULL) {
//			break;
//		}
//
//		ld = tokenize_str(ld, linebuff);
//		ld->linenum = linenum;
//		if (ld->month == selected_month && ld->year == selected_year) {
//			month_record_exists = true;
//			if (print_y + 1 < max_y) {
//				mvwprintw(wptr_read, print_y, 2,
//					"%d.) %d/%d/%d Category: %s Description: %s, %s, $%.2f\n",
//					ld->linenum, 
//					ld->month, 
//					ld->day, 
//					ld->year, 
//					ld->category, 
//					ld->desc, 
//					ld->transtype == 0 ? "Expense" : "Income", 
//					ld->amount
//				 );
//				print_y++;
//			}
//			if(ld->transtype == 1) {
//				income+=ld->amount;
//			} else if (ld->transtype == 0) {
//				expenses+=ld->amount;
//			}
//		}
//	}
	

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

	free(pidx);
	pidx = NULL;
	free(plines);
	plines = NULL;

	wgetch(wptr_read); // Just to hang the terminal
	wclear(wptr_read);
	wrefresh(wptr_read);
	delwin(wptr_read);
	delwin(wptr_lines);
	fclose(fptr);
	fptr = NULL;
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

int edit_csv_record(int linetoreplace, struct Linedata *ld, int field) {
	if (linetoreplace == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	linetoreplace += 1; // Count up by 1 to skip the header on line 1. So
	// when the user enters 1 the program will edit line 2.
	FILE *fptr = open_csv("r");
	FILE *tmpfptr = open_temp_csv();

	/* LINE_BUFFER * 2 to account for any line that may be longer
	 * due to a manual CSV entry, which shouldn't happen, but this will
	 * protect us in case it does */
	char buff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;

	switch(field) {
		case 1:
			ld->year = input_year();
			ld->month = input_month();
			ld->day = input_day(ld->month, ld->year);
			fclose(fptr);
			fclose(tmpfptr);
			delete_csv_record(linetoreplace - 1);
			add_csv_record(sort_csv(ld->month, ld->day, ld->year, 2000), ld);
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
			fclose(fptr);
			fclose(tmpfptr);
			return -1;
	}

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
	if (move_temp_to_main(tmpfptr, fptr) == 0) {
		puts("Edit Complete");
	}
	if (field == 2) free(ld->category);
	if (field == 3) free(ld->desc);
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

void edit_transaction() {
	int target;
	int humantarget;
	struct Linedata linedata, *ld = &linedata;

	FILE* fptr = open_csv("r+");
	assert(ftell(fptr) == 0);

//	legacy_read_csv();
	
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

	ld = tokenize_str(ld, linebuff);

	struct Linedata *pLd = malloc(sizeof(*ld));
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
		fieldtoedit = input_n_digits(2, 2); // Only input 1 digit
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
		wrefresh(wptr);
		c = getch();
		switch (c) {
			case (KEY_F(1)): // Add
				wclear(wptr);
				add_transaction();
				break;
			case (KEY_F(2)): // Edit
				wclear(wptr);
				edit_transaction();
				break;
			case (KEY_F(3)):
				read_data_to_screen();
				break;
			case (KEY_F(4)): // Quit
				wclear(wptr);
				return 1;
		}
	}
	endwin();
	return 0;
}

void get_selection() {
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
//			legacy_read_csv();
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
	
	// Make a non-ncurses command-line option to let the user use termBudget
	// how they want to
	stdscr = nc_new_win();

	fclose(fptr);

	int flag = 0;

	while (flag == 0) {
		flag = nc_get_selection(stdscr);
	}

	endwin();
	exit_curses(0);
}
