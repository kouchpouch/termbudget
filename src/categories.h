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

#ifndef CATEGORIES_H
#define CATEGORIES_H

#include <stdio.h>

#include "vector.h"

/*
 * _catg_nodes_t is a doubly linked list which includes members
 * of _catg_nodes_t pointer next and prev, long catg_fp stores the byte offset
 * of the category in BUDGET_DIR, and a _vector_t containing all
 * records in RECORD_DIR that match the category field at catg_fp
 * in BUDGET_DIR.

         |----------|
         |   Head   | // _catg_nodes_t **
         |----------|
              |
              V
         |------------|      |------------|      |------------|
         |    next    | ---> |    next    | ---> |    next    | ---> NULL
         |  1st Node  |      |  2nd Node  | .... |  nth Node  |
NULL <---|    prev    | <--- |    prev    | <--- |    prev    |
         |------------|      |------------|      |------------|
           |       |
           |       |_________
           V                |
        catg_fp             |
                            V
                      |----------|
                      |   size   |
                      | capacity |
                      |  []data  | // Each record's FPI that belongs to the
                      |----------| // node's category.
*/

typedef struct __catg_nodes_t _catg_nodes_t;

struct __catg_nodes_t { 
	_catg_nodes_t *next;
	_catg_nodes_t *prev;
	long catg_fp;
	_vector_t *data;
};

typedef struct __category_vec_t {
	size_t capacity;
	size_t size;
	char *categories[];
} _category_vec_t;

void free_categories(_category_vec_t *pc);
void free_category_nodes(_catg_nodes_t **nodes);
_catg_nodes_t **create_category_nodes(int m, int y);

#endif
