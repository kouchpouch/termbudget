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
 *
 * termbudget 2026
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#ifndef HELPER_H
#define HELPER_H
#include <stdbool.h>
#include <stdlib.h>

/* Returns ltr ascii value to uppercase */
int upper(char *ltr);

/* Returns ltr ascii value to lowercase */
int lower(char *ltr);

/* Checks to see if a day exists in a month, 
 * returns maximum days in that month */
bool dayexists(int d, int m, int y);

/* Returns the len of n */
int intlen(int n);

/* (fin)ancial (len)gth.
 * Returns the len of n + 3, to add a '.' and two decimal places */
int finlen(int n);

/* Returns the larger of a or b, or a if a == b */
long max_val(long a, long b);

/* Returns the smaller of a or b, or b if a == b */
long min_val(long a, long b);

int compare_for_sort(const void *a, const void *b);

/* Safely casts size_t to and integer. Returns -1 on unsafe cast */
int size_to_int(size_t n);

bool int_to_size_safe(int n);
#endif
