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

#include <ncurses.h>
#include <stdio.h>

#include "edit_categories.h"
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
	DEL_CATG
};

static void replace_category(struct BudgetTokens *bt, long b) {
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();
	char *str;
	char linebuff[LINE_BUFFER];
	size_t line = boff_to_linenum_budget(b) + 2;
	size_t linenum = 0;

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), bfptr);
		if (str == NULL) {
			break;
		}
		linenum++;	
		if (linenum != line) {
			fputs(str, tmpfptr);
		} else if (linenum == line) {
			fprintf(tmpfptr, "%d,%d,%s,%d,%.2f\n", 
				bt->m, 
				bt->y, 
				bt->catg, 
				bt->transtype,
				bt->amount);
		}
	}

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

static void insert_category(struct BudgetTokens *bt, unsigned int ln) {
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();
	
	char linebuff[LINE_BUFFER];
	char *line;
	unsigned int linenum = 0;

	while ((line = fgets(linebuff, sizeof(linebuff), bfptr)) != NULL) {
		linenum++;	
		if (linenum != ln) {
			fputs(line, tmpfptr);
		} else if (linenum == ln) {
			fputs(line, tmpfptr);
			fprintf(tmpfptr, "%d,%d,%s,%d,%.2f\n", 
				bt->m,
				bt->y,
				bt->catg,
				bt->transtype,
				bt->amount
		   );
		}
	}

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

static void delete_category(long b) {
	FILE *bfptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();
	char *str;
	char linebuff[LINE_BUFFER];
	size_t line = boff_to_linenum_budget(b) + 2;
	size_t linenum = 0;

	while (1) {
		str = fgets(linebuff, sizeof(linebuff), bfptr);
		if (str == NULL) {
			break;
		}
		linenum++;	
		if (linenum != line) {
			fputs(str, tmpfptr);
		}
	}

	mv_tmp_to_budget_file(tmpfptr, bfptr);
}

/*
 * If the user tries to delete a category that contains members, this warns
 * the user that the action cannot be completed.
 */
static void invalid_delete_warning(void) {
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 3, "Cannot delete a category");
	mvwxcprintw(wptr, 4, "which contains records");
	nc_exit_window_key(wptr);
}

static bool nc_confirm_amount(double amt) {
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 0, "Confirm Amount");
	mvwxcprintw(wptr, 3, "Is this amount correct?");
	mvwprintw(wptr, 4, (getmaxx(wptr) / 2) - (finlen(amt) / 2), "$%.2f", amt);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");

	int c = 0;

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

void mv_category_to_top(CategoryNode **nodes, size_t i) {
	if (i == 0) {
		return;
	}

	long first = nodes[0]->catg_fp;
	unsigned int insert_ln = boff_to_linenum_budget(first) + 1;
	struct BudgetTokens *bt = tokenize_budget_byte_offset(nodes[i]->catg_fp);

	delete_category(nodes[i]->catg_fp);
	insert_category(bt, insert_ln);

	free_budget_tokens(bt);
}

static int select_catg_field(void) {
	size_t n_options = 5;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * n_options));

	if (mp == NULL) {
		memory_allocate_fail();
	}

	mp->items = n_options;
	mp->title = "Editing Category";
	mp->strings[EDIT_AMNT] = "Edit Amount";
	mp->strings[EDIT_TYPE] = "Edit Type";
	mp->strings[ZERO_AMNT] = "Zero Out";
	mp->strings[RENAME_CATG] = "Rename";
	mp->strings[DEL_CATG] = "Delete";

	int retval = nc_input_menu(mp);

	free(mp);
	return retval;
}

static int rename_category(struct BudgetTokens *bt) {
	char *catg = nc_input_string("Renaming Category");
	if (catg == NULL) {
		return -1;
	}

	struct Categories *psc = get_budget_catg_by_date(bt->m, bt->y);

	if (check_dup_catg(psc, catg)) {
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

static void free_lda(struct LineData **lda, size_t sz) {
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
 * linedata to print at which line. Maybe a struct LineData **ld_head
 * indexed to match a vec of FPIs. */
//---//
/* Replaces the category field of records contained in nodes[node_idx] with
 * catg. */
static int replace_many_records_categories
(CategoryNode **nodes, size_t node_idx, char *catg)
{
	size_t n_recs = nodes[node_idx]->data->size;
	size_t del_lines[n_recs];
	struct LineData **lda = malloc(sizeof(struct LineData) * n_recs);
	if (lda == NULL) {
		memory_allocate_fail();
	}

	for (size_t i = 0; i < n_recs; i++) {
		lda[i] = malloc(sizeof(struct LineData));
		if (lda[i] == NULL) {
			memory_allocate_fail();
		}
		tokenize_record_fpi(nodes[node_idx]->data->data[i], lda[i]);
		free(lda[i]->category);
		lda[i]->category = strndup(catg, strlen(catg));;
		del_lines[i] = boff_to_linenum(nodes[node_idx]->data->data[i]) + 2;
	}

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();
	char *str;
	char linebuff[LINE_BUFFER];
	size_t line = 0;
	size_t temp_line;
	size_t temp_idx;
	int changed = 0;

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;
		temp_line = 0;
		for (size_t i = 0; i < n_recs; i++) {
			if (del_lines[i] == line) {
				temp_line = del_lines[i];
				temp_idx = i;
				break;
			}
		}
		if (temp_line == 0) {
			fputs(str, tmpfptr);
		} else {
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
			lda[temp_idx]->month, 
			lda[temp_idx]->day, 
			lda[temp_idx]->year, 
			lda[temp_idx]->category, 
			lda[temp_idx]->desc, 
			lda[temp_idx]->transtype, 
			lda[temp_idx]->amount
		   );
			changed++;
		}
	}

	free_lda(lda, n_recs);

	if (debug_flag) {
		printw("CHANGED: %d\n", changed);
		getch();
	}

	mv_tmp_to_record_file(tmpfptr, fptr);
	return changed;
}

void nc_edit_category(long node_idx, long nmembers, CategoryNode **nodes) {
	long b = nodes[node_idx]->catg_fp;
	double tmp = 0.0;
	enum fields select = select_catg_field();
	if (select < 0) {
		return;
	}
	struct BudgetTokens *bt = tokenize_budget_byte_offset(b);
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
