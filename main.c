#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include "helper.h"
#include "sorter.h"

#define CSV_DIR "./data.csv"
#define CSV_BAK_DIR "./data.csv.bak"
#define CSV_FIELDS 7
#define TEMP_FILE_DIR "./tmp.txt"
#define LINE_BUFFER 200
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
#define AMOUNT_BUFFER 16
#define MAX_LEN_DAYMON 3 // With \0
#define MAX_LEN_YEAR 5 // With \0
#define MIN_LEN_DAYMON MAX_LEN_DAYMON - 1

const bool debug = true;

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

struct csvindex {
	int lines;
	int offsets[];
};

struct Categories {
	int size;
	char **categories;
};

struct csvindex *index_csv();
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
	// Calling this twice is GROSS but I'm kinda stupid
			puts("Invalid Day");
		}
	}
	return day;
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


struct Categories *get_categories() {
	// Try to group categories by year
	struct Categories c, *pc = &c;
	pc->size = 0;
	FILE *fptr = open_csv("r");
	char *line;
	char buff[LINE_BUFFER];
	int year;
	return pc;
}

/* Adds a record to the CSV on line linetoadd*/
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
	struct csvindex *pcsvindex = index_csv();
	int year, month, day, resultline, transaction;
	char *categorystr;
	char *descstr;

	FILE* fptr = open_csv("r+");
	fseek(fptr, 0L, SEEK_END);

	year = input_year();
	month = input_month();
	day = input_day(month, year);
	categorystr = input_str_retry("Category:");
	descstr = input_str_retry("Description:");

	puts("Enter 1 or 2");
	puts("1. Expenses"); // 0 is an expense in the CSV
	puts("2. Income"); // 1 is an income in the CSV

	while((transaction = input_n_digits(2, 2)) == -1 || 
		transaction != 1 && transaction != 2) {
		puts("Invalid");
	}

	puts("$ Amount:");
	char *amountstr = user_input(AMOUNT_BUFFER);
	while (amountstr == NULL) {
		amountstr = user_input(AMOUNT_BUFFER);
	}
	float amount = atof(amountstr);

	puts("Verify Data is Correct:");
	printf(
		"%d,%d,%d,%s,%s,%d,%.2f\n", 
		month,
		day,
		year,
		categorystr,
		descstr,
		(transaction - 1),
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
		uld->transtype = transaction - 1;
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
	
//	int errcheck = fprintf(
//		fptr, 
//		"%d,%d,%d,%s,%s,%d,%.2f\n", 
//		uld->month, 
//		uld->day, 
//		uld->year, 
//		uld->category, 
//		uld->desc, 
//		uld->transtype, 
//		uld->amount
//	);

//	if (errcheck < 0) {
//		puts("Failed to write to file");
//	}

CLEANUP:
	if (debug == true) puts("CLEANUP");
	free(pcsvindex);
	fclose(fptr);
	free(categorystr);
	free(descstr);
	free(amountstr);
}

struct csvindex *index_csv() {
	struct csvindex *pcsvindex = malloc(sizeof(struct csvindex) + 0 * sizeof(int));
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

	printf("%d Lines\n", pcsvindex->lines); 
	// Now we know the # of lines,
	// realloc the struct to hold the offset data
	if (debug == true) {
		printf("NUMBER OF LINES: %d\n", pcsvindex->lines);
	}
	struct csvindex *tmp = realloc(pcsvindex, sizeof(*pcsvindex) + 
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
		pcsvindex->offsets[i] = ftell(fptr);
	}

// Uncomment to see every single line's offset
//
//	for (int i = 0; i < pcsvindex->lines; i++) {
//		printf("Offset is %d at line %d\n", pcsvindex->offsets[i], i+1);
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

	(void)fgets(buff, buffsize, fptr); // Read the header and throwaway
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
	puts("Years With Records:");
	for (int j = 0; j < i + 1; j++) {
		printf("%d ", years[j]);
	}
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

	(void)fgets(buff, buffsize, fptr); // Read the header and throwaway

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
	puts("Months With Records:");
	for (int i = 0; i < 12; i++) {
		if (months[i] != 0) {
			printf("%d ", months[i]);
		}
	}
	printf("\n");
	return months;
}

void read_csv(void) {
	int useryear;
	int usermonth;
	float income = 0;
	float expenses = 0;
	int linenum = 0;
	char linebuff[LINE_BUFFER] = {0};
	FILE *fptr = open_csv("r");
	int *yearsarr;
	int i = 0;
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
	free(yearsarr);
	yearsarr = NULL;

	rewind(fptr);
	int *monthsarr = list_records_by_month(fptr, useryear);
	free(monthsarr);
	monthsarr = NULL;
	rewind(fptr);
	usermonth = input_month();

	/* Read the header and throw it away */
	(void)fgets(linebuff, sizeof(linebuff), fptr);

	while (1) {
		linenum++;
		memset(linebuff, 0, LINE_BUFFER);
		if (fgets(linebuff, sizeof(linebuff), fptr) == NULL) {
			break;
		}
		ld = tokenize_str(ld, linebuff);
		ld->linenum = linenum;
		if (ld->month == usermonth && ld->year == useryear) {
			month_record_exists = true;
			printf(
				"%d.) %d/%d/%d Category: %s Description: %s, %d, $%.2f\n",
				ld->linenum, 
				ld->month, 
				ld->day, 
				ld->year, 
				ld->category, 
				ld->desc, 
				ld->transtype, 
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
		printf("Income: %.2f\n", income);
		printf("Expense: %.2f\n", expenses);
		printf("Total: %.2f\n", income - expenses);
	} else {
		printf("No records match the entered date\n");
	}
	fclose(fptr);
	fptr = NULL;
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
	char *amountstr;
	int transaction;

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
			ld->category = input_str_retry("Enter Category");
			break;
		case 3:
			ld->desc = input_str_retry("Enter Description");	
			break;
		case 4:
			puts("Enter 1 or 2");
			puts("1. Expenses");
			puts("2. Income");
			while((transaction = input_n_digits(2, 2)) == -1 || 
				transaction != 1 && transaction != 2) {
				puts("Invalid");
			}
			ld->transtype = transaction - 1;
			break;
		case 5:
			puts("$ Amount:");
			amountstr = user_input(AMOUNT_BUFFER);
			while (amountstr == NULL) {
				amountstr = user_input(AMOUNT_BUFFER);
			}
			float amount = atof(amountstr);
			ld->amount = amount;
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

	read_csv();
	struct csvindex *pcsvindex = index_csv();
	printf("LINES: %d\n", pcsvindex->lines);
	do {
		puts("Enter a line number");
		humantarget = input_n_digits(sizeof(long long) + 1, 2);
	} while (humantarget <= 0 || humantarget > pcsvindex->lines);

	target = humantarget - 1;

	if (debug == true) {
		printf("TARGET: %d\n", target);
		printf("TARGET OFFSET: %d\n", pcsvindex->offsets[target]);
	}

	fseek(fptr, pcsvindex->offsets[target], SEEK_SET);

	if (debug == true) {
		printf("COMMANDED SEEK OFFSET: %d\n", pcsvindex->offsets[target]);
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
		"4.) Type-->  %d\n"
		"5.) Amt.-->  $%.2f\n",
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype, 
		ld->amount
	);
	
	int fieldtoedit;
	do {
		puts("Enter field to change or press \"0\" to delete this transaction");
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
			read_csv();
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
	fclose(fptr);

	while (1) {
		get_selection();
		getchar();
	}
}
