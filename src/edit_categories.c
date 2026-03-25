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

#include "edit_categories.h"
#include "main.h"
#include "categories.h"
#include "parser.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "filemanagement.h"

enum fields {
	EDIT_AMNT = 0,
	EDIT_TYPE,
	ZERO_AMNT,
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
static void nc_print_category_member_warning(void) {
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

/* Moves the category found at FPI nodes[i]->catg_fp to the top of its
 * siblings with the same date range. */
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

static int nc_select_category_field_to_edit(void) {
	size_t n_options = 4;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * n_options));

	if (mp == NULL) {
		memory_allocate_fail();
	}

	mp->items = n_options;
	mp->title = "Editing Category";
	mp->strings[EDIT_AMNT] = "Edit Amount";
	mp->strings[EDIT_TYPE] = "Edit Type";
	mp->strings[ZERO_AMNT] = "Zero Out";
	mp->strings[DEL_CATG] = "Delete";

	int retval = nc_input_menu(mp);

	free(mp);
	return retval;
}

/* Allows the user to edit the category at file position b. */
void nc_edit_category(long b, long nmembers) {
	double tmp = 0.0;
	enum fields select = nc_select_category_field_to_edit();
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
			free_budget_tokens(bt);
			return;
		}
		bool conf = nc_confirm_amount(bt->amount);
		if (!conf) {
			free_budget_tokens(bt);
			return;
		}
		break;
	case EDIT_TYPE:
		bt->transtype = nc_input_category_type();
		if (bt->transtype < 0.0) {
			free_budget_tokens(bt);
			return;
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
	case DEL_CATG:
		if (nmembers > 0) {
			nc_print_category_member_warning();
			free_budget_tokens(bt);
			return;
		}
		if (!nc_confirm_input("Confirm Delete")) {
			free_budget_tokens(bt);
			return;
		}
		break;
	}

	if (select != DEL_CATG) {
		replace_category(bt, b);
	} else {
		delete_category(b);
	}

	free_budget_tokens(bt);
}
