#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	int fields = 0;
	int tokenFields = 0;
	char linebuff[1024];
	char* line;
	char* token;
	// First line to get field data
	line = fgets(linebuff, sizeof(linebuff), fptr);
	token = strtok(line, ",");
	while (token != NULL) {
		printf("%s \n", token);
		token = strtok(NULL, ","); // NULL for subsequent calls
		fields++;
	}
	// Loop through each line
	while (line) {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		token = strtok(line, ",");
		tokenFields = 0;
			while (token != NULL) {
				printf("%s \n", token);
				token = strtok(NULL, ","); // NULL for subsequent calls
				tokenFields++;
			}
		if (tokenFields != fields && tokenFields != 0) {
			printf("Missing data\n");
		}
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
				while (getchar() != '\n'); // Clear stdin buffer
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
