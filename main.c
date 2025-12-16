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
		return 0;
	}
	return 0;
}

void getSelection() {
	int ascii;
	char userInput;
	userInput = getchar();
	char* ptr = &userInput;
	int choice = upper(ptr);
	printf("SELECTION: %c\n", choice);
	if (choice != -1) {
		switch (choice) {
			case 'C':
				printf("Adding Category\n");
				break;
			case 'Q':
				printf("Quiting\n");
				break;
			default:
				printf("NOT AN OPTION\n");
				while (getchar() != '\n'); // Clear stdin buffer
				getSelection();
		}
	} 
}

void addCategory() {

}

int main() {
	getSelection();
}
