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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "main.h"
#include "dynamic_string.h"
#include "flags.h"

void debug_print_d_string(struct d_string *d_str)
{
	for (size_t i = 0; i < d_str->len; i++) {
		if (d_str->string[i] == '\n') {
			printw("%s\n", "(NEWLINE)");
		} else {
			printw("%c", d_str->string[i]);
		}
	}
	refresh();
}

struct d_string *create_d_string(size_t init_sz)
{
	size_t alloc_sz;
	if (init_sz == 0) {
		alloc_sz = REALLOC_INCR;
	} else {
		alloc_sz = init_sz;
	}
	struct d_string *d_str = malloc(sizeof(struct d_string) + 
								(sizeof(char) * alloc_sz));
	if (d_str == NULL) {
		exit(1);
	}

	d_str->len = 0;
	d_str->capacity = alloc_sz;
	memset(d_str->string, 0, d_str->capacity);

	return d_str;
}

void concatenate_d_string(struct d_string **d_str, char *str, size_t len)
{
	struct d_string *tmp = NULL;

	/* Realloc if the data to insert is greater than the capacity allows
	 * len + 1 to hold null byte */
	if ((*d_str)->capacity < ((*d_str)->len + len + 1)) {
		(*d_str)->capacity *= 2;
		tmp = realloc(*d_str, sizeof(struct d_string) + 
				(sizeof(char) * (*d_str)->capacity));
		if (tmp == NULL) {
			exit(1);
		} else {
			*d_str = tmp;
		}
		if (debug_flag) {
			printf("%s %zu %s\n", "Realloc'd to", (*d_str)->capacity, "bytes");
		}
	}

	memcpy((*d_str)->string + (*d_str)->len, str, len);
	(*d_str)->len += len;
	(*d_str)->string[(*d_str)->len + 1] = '\0';
}
