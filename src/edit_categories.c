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

#include <ncurses.h>
#include <stdio.h>

#include "edit_categories.h"
#include "file_write.h"
#include "main.h"
#include "categories.h"
#include "parser.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "filemanagement.h"
#include "flags.h"

enum fields {
	EDIT_AMNT = 0,
	EDIT_TYPE,
	ZERO_AMNT,
	RENAME_CATG,
	MOVE_TO_TOP,
	DEL_CATG
};

typedef struct __replace_records_t {
	size_t line;
	size_t temp_line;
	size_t temp_idx;
	int changed;
} _replace_records_t;

/* Returns true if a duplicate is found, false if not */
bool duplicate_category_exists(_category_vec_t *psc, char *catg)
{
	for (size_t i = 0; i < psc->size; i++) {
		if (strcasecmp(psc->categories[i], catg) == 0) {
			return true;
		}
	}
	return false;
}

static void replace_category(_budget_tokens_t *bt, long b)
{
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr;
	int replace_line = boff_to_linenum_budget(b) + 1;
	char replace_str[LINE_BUFFER];

	budget_tokens_to_string(replace_str, sizeof(replace_str), bt);

	tmpfptr = replace_in_file(bfptr, replace_str, replace_line);

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

static void insert_category(_budget_tokens_t *bt, int insert_line)
{
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr;
	char insert_str[LINE_BUFFER];

	budget_tokens_to_string(insert_str, sizeof(insert_str), bt);
	
	tmpfptr = insert_into_file(bfptr, insert_str, insert_line);

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

static void delete_category(long b)
{
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr;
	size_t delete_line = boff_to_linenum_budget(b) + 1;

	tmpfptr = delete_in_file(bfptr, delete_line);

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

/*
 * If the user tries to delete a category that contains members, this warns
 * the user that the action cannot be completed.
 */
static void invalid_delete_warning(void)
{
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 3, "Cannot delete a category");
	mvwxcprintw(wptr, 4, "which contains records");
	nc_exit_window_key(wptr);
}

static bool nc_confirm_amount(double amt)
{
	WINDOW *wptr = create_input_subwindow();
	int c = 0;

	mvwxcprintw(wptr, 0, "Confirm Amount");
	mvwxcprintw(wptr, 3, "Is this amount correct?");
	mvwprintw(wptr, 4, (getmaxx(wptr) / 2) - (finlen(amt) / 2), "$%.2f", amt);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");

	while (c != KEY_F(QUIT)) {
		c = wgetch(wptr);
		switch(c) {

		case('y'):
		case('Y'):
			nc_exit_window(wptr);
			return true;

		case('n'):
		case('N'):
			nc_exit_window(wptr);
			return false;

		default:
			break;
		}
	}

	nc_exit_window(wptr);
	return false;
}

void mv_category_to_top(_catg_nodes_t **nodes, size_t i)
{
	if (i == 0) {
		return;
	}

	long first = nodes[0]->catg_fp;
	unsigned int insert_ln = boff_to_linenum_budget(first);
	_budget_tokens_t *bt = tokenize_budget_fpi(nodes[i]->catg_fp);

	delete_category(nodes[i]->catg_fp);
	insert_category(bt, insert_ln);

	free_budget_tokens(bt);
}

static int select_catg_field(void)
{
	const size_t n_options = 6;
	int retval;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * n_options));
	if (mp == NULL) {
		mem_alloc_fail();
	}

	mp->items = n_options;
	mp->title = "Editing Category";
	mp->strings[EDIT_AMNT] = "Edit Amount";
	mp->strings[EDIT_TYPE] = "Edit Type";
	mp->strings[ZERO_AMNT] = "Zero Out";
	mp->strings[RENAME_CATG] = "Rename";
	mp->strings[MOVE_TO_TOP] = "Move to top";
	mp->strings[DEL_CATG] = "Delete";

	retval = nc_input_menu(mp);

	free(mp);
	return retval;
}

static int rename_category(_budget_tokens_t *bt)
{
	_category_vec_t *psc = get_budget_catg_by_date(bt->m, bt->y);
	char *catg = nc_input_string("Renaming Category");
	if (catg == NULL) {
		return -1;
	}

	if (duplicate_category_exists(psc, catg)) {
		nc_message("That Category Already Exists");
		free(catg);
		free_categories(psc);
		return -1;
	} else {
		free_categories(psc);
		free(bt->catg);
		bt->catg = catg;
		return 0;
	}
}

static void free_lda(_transact_tokens_t **lda, size_t sz)
{
	for (size_t i = 0; i < sz; i++) {
		free_tokenized_record_strings(lda[i]);
		free(lda[i]);
		lda[i] = NULL;
	}

	free(lda);
}

/* Thinking out loud: We don't want this function to replace each category
 * one by one. Instead we want to have an array of line numbers to replace
 * all in one chunk. The records are already sorted, we just need a way to
 * store their current line number and some index to determine which
 * linedata to print at which line. Maybe a _transact_tokens_t **ld_head
 * indexed to match a vec of FPIs. */

/*---*/

/* Replaces the category field of records contained in nodes[node_idx] with
 * catg. */
static int replace_many_records_categories
(_catg_nodes_t **nodes, size_t node_idx, char *catg)
{
	_replace_records_t rr = {0};
	FILE *fptr;
	FILE *tmpfptr;
	char *str;
	char linebuff[LINE_BUFFER];
	size_t n_recs = nodes[node_idx]->data->size;
	size_t del_lines[n_recs];

	_transact_tokens_t **lda = malloc(sizeof(_transact_tokens_t) * n_recs);

	if (lda == NULL) {
		mem_alloc_fail();
	}

	for (size_t i = 0; i < n_recs; i++) {
		lda[i] = malloc(sizeof(_transact_tokens_t));
		if (lda[i] == NULL) {
			mem_alloc_fail();
		}
		tokenize_record_fpi(nodes[node_idx]->data->data[i], lda[i]);
		free(lda[i]->category);
		lda[i]->category = strndup(catg, strlen(catg));;
		del_lines[i] = boff_to_linenum(nodes[node_idx]->data->data[i]) + 1;
	}

	fptr = open_record_csv("r");
	tmpfptr = open_temp_csv();

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		rr.line++;
		rr.temp_line = 0;
		for (size_t i = 0; i < n_recs; i++) {
			if (del_lines[i] == rr.line) {
				rr.temp_line = del_lines[i];
				rr.temp_idx = i;
				break;
			}
		}

		if (rr.temp_line == 0) {
			fputs(str, tmpfptr);
		} else {
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
			lda[rr.temp_idx]->month, 
			lda[rr.temp_idx]->day, 
			lda[rr.temp_idx]->year, 
			lda[rr.temp_idx]->category, 
			lda[rr.temp_idx]->desc, 
			lda[rr.temp_idx]->transtype, 
			lda[rr.temp_idx]->amount
		   );
			rr.changed++;
		}
	}

	free_lda(lda, n_recs);

	if (debug_flag) {
		printw("CHANGED: %d\n", rr.changed);
		getch();
	}

	mv_tmp_to_record_file(tmpfptr, fptr);
	return rr.changed;
}

void nc_edit_category(long node_idx, long nmembers, _catg_nodes_t **nodes)
{
	long b = nodes[node_idx]->catg_fp;
	double tmp = 0.0;
	enum fields select;
	_budget_tokens_t *bt;

	select = select_catg_field();
	if (select < 0) {
		return;
	}

	bt = tokenize_budget_fpi(b);
	if (bt == NULL) {
		return;
	}

	switch (select) {

	case EDIT_AMNT:
		bt->amount = nc_input_budget_amount();
		if (bt->amount < 0.0) {
			goto err_fail;
		}
		bool conf = nc_confirm_amount(bt->amount);
		if (!conf) {
			goto err_fail;
		}
		break;

	case EDIT_TYPE:
		bt->transtype = nc_input_category_type();
		if (bt->transtype < 0.0) {
			goto err_fail;
		}
		break;

	case ZERO_AMNT:
		tmp = get_expenditures_per_category(bt);
		if (bt->transtype == TT_INCOME) {
			if (tmp < 0.0) {
				tmp = 0.0;
			}
			bt->amount = tmp;
		} else {
			if (-(tmp) < 0.0) {
				tmp = -0.0;
			}
			bt->amount = -(tmp);
		}
		break;

	case RENAME_CATG:
		if (rename_category(bt) < 0) {
			goto err_fail;
		}
		replace_many_records_categories(nodes, node_idx, bt->catg);
		break;

	case MOVE_TO_TOP:
		mv_category_to_top(nodes, node_idx);
		break;

	case DEL_CATG:
		if (nmembers > 0) {
			invalid_delete_warning();
			goto err_fail;
		}
		if (!nc_confirm_input("Confirm Delete")) {
			goto err_fail;
		}
		break;
	}

	if (select != DEL_CATG) {
		replace_category(bt, b);
	} else {
		delete_category(b);
	}

err_fail:
	free_budget_tokens(bt);
}
