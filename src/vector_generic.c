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
#include <assert.h>
#include <string.h>

#include "main.h"
#include "vector_generic.h"

/* Returns the size of the new capacity, amortization with limits! */
static size_t calc_realloc_size(size_t current_size)
{
	if (current_size * 2 <= MAX_ALLOC) {
		return current_size * 2;
	} else {
		return current_size + MAX_ALLOC;
	}
}

/* Appends 'data' of size 'size' to v->data. Reallocs v->data if required,
 * sets the members of 'v' appropriately. */
void push_vec_generic(void *data, size_t size, struct vec_generic *v)
{
	void *tmp = NULL;
	assert(size == v->data_size);

	if (v->count >= v->capacity) {
		v->capacity = calc_realloc_size(v->capacity);
		tmp = realloc(v->data, v->data_size * v->capacity);
		if (tmp == NULL) {
			free(v->data);
			mem_alloc_fail();
		}
		v->data = tmp;
	}

	void *dest = (char *)v->data + (v->data_size * v->count);
	v->count++;
	memcpy(dest, data, size);
}

void *get_vec_generic(size_t idx, struct vec_generic *v)
{
	if (idx > v->count) {
		return NULL;
	}
	void *ret = (char *)v->data + (v->data_size * idx); 
	return ret;
}

/* Creates a malloc'd vec_generic * and sets the members appropriately. 
 * Mallocs void *data member to 'data_size' * 'init_count'.
 * If 'init_count' is zero, 'init_count' is set to REALLOC_INCR. */
struct vec_generic *create_vec_generic(size_t data_size, size_t init_count)
{
	assert(data_size != 0);
	if (init_count == 0) {
		init_count = REALLOC_INCR;
	}

	struct vec_generic *vg = malloc(sizeof(struct vec_generic));
	if (vg == NULL) {
		mem_alloc_fail();
	}

	void *arr = malloc(data_size * init_count);
	if (arr == NULL) {
		mem_alloc_fail();
	}

	vg->data_size = data_size;
	vg->capacity = init_count;
	vg->count = 0;
	vg->data = arr;

	return vg;
}

void free_vec_generic(struct vec_generic *v)
{
	if (v->data != NULL) {
		free(v->data);
	}
	v->data = NULL;

	if (v != NULL) {
		free(v);
	}
	v = NULL;
}
