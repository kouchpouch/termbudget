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
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "main.h"
#include "create.h"
#include "parser.h"
#include "read_init.h"
#include "sorter.h"
#include "edit_transaction.h"
#include "tui.h"
#include "tui_input.h"
#include "filemanagement.h"
#include "file_write.h"

struct field_select {
	int y_cursor;
	int choice;
	int max_x;
	int start_x;
	int nx;
};

static void delete_transaction(int line)
{
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = delete_in_file(fptr, line);
	mv_tmp_to_record_file(tmpfptr, fptr);
}

static void edit_field_loop_scroll_prev(struct field_select *fs, WINDOW *wptr)
{
	if (fs->y_cursor - 1 > 0) {
		mvwchgat(wptr, fs->y_cursor, fs->start_x, fs->nx, A_NORMAL, 0, NULL);
		fs->y_cursor--;
		highlight(wptr, fs->y_cursor, fs->start_x, fs->nx);
	} else {
		mvwchgat(wptr, fs->y_cursor, fs->start_x, fs->nx, A_NORMAL, 0, NULL);
		fs->y_cursor = INPUT_WIN_ROWS - BOX_OFFSET;
		highlight(wptr, fs->y_cursor, fs->start_x, fs->nx);
	}
}

static void edit_field_loop_scroll_next(struct field_select *fs, WINDOW *wptr)
{
	if (fs->y_cursor + 1 <= (INPUT_WIN_ROWS - BOX_OFFSET)) {
		mvwchgat(wptr, fs->y_cursor, fs->start_x, fs->nx, A_NORMAL, 0, NULL);
		fs->y_cursor++;
		highlight(wptr, fs->y_cursor, fs->start_x, fs->nx);
	} else {
		mvwchgat(wptr, fs->y_cursor, fs->start_x, fs->nx, A_NORMAL, 0, NULL);
		fs->y_cursor = 1;
		highlight(wptr, fs->y_cursor, fs->start_x, fs->nx);
	}
}

static int select_edit_field_loop(WINDOW *wptr)
{
	struct field_select fs;
	fs.y_cursor = 1;
	fs.choice = 0;
	fs.max_x = getmaxx(wptr);
	fs.start_x = BOX_OFFSET;
	fs.nx = fs.max_x - (BOX_OFFSET * 2);

	mvwchgat(wptr, 1, fs.start_x, fs.nx, A_REVERSE, REVERSE_COLOR, NULL);
	keypad(wptr, true);
	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Field to Edit");
	wrefresh(wptr);

	while (1) { 
		wrefresh(wptr);
		fs.choice = wgetch(wptr);

		switch (fs.choice) {

		case ('j'):
		case KEY_DOWN:
			edit_field_loop_scroll_next(&fs, wptr);
			break;

		case ('k'):
		case KEY_UP:
			edit_field_loop_scroll_prev(&fs, wptr);
			break;

		case ('\n'):
		case ('\r'):
			return fs.y_cursor;

		case ('q'):
		case KEY_F(QUIT):
			return 0;
		}
	}
	return 0;
}

/* Ncurses implementation to do the actual file writing */
static int nc_edit_csv_record(int replace_line,
							  int edit_field,
							  struct transaction_tokens *ld,
							  struct read_state *rret)
{
	if (replace_line == 0) {
		puts("Cannot delete line 0");
		return -1;
	}

	replace_line++;

	enum EditRecordFields field = edit_field;
	char replace_str[LINE_BUFFER] = { 0 };
	FILE *fptr;
	FILE *tmpfptr;
	struct full_date fd;

	switch (field) {
	case EDIT_RCRD_DATE:
		if (nc_input_full_date(ld->month, ld->day, ld->year, &fd) == 0) {
			ld->month = fd.month;
			ld->day = fd.day;
			ld->year = fd.year;
		} else {
			goto err_fail;
		}
		if (!nc_confirm_record(ld)) {
			goto err_fail;
		}

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		delete_transaction(replace_line);
		insert_transaction_record(sort_record_csv(ld->month, ld->day, ld->year), ld);
		if (rret != NULL) {
			rret->month = fd.month;
			rret->year = fd.year;
			rret->flag = RRET_BYDATE;
		}
		return EDIT_RCRD_DATE;

	case EDIT_RCRD_CATG:
		ld->category = nc_select_category(ld->month, ld->year);
		if (ld->category == NULL) {
			goto err_fail;
		}
		break;

	case EDIT_RCRD_DESC:
		ld->desc = nc_input_string("Enter Description");
		if (ld->desc == NULL) {
			goto err_fail;
		}
		break;

	case EDIT_RCRD_TYPE:
		ld->transtype = nc_input_transaction_type();
		if (ld->transtype < 0) {
			goto err_fail;
		}
		break;

	case EDIT_RCRD_AMNT:
		ld->amount = nc_input_amount();
		if (ld->amount < 0) {
			goto err_fail;
		}
		break;

	default:
		puts("Not a valid choice, exiting");
		return -1;
	}

	if (!nc_confirm_record(ld)) {
		if (field == EDIT_RCRD_CATG) {
			free(ld->category);
			ld->category = NULL;
		}
		if (field == EDIT_RCRD_DESC) {
			free(ld->desc);
			ld->desc = NULL;
		}
		goto err_fail;
	}

	fptr = open_record_csv("r");
	line_data_to_string(replace_str, sizeof(replace_str), ld);
	tmpfptr = replace_in_file(fptr, replace_str, replace_line);

	/* mv_tmp_to_record_file() closes the file pointers */
	mv_tmp_to_record_file(tmpfptr, fptr);

	if (field == EDIT_RCRD_CATG) {
		free(ld->category);
		ld->category = NULL;
	}
	if (field == EDIT_RCRD_DESC) {
		free(ld->desc);
		ld->desc = NULL;
	}

	return 0;

err_fail:
	if (rret != NULL) {
		rret->month = ld->month;
		rret->year = ld->year;
		rret->flag = RRET_BYDATE;
	}
	return -1;
}

/* Ncurses implementation for interactive transaction editing. To enter the
 * interactive mode, pass -1 or any int less than 0 to opt_action, if 
 * opt action is set to a integer within enum EditRecordFields that option
 * will be chosen automatically, bypassing the interactive choice portion. 
 * Returns a integer in enum EditRecordFields for the chosen field that was 
 * edited, -1 on quit or invalid choice. */
static int edit_transaction(long b, int opt_action, struct read_state *rret)
{
	struct transaction_tokens *ld = malloc(sizeof(*ld));
	if (ld == NULL) {
		mem_alloc_fail();
	}

	enum EditRecordFields field;
	WINDOW *wptr_edit = NULL;
	FILE *fptr = open_record_csv("r+");
	unsigned int linenum = boff_to_linenum(b);
	char linebuff[LINE_BUFFER] = { 0 };
	char *line = NULL;

	fseek(fptr, b, SEEK_SET);
	line = fgets(linebuff, sizeof(linebuff), fptr);
	if (line == NULL) {
		exit(1);
	}

	tokenize_record(ld, &line);

	if (opt_action >= 0) {
		field = opt_action;
	} else {
		wptr_edit = create_input_subwindow();
		nc_print_record_vert(wptr_edit, ld, BOX_OFFSET);
		mvwprintw(wptr_edit, INPUT_WIN_ROWS - BOX_OFFSET, BOX_OFFSET, "%s", "Delete");
		box(wptr_edit, 0, 0);
		wrefresh(wptr_edit);
		field = select_edit_field_loop(wptr_edit);
		nc_exit_window(wptr_edit);
	}

	fclose(fptr);

	switch (field) {

	case NO_RCRD_SELECT:
		break;

	case EDIT_RCRD_DATE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, EDIT_RCRD_DATE, ld, rret);
		break;

	case EDIT_RCRD_CATG:
		free(ld->category);
		ld->category = NULL;
		nc_edit_csv_record(linenum, EDIT_RCRD_CATG, ld, rret);
		break;

	case EDIT_RCRD_DESC:
		nc_print_input_footer(stdscr);
		free(ld->desc);
		ld->desc = NULL;
		nc_edit_csv_record(linenum, EDIT_RCRD_DESC, ld, rret);
		break;

	case EDIT_RCRD_TYPE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, EDIT_RCRD_TYPE, ld, rret);
		break;

	case EDIT_RCRD_AMNT:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, EDIT_RCRD_AMNT, ld, rret);
		break;

	case EDIT_RCRD_DELETE:
		nc_print_input_footer(stdscr);
		if (nc_confirm_input("Confirm Delete")) {
				delete_transaction(linenum + 1);
		}
		break;

	default:
		return -1;
	}

	free_tokenized_record_strings(ld);
	free(ld);
	ld = NULL;
	return field;
}

/* Wrapper for edit_transaction */
int nc_edit_transaction(long b, struct read_state *rret)
{
	return edit_transaction(b, -1, rret);
}

/* Wrapper for edit_transaction */
int nc_edit_transaction_opt(long b, int opt, struct read_state *rret)
{
	return edit_transaction(b, opt, rret);
}
