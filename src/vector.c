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

#include "main.h"
#include "vector.h"

struct vec_d *vec_d_create(void)
{
	size_t i;
	size_t sz = sizeof(struct vec_d) + (sizeof(long) * REALLOC_INCR);
	struct vec_d *v = malloc(sz);
	if (v == NULL) {
		mem_alloc_fail();
	}
	v->capacity = REALLOC_INCR;
	v->size = 0;
	for (i = 0; i < v->capacity; i++) {
		v->data[i] = 0;
	}
	return v;
}

void vec_d_append(struct vec_d *v, long data)
{
	if (v == NULL) {
		return;
	}

	struct vec_d *tmp = NULL;
	
	if (v->capacity <= v->size + 1) {
		tmp = realloc(v, sizeof(struct vec_d) + 
					  ((v->capacity + REALLOC_INCR) * sizeof(long)));
		if (tmp == NULL) {
			free(v);
			mem_alloc_fail();
		}
		v = tmp;
		v->capacity += REALLOC_INCR;
	}

	v->data[v->size] = data;
	v->size++;
}
