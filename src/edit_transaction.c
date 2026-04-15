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
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include "main.h"
#include "create.h"
#include "sorter.h"
#include "edit_transaction.h"
#include "tui.h"
#include "tui_input.h"
#include "filemanagement.h"
#include "file_write.h"

static void delete_transaction(int line) {
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = delete_in_file(fptr, line);
	mv_tmp_to_record_file(tmpfptr, fptr);
}

static int nc_select_field_to_edit(WINDOW *wptr) {
	int select = 1;
	int c = 0;
	int max_x = getmaxx(wptr);
	int start_x = BOX_OFFSET;
	int nx = max_x - (BOX_OFFSET * 2);

	mvwchgat(wptr, 1, start_x, nx, A_REVERSE, REVERSE_COLOR, NULL);
	keypad(wptr, true);
	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Field to Edit");
	wrefresh(wptr);

	while (c != KEY_F(QUIT) && c != 'q') { 
		wrefresh(wptr);
		c = wgetch(wptr);

		switch(c) {

		case('j'):
		case(KEY_DOWN):
			if (select + 1 <= (INPUT_WIN_ROWS - BOX_OFFSET)) {
				mvwchgat(wptr, select, start_x, nx, A_NORMAL, 0, NULL);
				select++;
				mvwchgat(wptr, select, start_x, nx, A_REVERSE, REVERSE_COLOR, NULL);
			} else {
				mvwchgat(wptr, select, start_x, nx, A_NORMAL, 0, NULL);
				select = 1;
				mvwchgat(wptr, select, start_x, nx, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;

		case('k'):
		case(KEY_UP):
			if (select - 1 > 0) {
				mvwchgat(wptr, select, start_x, nx, A_NORMAL, 0, NULL);
				select--;
				mvwchgat(wptr, select, start_x, nx, A_REVERSE, REVERSE_COLOR, NULL);
			} else {
				mvwchgat(wptr, select, start_x, nx, A_NORMAL, 0, NULL);
				select = INPUT_WIN_ROWS - BOX_OFFSET;
				mvwchgat(wptr, select, start_x, nx, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;

		case('\n'):
		case('\r'):
			return select;

		case('q'):
		case(KEY_F(QUIT)):
			break;
		}
	}
	return 0;
}

/* Ncurses implementation to do the actual file writing */
static int nc_edit_csv_record
(int replace_line, int edit_field, struct LineData *ld)
{
	if (replace_line == 0) {
		puts("Cannot delete line 0");
		return -1;
	}

	replace_line++;

	enum EditRecordFields field = edit_field;
	char replace_str[LINE_BUFFER];
	FILE *fptr;
	FILE *tmpfptr;

	switch(field) {
	case E_DATE:
		ld->year = nc_input_year();
		if (ld->year < 0) {
			goto err_fail;
		}
		ld->month = nc_input_month();
		if (ld->month < 0) {
			goto err_fail;
		}
		ld->day = nc_input_day(ld->month, ld->year);
		if (ld->day < 0) {
			goto err_fail;
		}
		if (!nc_confirm_record(ld)) {
			goto err_fail;
		}

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		delete_transaction(replace_line);
		insert_transaction_record(sort_record_csv(ld->month, ld->day, ld->year), ld);
		return 0;

	case E_CATG:
		ld->category = nc_select_category(ld->month, ld->year);
		if (ld->category == NULL) {
			goto err_fail;
		}
		break;

	case E_DESC:
		ld->desc = nc_input_string("Enter Description");
		if (ld->desc == NULL) {
			goto err_fail;
		}
		break;

	case E_TYPE:
		ld->transtype = nc_input_transaction_type();
		if (ld->transtype < 0) {
			goto err_fail;
		}
		break;

	case E_AMNT:
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
		if (field == E_CATG) {
			free(ld->category);
			ld->category = NULL;
		}
		if (field == E_DESC) {
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

	if (field == E_CATG) {
		free(ld->category);
		ld->category = NULL;
	}
	if (field == E_DESC) {
		free(ld->desc);
		ld->desc = NULL;
	}

	return 0;

err_fail:
	return -1;
}

void nc_edit_transaction(long b) {
	struct LineData *ld = malloc(sizeof(*ld));
	if (ld == NULL) {
		mem_alloc_fail();
	}

	enum EditRecordFields field;
	WINDOW *wptr_edit = create_input_subwindow();
	FILE *fptr = open_record_csv("r+");
	fseek(fptr, b, SEEK_SET);

	unsigned int linenum = boff_to_linenum(b);
	char linebuff[LINE_BUFFER];
	char *line = fgets(linebuff, sizeof(linebuff), fptr);
	if (line == NULL) {
		exit(1);
	}

	tokenize_record(ld, &line);
	nc_print_record_vert(wptr_edit, ld, BOX_OFFSET);

	mvwprintw(wptr_edit, INPUT_WIN_ROWS - BOX_OFFSET, BOX_OFFSET, "%s", "Delete");
	box(wptr_edit, 0, 0);
	wrefresh(wptr_edit);

	field = nc_select_field_to_edit(wptr_edit);

	fclose(fptr);
	nc_exit_window(wptr_edit);

	switch(field) {

	case NO_SEL:
		break;

	case E_DATE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, E_DATE, ld);
		break;

	case E_CATG:
		free(ld->category);
		ld->category = NULL;
		nc_edit_csv_record(linenum, E_CATG, ld);
		break;

	case E_DESC:
		nc_print_input_footer(stdscr);
		free(ld->desc);
		ld->desc = NULL;
		nc_edit_csv_record(linenum, E_DESC, ld);
		break;

	case E_TYPE:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, E_TYPE, ld);
		break;

	case E_AMNT:
		nc_print_input_footer(stdscr);
		nc_edit_csv_record(linenum, E_AMNT, ld);
		break;

	case DELETE:
		nc_print_input_footer(stdscr);
		if (nc_confirm_input("Confirm Delete")) {
				delete_transaction(linenum + 1);
		}
		break;

	default:
		return;
	}

	free_tokenized_record_strings(ld);
	free(ld);
	ld = NULL;
}
