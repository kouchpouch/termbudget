#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include "helper.h"

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

char *userinput(size_t buffersize) {
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

int inputndigits(int max_len, int min_len) {
	size_t bytesize = ((sizeof(char) * max_len) + 1);
	char *str = userinput(bytesize);

	while (str == NULL) {
		puts("Invalid Entry");
		str = userinput(bytesize);
	}

	if (strlen(str) < min_len) {
		puts("Input is too short");
		goto FAIL;
	}

	for (int i = 0; i < strlen(str); i++) {
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

int confirmInput() {
	char *confirm = userinput(STDIN_SMALL_BUFF);
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

int inputmonth() {
	int month;
	puts("Enter Month");
	while((month = inputndigits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Enter a Vaid Month");
	}
	return month;
}

int inputyear() {
	int year;
	puts("Enter Year");
	while((year = inputndigits(MAX_LEN_YEAR, MAX_LEN_YEAR)) == -1);
	return year;
}

int inputday(int month, int year) {
	int day;
	puts("Enter Day");
	while((day = inputndigits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1 ||
			dayexists(day, month, year) == false) {
		if (dayexists(day, month, year) == false) { // Calling this twice is GROSS but I'm kinda stupid
			puts("Invalid Day");
		}
	}
	return day;
}

char *input_str_retry(char *msg) {
	puts(msg);
	int len;
	char *str = userinput(STDIN_LARGE_BUFF);	
	while (str == NULL) {
		str = userinput(STDIN_LARGE_BUFF);
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

FILE *opencsv(char *mode) {
	FILE *fptr = fopen(CSV_DIR, mode);
	if (fptr == NULL) {
		puts("Failed to open file");
		exit(1);
	} else {
		return fptr;
	}
}

struct Categories *getcategories() {
	// Try to group categories by year
	struct Categories c, *pc = &c;
	pc->size = 0;
	FILE *fptr = opencsv("r");
	char *line;
	char buff[LINE_BUFFER];
	int year;
	return pc;
}

void addtransaction() {
	struct Linedata userlinedata_, *uld = &userlinedata_;
	int year;
	int month;
	int day;
	int transaction;
	char *categorystr;
	char *descstr;

	FILE* fptr = opencsv("r+");
	fseek(fptr, 0L, SEEK_END);

	year = inputyear();
	month = inputmonth();
	day = inputday(month, year);
	categorystr = input_str_retry("Category:");
	descstr = input_str_retry("Description:");

	puts("Enter 1 or 2");
	puts("1. Expenses"); // 0 is an expense in the CSV
	puts("2. Income"); // 1 is an income in the CSV

	while((transaction = inputndigits(2, 2)) == -1 || 
		transaction != 1 && transaction != 2) {
		puts("Invalid");
	}

	puts("$ Amount:");
	char *amountstr = userinput(AMOUNT_BUFFER);
	while (amountstr == NULL) {
		amountstr = userinput(AMOUNT_BUFFER);
	}
	float amount = atof(amountstr);

//	char *strings[] = { categorystr, descstr, amountstr };
//	for (int i = 0; i < 3; i++) { // Remove all newlines if they exist
//		int len = strlen(strings[i]);
//		printf("%s\n", strings[i]);
//		if (strings[i][len - 1] == '\n') {
//			strings[i][len - 1] = 0;
//		}
//	}

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

	int result = confirmInput();
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
		// Free heap and exit back to getselection()
	} else {
		puts("Invalid answer");
		goto CLEANUP;
	}

	int errcheck = fprintf(
		fptr, 
		"%d,%d,%d,%s,%s,%d,%.2f\n", 
		uld->month, 
		uld->day, 
		uld->year, 
		uld->category, 
		uld->desc, 
		uld->transtype, 
		uld->amount
	);

	if (errcheck < 0) {
		puts("Failed to write to file");
	}

CLEANUP:
	if (debug == true) puts("CLEANUP");
	fclose(fptr);
	free(categorystr);
	free(descstr);
	free(amountstr);
}

struct csvindex *indexcsv() {
	struct csvindex *pcsvindex = malloc(sizeof(struct csvindex) + 0 * sizeof(int));
	if (pcsvindex == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}

	pcsvindex->lines = 0;
	
	FILE *fptr = opencsv("r");

	assert(ftell(fptr) == 0); // Must start at a zero offset

	char charbuff[LINE_BUFFER]; // write into a buffer on the stack

	while (1) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pcsvindex->lines++;
	}

	printf("%d Lines\n", pcsvindex->lines); // Now we know the # of lines,
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
	assert(ftell(fptr) == 0); // Make sure we are back to the starting point

	for (int i = 0; i < pcsvindex->lines; i++) {
		char* test = fgets(charbuff, sizeof(charbuff), fptr);
		if (test == NULL) {
			break;
		}
		pcsvindex->offsets[i] = ftell(fptr);
	}

// --- Uncomment to see every single line's offset
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

void readcsv(void) {
	int useryear;
	int usermonth;
	float income = 0;
	float expenses = 0;
	int linenum = 0;

	FILE *fptr = opencsv("r");

	struct Linedata linedata_, *ld = &linedata_;

	useryear = inputyear();
	usermonth = inputmonth();

	// --------------------------------- //
	// Might want to keep this for later //
	// --------------------------------- //

//	char fields[LINE_BUFFER];
//
//	if (fgets(fields, sizeof(fields), fptr) == NULL) {
//		exit(1);
//	}

	// Init to 1 to count first field where no comma is present
//	int nfields = 1; 
//
//	for (int i = 0; i < strlen(fields); i++) {
//		if (fields[i] == ',') {
//			nfields++;	
//		}
//	}

	while (1) {
		char linebuff[LINE_BUFFER] = {0};

		if (fgets(linebuff, sizeof(linebuff), fptr) == NULL) {
			break;
		}

		ld = tokenize_str(ld, linebuff);

		linenum++;
		ld->linenum = linenum;
		if (ld->month == usermonth && ld->year == useryear) {
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
	printf("Income: %.2f\n", income);
	printf("Expense: %.2f\n", expenses);
	printf("Total: %.2f\n", income - expenses);
	fclose(fptr);
	fptr = NULL;
}

int copytemptomain(FILE* tempfile, FILE* mainfile) {
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

int editcsvline(int linetoreplace, struct Linedata* ld, int field) {
	if (linetoreplace == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	FILE *fptr = opencsv("r");
	FILE *tmpfptr = fopen("tmp.txt", "w+");
	if (fptr == NULL) {
		puts("Failed to open file");
		return -1;
	}
	/* LINE_BUFFER * 2 to account for any line that may be longer
	 * due to a manual CSV entry, which shouldn't happen, but this will
	 * protect us in case it does */
	char buff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	char *amountstr;
	int transaction;

	switch(field) {
		case 1: // change the date
			ld->month = inputmonth();
			ld->year = inputyear();
			ld->day = inputday(ld->month, ld->year);
			break;
		case 2: // change category
			ld->category = input_str_retry("Enter Category");
			break;
		case 3: // desc
			ld->desc = input_str_retry("Enter Description");	
			break;
		case 4: // transaction type
			while((transaction = inputndigits(2, 2)) == -1 || 
				transaction != 1 && transaction != 2) {
				puts("Invalid");
			}
			ld->transtype = transaction;
			break;
		case 5: // amount
			amountstr = userinput(AMOUNT_BUFFER);
			while (amountstr == NULL) {
				amountstr = userinput(AMOUNT_BUFFER);
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
		if (linenum != linetoreplace - 1) {
			fputs(line, tmpfptr);
		} else if (linenum == linetoreplace - 1) {
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
		linenum++;	
	} while(line != NULL);
	if (copytemptomain(tmpfptr, fptr) == 0) {
		puts("Edit Complete");
	}
	return 0;
}

int deletecsvline(int linetodelete) {
	if (linetodelete == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	FILE *fptr = opencsv("r");
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
	copytemptomain(tmpfptr, fptr);
	return 0;
}

void edittransaction() {
	int target;
	int humantarget;
	struct Linedata linedata, *ld = &linedata;

	FILE* fptr = opencsv("r+");
	assert(ftell(fptr) == 0);

	readcsv();
	struct csvindex *pcsvindex = indexcsv();
	printf("LINES: %d\n", pcsvindex->lines);
	do {
		puts("Enter a line number");
		humantarget = inputndigits(sizeof(long long) + 1, 2);
	} while (humantarget <= 0 || humantarget > pcsvindex->lines);

	/*	Subtract 2 from the human-readable target value because the index
	*	accessed in pcsv-offsets starts at zero, that's the first -1. Then
	*	the offset value itself is pointed to the last byte of the the prev.
	*	line, -1 again. I.E. the last offset in pcsv->offsets is the stream
	*	position of the very last byte in the stream. If passed the fgets
	*	function will return null and the program will exit */	

	target = humantarget - 2;

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
		fieldtoedit = inputndigits(2, 2); // Only input 1 digit
	} while (fieldtoedit > 5 || fieldtoedit < 0);

	switch(fieldtoedit) {
		case 0:
			if (deletecsvline(humantarget) == 0) {
				puts("Successfully Deleted Transaction");
			}
			break;
		case 1:
			editcsvline(humantarget, pLd, 1);
			break;
		case 2:
			editcsvline(humantarget, pLd, 2);
			break;
		case 3:
			editcsvline(humantarget, pLd, 3);
			break;
		case 4:
			editcsvline(humantarget, pLd, 4);
			break;
		case 5:
			editcsvline(humantarget, pLd, 5);
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

void getselection() {
	int choice;
	printf("Make a selection:\n");
	printf("a - Add Transaction\n");
	printf("e - Edit Transaction\n"); 
	printf("r - Read CSV\n");
	printf("q - Quit\n");

	char *userstr = userinput(STDIN_SMALL_BUFF); // Must be free'd
	
	while (userstr == NULL) {
		getselection();
	}

	if ((choice = upper(userstr)) == 0) {
		puts("Invalid character");
		free(userstr);
		userstr = NULL;
		getselection();
	}
	
	free(userstr);
	userstr = NULL;

	switch (choice) {
		case 'A':
			printf("-*-ADD TRANSACTION-*-\n");
			addtransaction();
			break;
		case 'E':
			printf("-*-EDIT TRANSACTION-*-\n");
			edittransaction();
			break;
		case 'R':
			printf("-*-READ CSV-*-\n");
			readcsv();
			break;
		case 'Q':
			printf("Quiting\n");
			exit(0);
		default:
			puts("Invalid character");
			printf("\n");
			getselection();
	}
	return;
}

int main(int argc, char **argv) {
	FILE* fptr = opencsv("a");
	fclose(fptr);

	while (1) {
		getselection();
		getchar();
	}
}
