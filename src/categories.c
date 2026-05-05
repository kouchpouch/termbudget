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

#include <stdlib.h>
#include <stdio.h>

#include "categories.h"
#include "main.h"
#include "parser.h"

void free_categories(struct catg_vec *pc)
{
	for (size_t i = 0; i < pc->size; i++) {
		free(pc->categories[i]);
	}
	free(pc);
}

struct catg_node *get_node_by_idx(struct catg_node *head, size_t idx)
{
	struct catg_node *curr = head;
	for (size_t i = 0; i < idx; i++) {
		if (curr == NULL) {
			puts("Attempted to access linked list out of bounds");
			exit(1);
		}
		curr = curr->next;
	}
	return curr;
}

int get_total_nodes(struct catg_node *head)
{
	int n = 0;
	struct catg_node *tmp = head;
	
	while (tmp != NULL) {
		tmp = tmp->next;
		n++;
	}

	return n;
}

static struct catg_node *create_catg_node(void)
{
	struct catg_node *n = malloc(sizeof(struct catg_node));
	if (n == NULL) {
		mem_alloc_fail();
	}
	return n;
}

static struct catg_node *init_head(void)
{
	struct catg_node *head = create_catg_node();
	head->next = NULL;
	head->prev = NULL;
	head->data = NULL;
	return head;
}

struct catg_node *append_catg_node
(struct catg_node *head, long catg_fp, struct vec_d *data)
{
	struct catg_node *ret = create_catg_node();
	struct catg_node *tmp = head;

	while (1) {
		if (tmp->next == NULL) {
			break;
		}
		tmp = tmp->next;
	}

	tmp->next = ret;
	ret->next = NULL;
	ret->prev = tmp;
	ret->catg_fp = catg_fp;
	ret->data = data;
	return ret;
}

struct catg_node *insert_catg_node
(struct catg_node **head, size_t idx, long catg_fp, struct vec_d *data)
{
	size_t i;
	struct catg_node *tmp = NULL;
	struct catg_node *prev = NULL;
	struct catg_node *ret = create_catg_node();

	tmp = *head;

	if (idx == 0) {
		ret->next = tmp;
		ret->prev = NULL;
		ret->catg_fp = catg_fp;
		ret->data = data;
		(*head)->prev = ret;
		*head = ret;
		return ret;
	}

	for (i = 0; i < idx; i++) {
		if (i == idx - 1) {
			prev = tmp;
		}
		if (tmp == NULL && i != idx) {
			free(ret);
			ret = NULL;
			return NULL;
		}
		tmp = tmp->next;
	}

	if (prev->next->next != NULL) {
		prev->next->prev = ret;
	}
	prev->next = ret;
	ret->next = tmp;
	ret->prev = prev;
	ret->catg_fp = catg_fp;
	ret->data = data;
	return ret;
}

void delete_catg_node(struct catg_node *head, size_t idx)
{
	size_t i;
	struct catg_node *tmp = head;
	struct catg_node *prev = NULL;

	for (i = 0; i < idx; i++) {
		if (i == idx - 1) {
			prev = tmp;
		}
		tmp = tmp->next;
		if (tmp == NULL) {
			puts("Failed to delete node: node does not exist");
			exit(1);
		}
	}

	if (tmp->next->next != NULL) {
		prev->next = tmp->next->next;
		prev->next->prev = prev;
	} else {
		prev->next = NULL;
	}

	free(tmp->data);
	free(tmp);
}

/* Verifies that the pointer "alleged_head" is the actual head of the linked
 * list. And if it isn't, set the alleged head to the real head. */
static void verify_head(struct catg_node **alleged_head)
{
	struct catg_node *curr = *alleged_head;
	while (curr->prev != NULL) {
		curr = (*alleged_head)->prev;
	}
	*alleged_head = curr;
}

void free_catg_nodes(struct catg_node *head)
{
	verify_head(&head);
	struct catg_node *curr = head;
	struct catg_node *to_free = NULL;
	struct vec_d *to_free_vec = NULL;
	
	while (1) {
		if (curr == NULL) {
			break;
		}
		to_free = curr;
		to_free_vec = curr->data;
		curr = curr->next;
		free(to_free);
		free(to_free_vec);
	}
}

void mv_catg_node_to_head(struct catg_node **head, size_t idx)
{
	if (idx == 0) {
		return;
	}
	struct catg_node *old = get_node_by_idx(*head, idx);
	struct catg_node *new_head = create_catg_node();
	new_head->data = old->data;
	new_head->next = *head;
	new_head->prev = NULL;

	(*head)->prev = new_head;

	old->prev->next = old->next;
	old->next->prev = old->prev;
	free(old);
	*head = new_head;
}

void debug_print_catg_node_data(struct catg_node *head)
{
	move(0,0);
	struct catg_node *tmp = head;
	size_t i = 0;

	while (1) {
		if (tmp == NULL) {
			break;
		}
		printw("Node: %lu, Addr: %p, Next: %p, Prev: %p, Data: %ld\n", 
		       i,
		       (void *)tmp, 
		       (void *)tmp->next,
		 	   (void *)tmp->prev, 
		 	   tmp->catg_fp);
		i++;
		tmp = tmp->next;
	}
}

struct catg_node *create_catg_node_list(int m, int y) {
	struct vec_d         *catg_fpis = get_budget_catg_by_date_bo(m, y);
	struct vec_d         *chunk = get_records_by_mo_yr(m, y);
	struct catg_node     *head = init_head();
	struct budget_tokens *budget_tokens = NULL;
	struct vec_d         *recs = NULL;

	for (size_t i = 0; i < catg_fpis->size; i++) {
		budget_tokens = tokenize_budget_fpi(catg_fpis->data[i]);
		recs = get_records_by_any(m, -1, y, 
							      budget_tokens->catg,
							      NULL, -1, -1, chunk);
		free_budget_tokens(budget_tokens);
		if (i > 0) {
			append_catg_node(head, catg_fpis->data[i], recs);
		} else {
			head->catg_fp = catg_fpis->data[i];
			head->data = recs;
		}
	}

	free(chunk);
	free(catg_fpis);

	return head;
}
