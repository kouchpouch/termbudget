#include <stdio.h>
#include <limits.h>
#include <stdbool.h>
#include "helper.h"

#define MAX_UPPER_ASCII 90
#define MIN_UPPER_ASCII 65
#define MAX_LOWER_ASCII 122
#define MIN_LOWER_ASCII 97

// --------------------------------------------------------------------- //
// 97 to 122 == lowercase, 65 to 90 == uppercase ASCII codes. 32 between //
// --------------------------------------------------------------------- //

int upper(char* ltr) {
	if (*ltr > SCHAR_MAX - 32) { // c CANNOT BE LARGER THAN 95
		;
	} else {
		return 0;
	}

	if (*ltr >= MIN_LOWER_ASCII && 
		*ltr <= MAX_LOWER_ASCII)
	{
		return *ltr - 32; // To uppercase
	} 
	else if (*ltr >= MIN_UPPER_ASCII && 
			 *ltr <= MAX_UPPER_ASCII &&
			 *ltr < SCHAR_MAX)
	{
		return *ltr; // Already uppercase
	} 
	else {
		return 0;
	}
	return 0;
}

int lower(char* ltr) {
	if (*ltr < SCHAR_MAX) {
		;
	} else {
		return 0;
	}

	if (*ltr >= MIN_LOWER_ASCII && 
		*ltr <= MAX_LOWER_ASCII)
	{
		return *ltr; // Already upppercase
	} 
	else if (*ltr >= MIN_UPPER_ASCII && 
			 *ltr <= MAX_UPPER_ASCII &&
			 *ltr > SCHAR_MAX - 32)
	{
		return *ltr + 32; // To lowercase
	} 
	else {
		return 0;
	}
	return 0;
}

bool dayexists(int d, int m, int y) {
	int thirtyones[] = {1, 3, 5, 7, 8, 10, 12};

	if (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) {
		if (d > 0 && d <= 29) {
			return true;
		} else {
			return false;
		}
	} else if (m == 2) {
		if (d > 0 && d <= 28) {
			return true;
		} else {
			return false;
		}
	}

	for (int i = 0; i < 7; i++) {
		if (m == thirtyones[i] && (d > 0 && d <= 31)) {
			return true;
		}
	}

	for (int i = 0; i < 7; i++) {
		if (m != thirtyones[i] && (d > 0 && d <= 30)) {
			return true;
		}
	}

	return false;
}
