#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>
#include "helper.h"

#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8
#define MAX_LEN_DAYMON 3 // With \0
#define MAX_LEN_YEAR 5 // With \0
#define MIN_LEN_DAYMON MAX_LEN_DAYMON - 1

struct Linedata {
	unsigned int month;
	unsigned int day;
	unsigned int year;
	char *category;
	char *desc;
	unsigned int transtype;
	float amount;
	int linenum;
};

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

void addexpense() {
	unsigned int year;
	unsigned int month;
	unsigned int day;
	FILE* fptr = fopen("data.csv", "r+"); // Might have to mess with this mode
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	fseek(fptr, 0L, SEEK_END);

	struct Linedata userlinedata_, *uld = &userlinedata_;

	puts("Enter Year");
	while((year = inputndigits(MAX_LEN_YEAR, MAX_LEN_YEAR)) == -1);

	puts("Enter Month");
	while((month = inputndigits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Invalid");
	}

	puts("Enter Day");
	while((day = inputndigits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1 ||
			dayexists(day, month, year) == false) {
		if (dayexists(day, month, year) == false) { // Calling this twice is GROSS but I'm kinda stupid
			puts("Invalid Day");
		}
	}

	puts("Category:");
	char *categorystr = userinput(STDIN_LARGE_BUFF);	
	while (categorystr == NULL) {
		puts("CATEGORY ENTRY NOT VALID");
		categorystr = userinput(STDIN_LARGE_BUFF);
	}

	puts("Description:");
	char *descstr = userinput(STDIN_LARGE_BUFF);	
	while (descstr == NULL) {
		puts("CATEGORY ENTRY NOT VALID");
		descstr = userinput(STDIN_LARGE_BUFF);
	}

	puts("Choose 1 or 2");
	puts("1. Expense"); // 0 is an expense in the CSV
	puts("2. Income"); // 1 is an income in the CSV
	char *transstr = userinput(STDIN_SMALL_BUFF); // Subtract 1 from the user
	// input

	int trans = atoi(transstr);
	while (trans != 1 && trans != 2) {
		printf("INVALID, ENTER 1 OR 2, YOU ENTERED: %d\n", trans);
		free(transstr);
		transstr = userinput(STDIN_SMALL_BUFF);
		if (transstr == NULL) break; // This whole thing needs to be in its own
		// function
		trans = atoi(transstr);
	}

	puts("$ Amount:");
	char *amountstr = userinput(STDIN_LARGE_BUFF);
	if (amountstr == NULL) {
		amountstr = userinput(STDIN_LARGE_BUFF);
	}
	float amount = atof(amountstr);

	char *strings[] = { categorystr, descstr, transstr, amountstr };
	for (int i = 0; i < 4; i++) { // Remove all newlines if they exist
		int len = strlen(strings[i]);
		printf("%s\n", strings[i]);
		if (strings[i][len - 1] == '\n') {
			strings[i][len - 1] = 0;
		}
	}

	puts("Verify Data is Correct:");
	puts("Y/N");
	printf(
		"%d,%d,%d,%s,%s,%d,%.2f\n", 
		month,
		day,
		year,
		categorystr,
		descstr,
		(trans - 1),
		amount
	);

	int result = confirmInput();
	if (result == 1) {
		puts("TRUE");
		uld->month = month;
		uld->day = day;
		uld->year = year;
		uld->category = categorystr;
		uld->desc = descstr;
		uld->transtype = trans - 1;
		uld->amount = amount;
	} else if (result == 0) {
		puts("FALSE");
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
	// DEBUG ONLY
	puts("CLEANUP");

	fclose(fptr);

//	free(daystr);
//	free(monthstr);
//	free(yearstr);
	free(categorystr);
	free(descstr);
	free(transstr);
	free(amountstr);

	return;
}

void rcsv() {
	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	struct Linedata linedata_, *ld = &linedata_;

	int userYear;
	int userMonth;
	int linenum = 1; // The first line will be line #1, not zero.

	puts("Enter Year");
	while((userYear = inputndigits(MAX_LEN_YEAR, MAX_LEN_YEAR)) == -1);

	puts("Enter Month");
	while((userMonth = inputndigits(MAX_LEN_DAYMON, MIN_LEN_DAYMON)) == -1
		|| userMonth <= 0
		|| userMonth > 12) {
		puts("Invalid");
	}

	size_t buffsize = 128;
	char *fields = (char *)malloc(buffsize * sizeof(char));
	if (fields == NULL) {
		exit(0);
	}

	if (fgets(fields, buffsize, fptr) == NULL) {
		free(fields);
		fields = NULL;
		exit(0);
	}

	/* Count number of fields
	* Init to 1 to count first field where no comma is present */
	int numFields = 1; 

	for (int i = 0; i < strlen(fields); i++) {
		if (fields[i] == ',') {
			numFields++;	
		}
	}

	free(fields);
	fields = NULL;
	/* Read the rest of lines in the CSV after the header */

	while (true) { // NEVER
		char *charbuff = (char *)malloc(buffsize * sizeof(char));
		if (charbuff == NULL) {
			exit(0);
		}
		/* For each line, tokenize the fields to retrieve each cell's data */

		if (fgets(charbuff, buffsize, fptr) == NULL) {
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

		if (ld->month == userMonth && ld->year == userYear) {
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
		}

		free(charbuff);
		charbuff = NULL;
	}
	fclose(fptr);
	fptr = NULL;
}

void getSelection() {
	int choice;
	printf("Make a selection:\n");
	printf("m - Select Month\n");
	printf("c - Add budget category\n");
	printf("e - Add transaction\n");
	printf("v - Read CSV\n");
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
			printf("Adding Category\n");
			break;
		case 'E':
			printf("Add Expense\n");
			addexpense();
			break;
		case 'V':
			printf("-*-READ CSV-*-\n");
			rcsv();
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

int main(char argc, char **argv) {
	FILE* fptr = fopen("data.csv", "a"); // Check that CSV exists
	if (fptr == NULL) {
		printf("File not found\n");
		fclose(fptr);
		return -1;
	}

	fclose(fptr);

	while (1) {
		getSelection();
	}
}
