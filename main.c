#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_UPPER_ASCII 90
#define MIN_UPPER_ASCII 65
#define MAX_LOWER_ASCII 122
#define MIN_LOWER_ASCII 97

int upper(char* letter) {
	int asciiVal = *letter;
	// 97 to 122 == lowercase, 65 to 90 == uppercase ASCII codes. 32 between
	if (asciiVal >= MIN_LOWER_ASCII && asciiVal <= MAX_LOWER_ASCII) {
		return asciiVal - 32;
	} else if (asciiVal >= MIN_UPPER_ASCII && asciiVal <= MAX_UPPER_ASCII) {
		return asciiVal;
	// This is here just in case we need to convert to lowercase later
	} else {
		return asciiVal;
	}
	return 0;
}

int getMonth() {
	char b1[1024];
	printf("Enter Month:\n");

	// atoi(b1) check if b1 is an interger
	while (fgets(b1, 1024, stdin) == NULL || atoi(b1) == 0) {
		printf("Invalid Input\n");
//		exit(0);
	}

	int month = atoi(b1);
	return month;
}

int viewBudget() {
	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		exit(0);
	}

	struct {
		int month;
		int day;
		int year;
		char *category;
		char *desc;
		int transtype;
		float value;
	} rd;

	int userYear = 2025;

	int userMonth = getMonth();

	int buffsize = 128;
	char tmp[256];
	char *fields = (char *)calloc(buffsize, sizeof(char));
	if (fields == NULL) {
		exit(0);
	}

	fgets(fields, buffsize, fptr);

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
		char *charbuff = (char *)calloc(buffsize, sizeof(char));
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
			rd.month = atoi(token);
		}

		for (int i = 1; i < numFields; i++) {
			token = strtok(NULL, ",");
			if (token != NULL) {
				switch (i) {
					case 1:
						rd.day = atoi(token);
						break;
					case 2:
						rd.year = atoi(token);
						break;
					case 3:
						rd.category = token;
						break;
					case 4:
						rd.desc = token;
						break;
					case 5:
						rd.transtype = atoi(token);
						break;
					case 6:
						rd.value = atof(token);
						break;
				}
			}
		}

		if (rd.month == userMonth && rd.year == userYear) {
		printf("%d/%d/%d Category: %s, Description: %s, %d, $%.2f\n",
		 		rd.month, rd.day, rd.year, rd.category, rd.desc, rd.transtype,
		 		rd.value);
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
	char buff[64];
	char *ptr;
	while (fgets(buff, sizeof(buff) - 1, stdin) == NULL) {
		printf("Invalid input\n");
	}
	int choice = upper(buff);
	
	if (choice != -1) {
		switch (choice) {
			case 'C':
				printf("Adding Category\n");
				break;
			case 'E':
				printf("Add Expense\n");
				break;
			case 'V':
				printf("Budget overview:\n");
				viewBudget();
				break;
			case 'Q':
				printf("Quiting\n");
				break;
			default:
				printf("\"%c\" is not a valid option\n", *buff);
				printf("\n");
				getSelection();
		}
	} 
}

void addCategory() {

}

int main() {
	FILE* fptr = fopen("data.csv", "a");
	if (fopen("data.csv", "r") == NULL) {
		printf("File not found\n");
		return -1;
	}
	fclose(fptr);
	getSelection();
}
