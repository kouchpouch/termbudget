#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include "helper.h"

#define LINE_BUFFER 512
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
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

struct csvindex { // Dynamically Sized Array
	int lines;
	int offsets[];
}; // csvindex_, *pcsvindex = &csvindex_;

char *userinput(size_t buffersize) {
	char *buffer = (char *)malloc(buffersize);
	if (buffer == NULL) {
		puts("Failed to allocate memory");
		return buffer;
	}

	while (fgets(buffer, buffersize, stdin) == NULL) {
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
	return buffer; // Must be free'd
}

int inputndigits(int max_len, int min_len) {
	size_t bytesize = ((sizeof(char) * max_len) + 1);
	char *str = userinput(bytesize);

	while (str == NULL) {
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

void addtransaction() {
	struct Linedata userlinedata_, *uld = &userlinedata_;
	unsigned int year;
	unsigned int month;
	unsigned int day;
	unsigned int transaction;
	FILE* fptr = fopen("data.csv", "r+"); // Might have to mess with this mode
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(1);
	}

	fseek(fptr, 0L, SEEK_END);

	year = inputyear();
	month = inputmonth();
	day = inputday(month, year);

	puts("Category:");
	char *categorystr = userinput(STDIN_LARGE_BUFF);	
	while (categorystr == NULL) {
		puts("CATEGORY ENTRY NOT VALID");
		categorystr = userinput(STDIN_LARGE_BUFF);
	}
	if (strlen(categorystr) <= 1) {
		categorystr = "null";
	}

	puts("Description:");
	char *descstr = userinput(STDIN_LARGE_BUFF);	
	while (descstr == NULL) {
		puts("CATEGORY ENTRY NOT VALID");
		descstr = userinput(STDIN_LARGE_BUFF);
	}
	if (strlen(descstr) <= 1) {
		descstr = "null";
	}

	puts("Enter 1 or 2");
	puts("1. Expense"); // 0 is an expense in the CSV
	puts("2. Income"); // 1 is an income in the CSV

	while((transaction = inputndigits(2, 2)) == -1
		&& transaction != 1
		&& transaction != 2) {
		puts("Invalid");
	}

	puts("$ Amount:");
	char *amountstr = userinput(STDIN_LARGE_BUFF);
	if (amountstr == NULL) {
		amountstr = userinput(STDIN_LARGE_BUFF);
	}
	float amount = atof(amountstr);

	char *strings[] = { categorystr, descstr, amountstr };
	for (int i = 0; i < 3; i++) { // Remove all newlines if they exist
		int len = strlen(strings[i]);
		printf("%s\n", strings[i]);
		if (strings[i][len - 1] == '\n') {
			strings[i][len - 1] = 0;
		}
	}

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
		// Free heap and exit back to getSelection()
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

	return;
}

struct csvindex *indexcsv() {
	// Inital alloc
	struct csvindex *pcsvindex = malloc(sizeof(struct csvindex) + 0 * sizeof(int));
	if (pcsvindex == NULL) {
		puts("Failed to allocate memory");
		exit(1);
	}

	pcsvindex->lines = 0;
	
	FILE* fptr = fopen("data.csv", "r"); // Beginning of file stream 'r'
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(1);
	}

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

void readcsv(void) {
	int useryear;
	int usermonth;
	float total = 0;
	float income = 0;
	float expenses = 0;
	int linenum = 0;

	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(1);
	}

	struct Linedata linedata_, *ld = &linedata_;

	useryear = inputyear();
	usermonth = inputmonth();

	char fields[LINE_BUFFER];

	if (fgets(fields, sizeof(fields), fptr) == NULL) {
		exit(1);
	}

	/* Count number of fields
	* Init to 1 to count first field where no comma is present */
	int numFields = 1; 

	for (int i = 0; i < strlen(fields); i++) {
		if (fields[i] == ',') {
			numFields++;	
		}
	}

	while (1) {
		char *charbuff = (char *)malloc(LINE_BUFFER * sizeof(char));
		if (charbuff == NULL) {
			exit(1);
		}
		/* For each line, tokenize the fields to retrieve each cell's data */

		if (fgets(charbuff, LINE_BUFFER, fptr) == NULL) {
			free(charbuff);
			charbuff = NULL;
			break;
		}

		char *token = strtok(charbuff, ",");
		if (token != NULL) {
			ld->month = atoi(token);
		}

		for (int i = 1; i < numFields; i++) {
			token = strtok(NULL, ",");
			if (token != NULL) {
				switch (i) {
					case 1:
						ld->day = atoi(token);
						break;
					case 2:
						ld->year = atoi(token);
						break;
					case 3:
						ld->category = token;
						break;
					case 4:
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
		free(charbuff);
		charbuff = NULL;
	}
	printf("Income: %.2f\n", income);
	printf("Expense: %.2f\n", expenses);
	printf("Total: %.2f\n", income - expenses);
	fclose(fptr);
	fptr = NULL;
}

void edittransaction() {

	// ----------------------------------------------------------- //
	//  Users should be able to delete and edit transactions in a  //
	//  specific month and year									   //
	// ----------------------------------------------------------- //

	int target;
	int targetoffset;
	struct Linedata linedata, *ld = &linedata;

	FILE* fptr = fopen("data.csv", "r+");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(1);
	}
	assert(ftell(fptr) == 0);

	readcsv();
	struct csvindex *pcsvindex = indexcsv();

	puts("Enter a line number");
	target = inputndigits(sizeof(long long) + 1, 2);
	target -= 1;
	targetoffset = pcsvindex->offsets[target];

	if (debug == true) printf("TARGET: %d\n", target);

	fseek(fptr, pcsvindex->offsets[target], SEEK_SET);

	if (debug == true) {
		printf("COMMANDED SEEK OFFSET: %d\n", pcsvindex->offsets[target]);
	}
	
	char buff[LINE_BUFFER];
	char *str = fgets(buff, sizeof(buff), fptr);
	if (str == NULL) {
		puts("failed to read line");
		exit(1);
	}

	char *token;
	printf("%s\n", str);

	token = strtok(buff, ",");
	if (token != NULL) {
		ld->month = atoi(token);
	}
	for (int i = 1; i < 7; i++) {
		token = strtok(NULL, ",");
		if (token != NULL) {
			switch (i) {
				case 1:
					ld->day = atoi(token);
					break;
				case 2:
					ld->year = atoi(token);
					break;
				case 3:
					ld->category = token;
					break;
				case 4:
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

	printf(
		"1.) Date-->     %d/%d/%d\n"
		"2.) Category--> %s\n"
		"3.) Desc-->     %s\n"
		"4.) Type-->     %d\n"
		"5.) Amount -->  $%.2f\n",
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype, 
		ld->amount
	);
	
	int fieldtoedit = 0;
	while (fieldtoedit > 5 || fieldtoedit < 1) { // There's only 5 fields
		puts("Enter field to change");
		fieldtoedit = inputndigits(2, 2); // Only input 1 digit
	}

	switch(fieldtoedit) {
		case 1:
			puts("case 1");
			break;
		case 2:
			puts("case 2");
			break;
		case 3:
			puts("case 3");
			break;
		case 4:
			puts("case 4");
			break;
		case 5:
			puts("case 5");
			break;
		default:
			return;
	}

	free(pcsvindex);
	fclose(fptr);
	fptr = NULL;
}

void getSelection() {
	int choice;
	printf("Make a selection:\n");
	printf("c - Add Budget Category\n");
	printf("a - Add Transaction\n");
	printf("e - Edit Transaction\n"); 
	printf("r - Read CSV\n");
	printf("q - Quit\n");

	char *userstr = userinput(STDIN_SMALL_BUFF); // Must be free'd
	
	while (userstr == NULL) {
		getSelection();
	}

	if ((choice = upper(userstr)) == 0) {
		puts("Invalid character");
		free(userstr);
		userstr = NULL;
		getSelection();
	}
	
	free(userstr);
	userstr = NULL;

	switch (choice) {
		case 'C':
			indexcsv();
			break;
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
			getSelection();
	}
	return;
}

void addCategory() {

	return;
}

int main(int argc, char **argv) {
	FILE* fptr = fopen("data.csv", "a"); // Check that CSV exists
	if (fptr == NULL) {
		printf("File not found\n");
		fclose(fptr);
		return -1;
	}
	
	fclose(fptr);

	while (1) {
		getSelection();
		getchar();
	}
}
