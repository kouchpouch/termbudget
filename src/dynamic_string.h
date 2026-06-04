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

#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H

#include <stdlib.h>

struct d_string {
	size_t capacity;
	size_t len; // Without null byte
	char string[];
};

void debug_print_d_string(struct d_string *d_str);

/* Create a dynamic string, returns a null-checked pointer,
 * 'init_sz' will set the initial capacity of the string,
 * if 'init_sz' is 0, the initial capacity will be set to REALLOC_INCR */
struct d_string *create_d_string(size_t init_sz);

/* Contatenates 'str' to the end of the string pointed to by '**d_str.string',
 * null terminates the string and reallocs if required */
void concatenate_d_string(struct d_string **d_str, char *str, size_t len);

#endif
