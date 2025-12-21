#include <stdio.h>
#include "helper.h"

#define MAX_UPPER_ASCII 90
#define MIN_UPPER_ASCII 65
#define MAX_LOWER_ASCII 122
#define MIN_LOWER_ASCII 97

int upper(char* ltr) {
	int asciiVal = *ltr;
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

int lower(char* ltr) {
	int asciiVal = *ltr;
	// 97 to 122 == lowercase, 65 to 90 == uppercase ASCII codes. 32 between
	if (asciiVal >= MIN_LOWER_ASCII && asciiVal <= MAX_LOWER_ASCII) {
		return asciiVal;
	} else if (asciiVal >= MIN_UPPER_ASCII && asciiVal <= MAX_UPPER_ASCII) {
		return asciiVal + 32;
	// This is here just in case we need to convert to lowercase later
	} else {
		return asciiVal;
	}
	return 0;
}
