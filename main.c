#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "helper.h"

int getMonth() {
	size_t b_sz = 8;
	char buff[b_sz];
	printf("Enter Month:\n");

	// atoi(b1) check if b1 is an interger
	while (fgets(buff, b_sz - 1, stdin) == NULL || atoi(buff) == 0) {
		printf("Invalid Input\n");
		exit(0);
	}

	unsigned char month = atoi(buff);
	return month;
}

int viewBudget() {
	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	// Line data
	struct {
		unsigned int month;
		unsigned int day;
		unsigned int year;
		char *category;
		char *desc;
		unsigned int transtype;
		float value;
	} linedata_, *ld = &linedata_; 

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

	while (1 == 1) {
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
						ld->value = atof(token);
						break;
				}
			}
		}

		if (ld->month == userMonth && ld->year == userYear) {
		printf("%d/%d/%d Category: %10s Description: %10s, \t%5d, \t$%5.2f\n",
		 	ld->month, ld->day, ld->year, ld->category, 
		 	ld->desc, ld->transtype, ld->value);
		}

		free(charbuff);
		charbuff = NULL;
	}
	fclose(fptr);
	return 0;
}

void getSelection() {
	printf("Make a selection:\n");
	printf("m - Select Month\n");
	printf("c - Add budget category\n");
	printf("e - Add transaction\n");
	printf("v - View Budget Overview\n");
	printf("q - Quit\n");
	int ascii;
	int choice;
	char buff[8];
	char *ptr;

	while (fgets(buff, sizeof(buff) - 1, stdin) == NULL) {
		printf("Invalid input\n");
	}

	choice = upper(buff);

	if (choice != 0) {
		switch (choice) {
			case 'C':
				printf("Adding Category\n");
				break;
			case 'E':
				printf("Add Expense\n");
				break;
			case 'V':
				printf("-*-Budget overview-*-\n");
				viewBudget();
				break;
			case 'Q':
				printf("Quiting\n");
				break;
			default:
				printf("\"%c\" is not a valid option\n", choice);
				printf("\n");
				getSelection();
		}
	} else {
		printf("Invalid Character\n");
		getSelection();
	}
}

void addCategory() {

}

int main() {
	FILE* fptr = fopen("data.csv", "a");
	if (fptr == NULL) {
		printf("File not found\n");
		fclose(fptr);
		return -1;
	}
	fclose(fptr);
	getSelection();
}
