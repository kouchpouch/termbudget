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
#ifndef HELPER_H
#define HELPER_H
#include <stdbool.h>

/* Returns ltr ascii value to uppercase */
extern int upper(char *ltr);

/* Returns ltr ascii value to lowercase */
extern int lower(char *ltr);

/* Checks to see if a day exists in a month, 
 * returns maximum days in that month */
extern bool dayexists(int d, int m, int y);

/* Returns the len of n */
extern int intlen(int n);

#endif
