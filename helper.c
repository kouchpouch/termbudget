#include <limits.h>
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
