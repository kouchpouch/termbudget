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

#ifndef CATEGORIES_H
#define CATEGORIES_H

#include <stdio.h>
#include <ncurses.h>

#include "vector.h"

struct catg_node { 
	struct catg_node *next;
	struct catg_node *prev;
	struct vec_d *data;
	long catg_fp;
};

struct catg_vec {
	size_t capacity;
	size_t size;
	char *categories[];
};

void free_categories(struct catg_vec *pc);

/*
 struct catg_node is a node to be placed inside of a doubly linked list.
 The doubly linked list includes members of struct catg_nodes pointer next and 
 prev, long catg_fp stores the byte offset of the category in BUDGET_DIR, and 
 a struct vec_d containing all records in RECORD_DIR that match the category 
 field at catg_fp in BUDGET_DIR.

           |----HEAD----|      |------------|      |----TAIL----|
           |   next &   | ---> |   next &   | ---> |   next &   | ---> NULL
           |  1st Node  |      |  2nd Node  | .... |  nth Node  |
  NULL <---|   prev &   | <--- |   prev &   | <--- |   prev &   |
           |------------|      |------------|      |------------|
             |       |
         .catg_fp    |__.data__
             |                |
             V                |
         catg file            |
	   pos indicator          V
                        |----------|
                        |   size   |
                        | capacity |
                        |  []data  | // Each record's FPI that belongs to the
                        |----------| // node's category.
*/

/* Returns a pointer to the node at index "idx" */
struct catg_node *get_node_by_idx(struct catg_node *head, size_t idx);

/* Returns number of nodes in the linked list */
size_t get_total_nodes(struct catg_node *head);

/* Appends a struct catg_node to the tail of the doubly linked list. */
struct catg_node *append_catg_node(struct catg_node *head,
								   long catg_fp,
								   struct vec_d *data);

/* Inserts a struct catg_node to the index "idx" of the doubly linked list. */
struct catg_node *insert_catg_node(struct catg_node **head,
								   size_t idx,
								   long catg_fp,
								   struct vec_d *data);

/* Deletes node at index "idx" of the doubly linked list. */
void delete_catg_node(struct catg_node *head, size_t idx);

/* Frees the entire doubly linked list */
void free_catg_nodes(struct catg_node *head);

/* Moves the node and data of node at "idx" to the head of the linked list */
void mv_catg_node_to_head(struct catg_node **head, size_t idx);

/* Prints debug data of the entire doubly linked list */
void debug_print_catg_node_data(struct catg_node *head);

/* Creates all nodes and initializes the members of each node from data found 
 * in BUDGET_DIR and RECORD_DIR, returns the HEAD of the list */
struct catg_node *create_catg_node_list(int m, int y);

#endif
