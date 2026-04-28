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

/*
 * Initializes struct catg_nodes.data. The data is a struct vec_t which contains all of
 * the file position byte offsets for the records that match the struct catg_nodes's
 * category.
 */
static void init_category_nodes
(struct catg_nodes *node, struct vec_t *chunk, int m, int y)
{
	_budget_tokens_t *budget_tokens = tokenize_budget_fpi(node->catg_fp);
	struct vec_t *recs = get_records_by_any(m, -1, y, budget_tokens->catg, NULL, -1, -1, chunk);
	node->data = recs;
	free_budget_tokens(budget_tokens);
}

/*
 * Returns a pointer to a pointer to the first struct catg_nodes in a doubly 
 * linked list of struct catg_nodess.
 */
struct catg_nodes **create_category_nodes(int m, int y) {
	struct vec_t *catgs_file_pos = get_budget_catg_by_date_bo(m, y);
	struct vec_t *chunk = get_records_by_mo_yr(m, y);
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
