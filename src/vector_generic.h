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

#ifndef VECTOR_GENERIC_H
#define VECTOR_GENERIC_H

#include <stdio.h>

/*             
 *              struct vec_generic
 * +------------------------------------------+
 * | .data_size | .count | .capacity | void * |
 * +------------------------------------------+
 *                 malloc'd              |
 *             +-------------------------+
 *             |
 *             V
 * +-------------------------+-------------------------+
 * | data of size .data_size | data of size .data_size |
 * +-------------------------+-------------------------+
 *  malloc'd separately. This is not actually 'chunked' as shown.
 *  Access is manual, see comment block below.
 */

/* 
 * Accessing an 'element' inside of the void *data memory is done with
 * pointer arithmetic, casting the void * to a char * to increment by
 * one byte at a time... because char = 1 byte;
 *
 * Pseudo-code:
 * void *destination = (char *)data + (data size * index)
 * +------+------+------+------+------+------+------+------+------+------+
 * | data | data | data | data | data | data | data | data | data | data |
 * +------+------+------+------+------+------+------+------+------+------+
 * ^      ^                                                       ^
 * |      |                                                       |
 * |      +-------------------+                                   |
 * Initial Pointer 'data'.    |                                   |
 *                            'data' plus data size               |
 *        +-------------------------------------------------------+
 *        |
 *        The last 'element' of the memory == .count == \
 *        (char *)data + (data size * count).
 *
 * For each 'index', add the data size to the pointer.
 */

/* For each item of size 'data_size' the memory allocated at a generic 
 * vector's data member is casted the argument 'T' and exposed as 
 * 'item' */
#define VEC_GENERIC_FOREACH(T, item, vec) \
	T item = vec->data; \
	for (size_t i = 0; \
		 i < vec->count; \
		 i++, item = (void *)((char *)vec->data + (vec->data_size * i))) \

/* Reverse of VEC_GENERIC_FOREACH, conditional checks if i wraps around. */
#define VEC_GENERIC_FOREACH_REVERSE(T, item, vec) \
	T item = (void *)((char *)vec->data + (vec->data_size * (vec->count - 1))); \
	for (size_t i = vec->count - 1; \
		 i < vec->count; \
		 i--, item = (void *)((char *)vec->data + (vec->data_size * i))) \

struct vec_generic {
	size_t data_size; /* Size of each element */
	size_t count;     /* Number of elements with data */
	size_t capacity;  /* As number of elements that the 'da' can hold */
	void *data;
}; 

void push_vec_generic(void *data, size_t size, struct vec_generic *v);
struct vec_generic *create_vec_generic(size_t data_size, size_t init_count);
void *get_vec_generic_reverse(size_t idx, struct vec_generic *v);
void *get_vec_generic(size_t idx, struct vec_generic *v);
void free_vec_generic(struct vec_generic *v);

#endif
