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

#define SET_SIZE_MEMBERS(x) \
	x->capacity = REALLOC_INCR; \
	x->size = 0; \

struct vec_d *vec_d_create(void)
{
	size_t i;
	size_t sz = sizeof(struct vec_d) + (sizeof(long) * REALLOC_INCR);
	struct vec_d *v = malloc(sz);
	if (v == NULL) {
		mem_alloc_fail();
	}
	SET_SIZE_MEMBERS(v);
	for (i = 0; i < v->capacity; i++) {
		v->data[i] = 0;
	}
	return v;
}

struct catg_vec *catg_vec_create(void)
{
	size_t sz = sizeof(struct vec_d) + (sizeof(long) * REALLOC_INCR);
	struct catg_vec *v = malloc(sz);
	if (v == NULL) {
		mem_alloc_fail();
	}
	SET_SIZE_MEMBERS(v);
	return v;
}

void catg_vec_append(struct catg_vec **v, char *data)
{
	if (v == NULL) {
		return;
	}

	struct catg_vec *tmp = NULL;
	size_t multiplier;
	size_t realloc_sz;
	
	if ((*v)->capacity < (*v)->size + 1) {
		multiplier = (*v)->capacity / REALLOC_INCR;
		realloc_sz = sizeof(struct vec_d) + (((*v)->capacity + 
			         (REALLOC_INCR * multiplier)) *
					 sizeof(long));
		tmp = realloc(*v, realloc_sz);
		if (tmp == NULL) {
			free(v);
			mem_alloc_fail();
		} else {
			(*v) = tmp;
		}
		(*v)->capacity += REALLOC_INCR * multiplier;
	}

	(*v)->categories[(*v)->size] = data;
	(*v)->size++;
}

void vec_d_append_many(struct vec_d **v, long *data, size_t data_len)
{
	if (v == NULL) {
		return;
	}

	struct vec_d *tmp = NULL;
	size_t multiplier = 1;
	size_t realloc_sz;
	size_t i;
	
	if ((*v)->capacity < (*v)->size + 1 + data_len) {
		while ((REALLOC_INCR * multiplier) + (*v)->capacity < (*v)->size + 1 + data_len) {
			multiplier++;
		}
		realloc_sz = sizeof(struct vec_d) + (((*v)->capacity + 
			         (REALLOC_INCR * multiplier)) *
					 sizeof(long));
		tmp = realloc(*v, realloc_sz);
		if (tmp == NULL) {
			free(v);
			mem_alloc_fail();
		} else {
			(*v) = tmp;
		}
		(*v)->capacity += REALLOC_INCR * multiplier;
	}

	for (i = 0; i < data_len; i++) {
		(*v)->data[(*v)->size] = data[i];
		(*v)->size++;
	}
}

void vec_d_append(struct vec_d **v, long data)
{
	if (v == NULL) {
		return;
	}

	struct vec_d *tmp = NULL;
	size_t multiplier;
	size_t realloc_sz;
	
	if ((*v)->capacity < (*v)->size + 1) {
		multiplier = (*v)->capacity / REALLOC_INCR;
		realloc_sz = sizeof(struct vec_d) + (((*v)->capacity + 
			         (REALLOC_INCR * multiplier)) *
					 sizeof(long));
		tmp = realloc(*v, realloc_sz);
		if (tmp == NULL) {
			free(v);
			mem_alloc_fail();
		} else {
			(*v) = tmp;
		}
		(*v)->capacity += REALLOC_INCR * multiplier;
	}

	(*v)->data[(*v)->size] = data;
	(*v)->size++;
}
