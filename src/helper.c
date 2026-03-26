/*
 * This program is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program. If not, see <https://www.gnu.org/licenses/>. 
 */

#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "helper.h"

#define MAX_UPPER_ASCII 90
#define MIN_UPPER_ASCII 65
#define MAX_LOWER_ASCII 122
#define MIN_LOWER_ASCII 97

// --------------------------------------------------------------------- //
// 97 to 122 == lowercase, 65 to 90 == uppercase ASCII codes. 32 between //
// --------------------------------------------------------------------- //

int upper(char *ltr) {
	if (*ltr > SCHAR_MAX - 32) { // Check integer overflow
		;
	} else {
		return 0;
	}

	if (*ltr >= MIN_LOWER_ASCII && *ltr <= MAX_LOWER_ASCII) {
		return *ltr - 32; // To uppercase
	} 
	else if (*ltr >= MIN_UPPER_ASCII && *ltr <= MAX_UPPER_ASCII && *ltr < SCHAR_MAX) {
		return *ltr; // Already uppercase
	} 
	else {
		return 0;
	}
	return 0;
}

int lower(char *ltr) {
	if (*ltr < SCHAR_MAX) {
		;
	} else {
		return 0;
	}

	if (*ltr >= MIN_LOWER_ASCII && *ltr <= MAX_LOWER_ASCII)
	{
		return *ltr; // Already lowercase
	} 
	else if (*ltr >= MIN_UPPER_ASCII && *ltr <= MAX_UPPER_ASCII && *ltr > SCHAR_MAX - 32)
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

int intlen(int n) {
	if (n < 0) {
		n = -(n);
	}
	int len = 0;
	for (int i = 1; i <= n; i *= 10) {
		len++;
	}
	return len == 0 ? 1 : len;
}

int finlen(int n) {
	int add;
	n >= 0 ? (add = 3) : (add = 4);
	return intlen(n) + add;
}

long max_val(long a, long b) {
	return a >= b ? a : b;
}

long min_val(long a, long b) {
	return a >= b ? b : a;
}

int compare_for_sort(const void *a, const void *b) {
	return (*(long *)a - *(long *)b);
}

int size_to_int(size_t n) {
	if (n <= INT_MAX) {
		return (int)n;
	} else {
		return -1;
	}
}

bool int_to_size_safe(int n) {
	bool ret;
	n >= 0 ? (ret = true) : (ret = false);
	return ret;
}
