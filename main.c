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

int viewBudget() {
	FILE* fptr = fopen("data.csv", "r");
	if (fptr == NULL) {
		printf("Unable to open file\n");
		return -1;
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

	int userMonth = 4;
	int userYear = 2025;


	int buffsize = 128;
	char tmp[256];
	char *fields = (char *)calloc(buffsize, sizeof(char));
	if (fields == NULL) {
		exit(-1);
	}

	fgets(fields, buffsize, fptr);
	printf("%s", fields);

	/* Count number of fields
	* Init to 1 to count first field where no comma is present */
	int numFields = 1; 

	for (int i = 0; i < strlen(fields); i++) {
		if (fields[i] == ',') {
			numFields++;	
		}
	}

	printf("Num of Fields %d\n", numFields);
	free(fields);
	fields = NULL;

	/* Read the rest of lines in the CSV after the header */

	while (1 == 1) {
		char *charbuff = (char *)calloc(buffsize, sizeof(char));
		if (charbuff == NULL) {
			exit(1);
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
			printf("Month %d\n", rd.month);
		}

		for (int i = 1; i < numFields; i++) {
			token = strtok(NULL, ",");
			if (token != NULL) {
				switch (i) {
					case 1:
						printf("Day: %s\n", token);
						break;
					case 2:
						printf("Year: %s\n", token);
						break;
					case 3:
						printf("Category: %s\n", token);
						break;
					case 4:
						printf("Description: %s\n", token);
						break;
					case 5:
						printf("Transaction Type: %s\n", token);
						break;
					case 6:
						printf("Value: %s\n", token);
						break;
				}
			}
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
	char userInput;
	userInput = getchar();
	char* ptr = &userInput;
	int choice = upper(ptr);
	
	if (choice != -1) {
		switch (choice) {
			case 'M':
				printf("Select Month\n");
				break;
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
				printf("\"%c\" is not a valid option\n", userInput);
				while (getchar() != '\n'); // Clear STDIN buffer
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
