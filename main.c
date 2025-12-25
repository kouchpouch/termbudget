#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "helper.h"

#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8

struct Linedata {
	unsigned int month;
	unsigned int day;
	unsigned int year;
	char *category;
	char *desc;
	unsigned int transtype;
	float amount;
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

int getMonth() {
	printf("Enter Month:\n");
	char *userstr = userinput(STDIN_LARGE_BUFF); // Must be free'd
	unsigned char month = atoi(userstr);
	free(userstr);
	return month;
}

void addexpense() {
	FILE* fptr = fopen("data.csv", "r+"); // Open to read and write
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	fseek(fptr, 0L, SEEK_END);

	struct Linedata userlinedata_, *uld = &userlinedata_;
	char userline[512] = {'0'};

	puts("Year:");
	char *yearstr = userinput(STDIN_SMALL_BUFF);
	while (yearstr == NULL) {
		yearstr = userinput(STDIN_SMALL_BUFF);
	}
	int year = atoi(yearstr);

	puts("Month:");
	char *monthstr = userinput(STDIN_SMALL_BUFF);
	while (monthstr == NULL) {
		monthstr = userinput(STDIN_SMALL_BUFF);
	}

	int month = atoi(monthstr);

	while (month <= 0 || month > 12) {
		puts("MONTH ENTRY NOT VALID");
		free(monthstr);
		monthstr = userinput(STDIN_SMALL_BUFF);
		if (monthstr == NULL) {
			monthstr = userinput(STDIN_SMALL_BUFF);
		}
	    month = atoi(monthstr); 
	}

	puts("Day:");
	char *daystr = userinput(STDIN_SMALL_BUFF);
	while (daystr == NULL) {
		daystr = userinput(STDIN_SMALL_BUFF);
	}

	int day = atoi(daystr);

	while (dayexists(day, month, year) == false) {
		puts("DAY ENTRY NOT VALID");
		free(daystr);
		daystr = userinput(STDIN_SMALL_BUFF);
		if (daystr == NULL) {
			daystr = userinput(STDIN_SMALL_BUFF);
		}
	    day = atoi(daystr); 
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
	char *transstr = userinput(STDIN_SMALL_BUFF);

	int trans = atoi(transstr);
	while (trans != 1 && trans != 2) {
		printf("INVALID, ENTER 1 OR 2, YOU ENTERED: %d\n", trans);
		free(transstr);
		transstr = NULL;
		transstr = userinput(STDIN_SMALL_BUFF);
		if (transstr == NULL) break;
		trans = atoi(transstr);
	}

	puts("$ Amount:");
	char *amountstr = userinput(STDIN_LARGE_BUFF);
	float amount = atof(amountstr);

	int len = strlen(categorystr);
	char *strings[] = { monthstr, daystr, yearstr, categorystr, descstr, transstr, amountstr };

	for (int i = 0; i < 7; i++) { // Remove all newlines if they exist
		int len = strlen(strings[i]);
		if (strings[i][len - 1] == '\n') {
			strings[i][len - 1] = 0;
		}
	}

	puts("Verify Data is Correct:");
	printf("%s/%s/%s Category: %s Description: %s, %s, %s\n", monthstr, daystr,
		yearstr, categorystr, descstr, transstr, amountstr);

	char *confirmstr;
	do {
		puts("Y/N");
		confirmstr = userinput(STDIN_SMALL_BUFF);
	} while (confirmstr == NULL);

	char cnfm = *confirmstr;

	uld->month = month;
	uld->day = day;
	uld->year = year;
	uld->category = categorystr;
	uld->desc = descstr;
	uld->transtype = trans - 1;
	uld->amount = amount;

	fclose(fptr);

	free(daystr);
	free(monthstr);
	free(yearstr);
	free(categorystr);
	free(descstr);
	free(transstr);
	free(amountstr);
	free(confirmstr);

	return;
}

void rcsv() {
	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	struct Linedata linedata_, *ld = &linedata_;

	int userYear = 2025;
	int userMonth = getMonth();

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

		if (ld->month == userMonth && ld->year == userYear) {
		printf("%d/%d/%d Category: %10s Description: %10s, \t%5d, \t$%5.2f\n",
		 	ld->month, ld->day, ld->year, ld->category, 
		 	ld->desc, ld->transtype, ld->amount);
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
		getSelection();
	}
	
	free(userstr);

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
			break;
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

int main() {
	FILE* fptr = fopen("data.csv", "a"); // Check that CSV exists
	if (fptr == NULL) {
		printf("File not found\n");
		fclose(fptr);
		return -1;
	}
	fclose(fptr);
	getSelection();
}
