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

void free_category_nodes(struct catg_nodes **nodes)
{
	int i = 0;
	while (1) {
		if (nodes[i]->next == NULL) {
			free(nodes[i]->data);
			free(nodes[i]);
			break;
		}
		free(nodes[i]->data);
		free(nodes[i]);
		i++;
	}

	free(nodes);
}

static void print_debug_node(struct catg_nodes *node)
{
	printw("Data sz: %lu, Next: %p, Prev: %p, FPI: %lu\n", 
		 node->data->size, 
		 (void *)node->next, 
		 (void *)node->prev, 
		 node->catg_fp);
}

void debug_category_nodes(struct catg_nodes **nodes)
{
	size_t i = 0;

	while (1) {
		if (nodes[i]->next == NULL) {
			print_debug_node(nodes[i]);
			break;
		} else {
			print_debug_node(nodes[i]);
		}
		i++;
	}
}

/*
 * Initializes struct catg_nodes.data. The data is a struct vec_d which contains all of
 * the file position byte offsets for the records that match the struct catg_nodes's
 * category.
 */
void init_category_nodes
(struct catg_nodes *node, struct vec_d *chunk, int m, int y)
{
	struct budget_tokens *budget_tokens = tokenize_budget_fpi(node->catg_fp);
	struct vec_d *recs = get_records_by_any(m, -1, y, 
										    budget_tokens->catg, 
										    NULL, -1, -1, chunk);
	node->data = recs;
	free_budget_tokens(budget_tokens);
}

/*
 * Returns a pointer to a pointer to the first struct catg_nodes in a doubly 
 * linked list of struct catg_nodess.
 */
struct catg_nodes **create_category_nodes(int m, int y) {
	struct vec_d *catgs_file_pos = get_budget_catg_by_date_bo(m, y);
	struct vec_d *chunk = get_records_by_mo_yr(m, y);
	unsigned long n = catgs_file_pos->size;
	struct catg_nodes **pnode = malloc(sizeof(struct catg_nodes *) * n);
	if (pnode == NULL) {
		mem_alloc_fail();
	}

	for (size_t i = 0; i < n; i++) {
		pnode[i] = malloc(sizeof(struct catg_nodes));
		if (pnode[i] == NULL) {
			mem_alloc_fail();
		}

		pnode[i]->catg_fp = catgs_file_pos->data[i];

		/* The first node has no previous node, set to NULL */
		if (i == 0) {
			pnode[0]->prev = NULL;
		} else if (i > 0) {
			pnode[i]->prev = pnode[i - 1];
			pnode[i - 1]->next = pnode[i];		
		}

		/* The last node has no next node, set to NULL */
		if (i == n - 1) {
			pnode[i]->next = NULL;
		}

		init_category_nodes(pnode[i], chunk, m, y);
	}

	free(chunk);
	free(catgs_file_pos);
	return pnode;
}

/* New Stuff Below
/////////////////////
/////////////////////
/////////////////////
/////////////////////
  New Stuff Below */

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

void free_catg_nodes(struct catg_node *head)
{
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
