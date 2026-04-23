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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>

#include "parser.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "main.h"
#include "helper.h"
#include "create.h"
#include "flags.h"

#define DATE_SUBWINDOW_ROWS 9

struct date_input_vars {
	int print_x;
	int field_idx;
	int incr_x;
	int day_field_len;
	int month_field_len;
	int year_field_len;
	int date_y;
	int day_x;
	int month_x;
	int year_x;
	int cancel_x;
	int accept_x;
	int opt_y;
	int opt_len;
	int x_padding;
};

enum _fd_fields {
	F_MONTH = 0,
	F_DAY,
	F_YEAR,
	F_CANCEL,
	F_ACCEPT,
	F_END
} fd_fields;

struct UserInputDigit {
	int data;
	int flag;
};

static void unhighlight_boxed(WINDOW *wptr, int y)
{
	unhighlight(wptr, y, BOX_OFFSET, getmaxx(wptr) - (BOX_OFFSET * 2));
}

static bool date_is_valid(int day, int month, int year)
{
	if (month < 1 || month > 12) {
		return false;
	}

	if (year < 1900 || year > 2100) {
		return false;
	}

	if (!dayexists(day, month, year)) {
		return false;
	}
	return true;
}

static void init_input_window(int n, WINDOW *wptr, int input_y)
{
	int max_x = getmaxx(wptr);
	int center;

	center = max_x / 2 - n / 2;

	/* Print a line of underscores to accept the user input */
	for (int i = 0; i < n; i++) {
		mvwprintw(wptr, input_y, center + i, "%c", '_');
	}

	keypad(wptr, true);
	wmove(wptr, input_y, center);
	echo();
	curs_set(1);
	wrefresh(wptr);
}

static bool validate_input_len(size_t buffer, char *input, WINDOW *wptr)
{
	size_t length = strnlen(input, buffer);
	int c = 0;

	if (input[length] != '\0') {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Input is too long");
		while (c != '\n') {
			c = getchar();
		}
		return false;
	}

	if (length < 1) {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Input is too short");
		return false;
	}

	if (strstr(input, ",")) {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "No commas allowed");
		return false;
	}

	return true;
}

static int get_edit_field_x_coord
(WINDOW *wptr, struct date_input_vars *s)
{
	fd_fields = s->field_idx;
	switch (fd_fields) {

	case F_MONTH:
		wmove(wptr, s->date_y, s->month_x + 1);
		break;

	case F_DAY:
		wmove(wptr, s->date_y, s->day_x + 1);
		break;

	case F_YEAR:
		wmove(wptr, s->date_y, s->year_x + 1);
		break;

	case F_CANCEL:
	case F_ACCEPT:
	case F_END:
		return 0;
	};

	return getcurx(wptr) + (s->x_padding / 2);
}

static void clear_field(WINDOW *wptr, int edit_field_x, int n, int y)
{
	int tmp_x = edit_field_x;
	for (int i = 0; i < n; i++) {
		mvwprintw(wptr, y, edit_field_x + i, "%c", '_'); 
	}
	wmove(wptr, y, tmp_x);
}

static bool char_is_valid(char c) {
	/* ASCII number range */
	if (c >= 48 && c <= 57) {
		return true;
	}
	return false;
}

static void modify_date_vals 
(int val, struct full_date *d, int field)
{
	switch (field) {

	case F_MONTH:
		d->month = val;
		break;

	case F_DAY:
		d->day = val;
		break;

	case F_YEAR:
		d->year = val;
		break;
	}
}

static void rehighlight_date_field(WINDOW *wptr, struct date_input_vars *s)
{
	int x;
	int n;
	switch (s->field_idx) {

	case F_MONTH:
		x = s->month_x;
		n = s->month_field_len;
		break;

	case F_DAY:
		x = s->day_x;
		n = s->day_field_len;
		break;

	case F_YEAR:
		x = s->year_x;
		n = s->year_field_len;
		break;
	}
	highlight(wptr, s->date_y, x, n);
}

/* Determines the field from s->field_idx, and prints characters from stdin 
 * to the screen, returns the value entered if it passes verification, 0
 * on quit, -1 on failure. */
static int date_field_input_loop
(WINDOW *wptr, struct date_input_vars *s, struct full_date *date)
{
	int n;
	s->field_idx == F_YEAR ? (n = s->year_field_len) : (n = s->month_field_len);
	n -= s->x_padding;

	size_t buffersize = n + 1; // Plus 1 to hold null terminator
	char temp[buffersize];
	int c = 0;
	int idx = 0;
	int modified_field_val;
	int x_cursor;
	int edit_field_x = get_edit_field_x_coord(wptr, s);

	x_cursor = getcurx(wptr);

	clear_field(wptr, edit_field_x - (s->x_padding / 2), n, s->date_y);

	while (c != '\n' && c != '\r' && c != '\t') {
		wrefresh(wptr);
		noecho();
		c = wgetch(wptr);
		switch (c) {

		case ('\t'):
		case ('\n'):
		case ('\r'):
			temp[idx] = '\0';
			break;

		case (27): /* ESC */
		case KEY_F(QUIT):
			noecho();
			return 0;

		case KEY_BACKSPACE:
		case (127): /* DELETE */
		case ('\b'):
			if (x_cursor >= edit_field_x) {
				idx--;
				x_cursor--;
				temp[idx] = '\0';
				wmove(wptr, s->date_y, x_cursor);
				wprintw(wptr, "%c", '_');
				wmove(wptr, s->date_y, x_cursor);
			} else {
				wmove(wptr, s->date_y, x_cursor);
			}
			break;

		default:
			if (idx < n) { 
				if (char_is_valid(c)) {
					wprintw(wptr, "%c", c);
					temp[idx] = c;
					idx++;
					x_cursor++;
				}
			} else {
				erasechar();
				wmove(wptr, s->date_y, x_cursor);
			}
			break;
		}
	}

	noecho();
	curs_set(0);

	modified_field_val = atoi(temp);
	modify_date_vals(modified_field_val, date, s->field_idx);

	return 0;
}

static void scroll_prev_field
(WINDOW *wptr, struct date_input_vars *s)
{
	unhighlight_boxed(wptr, s->date_y);
	unhighlight_boxed(wptr, s->opt_y);
	switch (s->field_idx) {

	case F_MONTH:
		highlight(wptr, s->opt_y, s->accept_x, s->opt_len);
		s->field_idx = F_ACCEPT;
		break;

	case F_DAY: 
		highlight(wptr, s->date_y, s->month_x, s->month_field_len); 
		s->field_idx--;
		break;

	case F_YEAR:
		highlight(wptr, s->date_y, s->day_x, s->day_field_len); 
		s->field_idx--;
		break;

	case F_CANCEL:
		highlight(wptr, s->date_y, s->year_x, s->year_field_len);
		s->field_idx--;
		break;

	case F_ACCEPT:
		highlight(wptr, s->opt_y, s->cancel_x, s->opt_len);
		s->field_idx--;
		break;

	default:
		break;
	}
}

static void scroll_next_field
(WINDOW *wptr, struct date_input_vars *s)
{
	unhighlight_boxed(wptr, s->date_y);
	unhighlight_boxed(wptr, s->opt_y);
	switch (s->field_idx) {

	case F_MONTH:
		highlight(wptr, s->date_y, s->day_x, s->day_field_len);
		s->field_idx++;
		break;

	case F_DAY: 
		highlight(wptr, s->date_y, s->year_x, s->year_field_len);
		s->field_idx++;
		break;

	case F_YEAR: /* Move to the options */
		highlight(wptr, s->opt_y, s->cancel_x, s->opt_len);
		s->field_idx++;
		break;

	case F_CANCEL:
		highlight(wptr, s->opt_y, s->accept_x, s->opt_len);
		s->field_idx++;
		break;

	case F_ACCEPT:
		highlight(wptr, s->date_y, s->month_x, s->month_field_len); 
		s->field_idx = F_MONTH;
		break;

	default:
		break;
	}
}

static void init_fd_input_scroll(WINDOW *wptr, struct date_input_vars *s)
{
	/* Relative to the center of the window, print this many spaces to the 
	 * left of center.
	 * "XX / XX / XXXX" = 14 chars.. start printing on 7.                */
	s->print_x = 7;

	/* How many spaces to increment X by. Shifting the highlighted portion
	 * from month to date to year.
	 *  XX / XX / XXXX
	 *  ^....^
	 *    5	                                                             */
	s->incr_x = 5;

	s->x_padding = 2;
	/* Initially 4, due to the first field being the month field, which is 
	 * two characters plus 1 space of padding on either side. Year field will
	 * require a field_len of 6.                                         */
	s->day_field_len = 2 + s->x_padding;
	s->month_field_len = 2 + s->x_padding;
	s->year_field_len = 4 + s->x_padding;
	s->date_y = INPUT_MSG_Y_OFFSET;

	/* Absolute Positions, with padding */
	s->month_x = getmaxx(wptr) / 2 - s->print_x - (s->x_padding / 2);
	s->day_x = s->month_x + s->incr_x;
	s->year_x = s->day_x + s->incr_x;
}

static void print_data_to_window 
(WINDOW *wptr, struct date_input_vars *s, int m, int d, int y)
{
	int center_x = (getmaxx(wptr) / 2) - s->print_x;
	char *cancel = "<Cancel>";
	char *space = "      ";
	char *accept = "<Accept>";
	assert(strlen(cancel) == strlen(accept));
	s->opt_len = (int)strlen(cancel);

	size_t opt_len = strlen(cancel) + strlen(space) + strlen(accept);
	int center_opt = getmaxx(wptr) / 2 - opt_len / 2;
	mvwxcprintw(wptr, s->date_y++, "Modifying Date");

	s->date_y++; /* New Line */

	/* Print zero padded */
	mvwprintw(wptr, s->date_y, center_x, "%02d / %02d / %04d", m, d, y);	

	/* Highlight first field (month) */
	mvwchgat(wptr, s->date_y, s->month_x, s->month_field_len, A_REVERSE, 0, NULL);

	s->opt_y = s->date_y + 2;

	/* Print options */
	mvwprintw(wptr, s->opt_y, center_opt, "%s%s%s", cancel, space, accept);

	s->cancel_x = center_opt;
	s->accept_x = s->cancel_x + strlen(cancel) + strlen(space);
}

static void clear_invalid_date_msg(WINDOW *wptr)
{
	int y = getmaxy(wptr) - BOX_OFFSET;
	mvwprintw(wptr, y, BOX_OFFSET, "%*.s", getmaxx(wptr) - BOX_OFFSET * 2, " ");
}

static void print_invalid_date_msg(WINDOW *wptr)
{
	char *msg = "Date is not valid";
	int y = getmaxy(wptr) - BOX_OFFSET;
	mvwprintw(wptr, y, getmaxx(wptr) / 2 - strlen(msg) / 2, "%s", msg);
	/* Color red */
	mvwchgat(wptr, y, BOX_OFFSET, getmaxx(wptr) - BOX_OFFSET * 2, A_NORMAL, 1, NULL);
}

/* Displays an interactive menu for full date input (MONTH/DAY/YEAR), includes
 * date validation. Fills "new_date" struct with the old + any modified date
 * values.
 * Returns -1 on quit, 0 on success */
int nc_input_full_date
(int old_mo, int old_day, int old_yr, struct full_date *new_date)
{
	int c = 0;
	bool is_valid = false;
	WINDOW *wptr = create_input_subwindow_force_rows(DATE_SUBWINDOW_ROWS);

	struct date_input_vars scrl = { 0 };
	init_fd_input_scroll(wptr, &scrl);

	/* Set the new date struct to the values of the old dates, each field
	 * will be edited interactively. */
	new_date->day = old_day;
	new_date->month = old_mo;
	new_date->year = old_yr;

	print_data_to_window(wptr, &scrl, old_mo, old_day, old_yr);

	while (c != 'q' && c != KEY_F(QUIT)) {
		wrefresh(wptr);
		c = wgetch(wptr);
		if (!is_valid) {
			clear_invalid_date_msg(wptr);
		}
		switch (c) {

		case KEY_RIGHT:
		case ('l'):
		case ('\t'):
			scroll_next_field(wptr, &scrl);
			break;
			
		case KEY_LEFT:
		case ('h'):
		case KEY_BTAB: /* SHIFT + TAB */
			scroll_prev_field(wptr, &scrl);
			break;

		case KEY_ENTER:
		case ('\n'):
		case ('\r'):
			unhighlight_boxed(wptr, scrl.date_y);
			if (scrl.field_idx == F_MONTH || 
				scrl.field_idx == F_DAY ||
				scrl.field_idx == F_YEAR) {
				date_field_input_loop(wptr, &scrl, new_date);
				rehighlight_date_field(wptr, &scrl);
			} else {
				if (scrl.field_idx == F_CANCEL) {
					nc_exit_window(wptr);
					return -1;
				} else if (scrl.field_idx == F_ACCEPT) {
					if (!date_is_valid(new_date->day, new_date->month, new_date->year)) {
						print_invalid_date_msg(wptr);
						is_valid = false;
					} else {
						is_valid = true;
					}
					if (is_valid) {
						goto quit;	
					}
				}
			}
			break;

		case KEY_F(QUIT):
		case ('q'):
			nc_exit_window(wptr);
			return -1;
		}
	}

quit:
	nc_exit_window(wptr);
	return 0;
}

static void nc_user_input(int n, WINDOW *wptr, struct UserInput *pui)
{
	int max_y, max_x;
	size_t buffersize = n + 1; // Plus 1 to hold null terminator
	int center;
	char temp[buffersize];
	int c = 0;
	int idx = 0;
	int xcursor;
	int input_y;

	getmaxyx(wptr, max_y, max_x);
	center = max_x / 2 - n / 2;
	xcursor = center;
	input_y = max_y - 3;

	init_input_window(n, wptr, input_y);
	pui->flag = 0;
	getmaxyx(wptr, max_y, max_x);
	wmove(wptr, input_y, center);
	while (c != '\n' && c != '\r') {
		wrefresh(wptr);
		noecho();
		c = wgetch(wptr);
		switch (c) {

		case ('\n'):
		case ('\r'):
			temp[idx] = '\0';
			break;

		case (27):
		case KEY_F(QUIT):
			noecho();
			pui->flag = QUIT;
			pui->str = NULL;
			return;

		case KEY_BACKSPACE:
		case (127):
		case ('\b'):
			if (xcursor > center) {
				idx--;
				xcursor--;
				temp[idx] = '\0';
				wmove(wptr, input_y, xcursor);
				wprintw(wptr, "%c", '_');
				wmove(wptr, input_y, xcursor);
			} else {
				wmove(wptr, input_y, xcursor);
			}
			break;

		default:
			if (idx < n) { 
				wprintw(wptr, "%c", c);
				temp[idx] = c;
				idx++;
				xcursor++;
			} else {
				erasechar();
				wmove(wptr, input_y, xcursor);
			}
			break;
		}
	}

	pui->str = malloc(buffersize);
	if (pui->str == NULL) {
		pui->flag = -1;
		return;
	}

	strncpy(pui->str, temp, buffersize);

	noecho();
	curs_set(0);
	if (!validate_input_len(buffersize, pui->str, wptr)) {
		pui->flag = -1;
		free(pui->str);
		pui->str = NULL;
		return;
	}
}

static void nc_input_n_digits
(struct UserInputDigit *puid, WINDOW *wptr, size_t max_len, size_t min_len) 
{
	struct UserInput pui_, *pui = &pui_;
	puid->flag = 0;
	nc_user_input(max_len, wptr, pui);
	while (pui->str == NULL && pui->flag != QUIT) {
		nc_user_input(max_len, wptr, pui);
	}

	if (pui->flag == QUIT) {
		puid->flag = QUIT;
		puid->data = -1;
		return;
	}

	if (strlen(pui->str) < min_len) {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Input is too short");
		wrefresh(wptr);
		free(pui->str);
		puid->data = -1;
		return;
	}

	for (size_t i = 0; i < strlen(pui->str); i++) {
		if (!isdigit(*(pui->str + i))
			&& (*(pui->str + i) != '\n' 
			|| *(pui->str + i) != '\0')) 
		{
			mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, 
			   "Invalid character, must be digits");
			wrefresh(wptr);
			puid->data = -1;
			return;
		}
	}

	wrefresh(wptr);

	puid->data = atoi(pui->str);
	free(pui->str);
	pui->str = NULL;
}

bool nc_confirm_input_loop(WINDOW *wptr)
{
	int c = 0;
	while (1) {
		c = wgetch(wptr);
		switch (c) {

		case ('y'):
		case ('Y'):
			return true;	

		case ('n'):
		case ('N'):
			return false;

		case ('q'):
		case ('Q'):
		case KEY_F(QUIT):
			return false;

		default:
			break;
		}
	}
}

bool nc_confirm_input(char *msg)
{
	WINDOW *wptr = create_input_subwindow();
	bool retval;

	mvwxcprintw(wptr, 3, msg);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");
	wrefresh(wptr);

	retval = nc_confirm_input_loop(wptr);
	nc_exit_window(wptr);
	return retval;
}

bool nc_confirm_record(_transact_tokens_t *ld)
{
	WINDOW *wptr = create_input_subwindow();
	int c = 0;

	mvwxcprintw(wptr, 0, "Confirm Record");
	nc_print_record_vert(wptr, ld, BOX_OFFSET);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");
	wrefresh(wptr);

	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		switch (c) {

		case ('y'):
		case ('Y'):
			nc_exit_window(wptr);
			return true;

		case KEY_F(QUIT):
		case ('n'):
		case ('N'):
		case ('q'):
		case ('Q'):
			nc_exit_window(wptr);
			return false;

		default:
			break;
		}
	}

	nc_exit_window_key(wptr);
	return false;
}

/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_month(int old_month, int old_year)
{
	WINDOW *wptr = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;
	int y = INPUT_MSG_Y_OFFSET;
	char *txt = "Editing Date:";
	int x_center = getmaxx(wptr) / 2 - 
		(intlen(old_month) + intlen(old_year) + strlen(txt) / 2);

	mvwxcprintw(wptr, y++, "Enter Month");
	if (old_month > 0 && old_year > 0) {
		mvwprintw(wptr, y, x_center, "Month: %d, Year: %d", old_month, old_year);
	} else if (old_month > 0) {
		mvwprintw(wptr, y, x_center, "Month: %d", old_month); 
	}

	wrefresh(wptr);
	nc_input_n_digits(puid, wptr, MAX_LEN_DAY_MON, 1);
	while (puid->flag != QUIT && (puid->data <= 0 || puid->data > 12)) {
		clear_input_error_message(wptr);
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Invalid Month");
		nc_input_n_digits(puid, wptr, MAX_LEN_DAY_MON, 1);
	} 

	nc_exit_window(wptr);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_year(int old_year)
{
	WINDOW *wptr = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;
	int y = INPUT_MSG_Y_OFFSET;
	char *txt = "Editing Year:";
	int x_center;

	mvwxcprintw(wptr, y++, "Enter Year");
	if (old_year > 0) {
		x_center = getmaxx(wptr) / 2 - (intlen(old_year) + strlen(txt)) / 2;
		mvwprintw(wptr, y++, x_center, "%s %d", txt, old_year);
	}
	wrefresh(wptr);

	do {
		nc_input_n_digits(puid, wptr, MAX_LEN_YEAR, MIN_LEN_YEAR);
	} while (puid->data < 0 && puid->flag != QUIT);

	nc_exit_window(wptr);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

/* Creates and destroys an ncurses input window and validates the input with
 * retries. Returns -1 on quit */
int nc_input_day(int month, int year, int old_day)
{
	WINDOW *wptr = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;
	int y = INPUT_MSG_Y_OFFSET;
	char *txt = "Editing Date:";
	int x_center = getmaxx(wptr) / 2 - 
		(intlen(month) + intlen(year) + intlen(old_day) + strlen(txt) + strlen("//")) / 2;

	mvwxcprintw(wptr, y++, "Enter Day");
	if (old_day > 0) {
		mvwprintw(wptr, y, x_center, "%s %d/%d/%d", txt, month, old_day, year);
	}
	wrefresh(wptr);

	nc_input_n_digits(puid, wptr, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	while (puid->flag != QUIT && 
		(puid->data == -1 || dayexists(puid->data, month, year) == false)) 
	{
		clear_input_error_message(wptr);
		if (dayexists(puid->data, month, year) == false) { 
			mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Not a valid day");
			wrefresh(wptr);
		}
		nc_input_n_digits(puid, wptr, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	}

	nc_exit_window(wptr);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

/* Returns NULL on quit */
char *nc_input_string(char *msg)
{
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInput pui_, *pui = &pui_;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, msg);
	do {
		nc_user_input(STDIN_LARGE_BUFF, wptr_input, pui);
	} while (pui->str == NULL && pui->flag != QUIT);
	wrefresh(wptr_input);
	nc_exit_window(wptr_input);
	return pui->str;
}

int nc_input_category_type(void)
{
	int retval;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * 2));
	if (mp == NULL) {
		mem_alloc_fail();
	}

	mp->title = "This Category is to track:";
	mp->items = 2;
	mp->strings[0] = "Expenses";
	mp->strings[1] = "Income";

	/* Transaction type 0 = expense, 1 = income. */
	retval = nc_input_menu(mp);

	free(mp);

	return retval;
}

int nc_input_transaction_type(void)
{
	int retval;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * 2));
	if (mp == NULL) {
		mem_alloc_fail();
	}

	mp->title = "Select Transaction Type";
	mp->items = 2;
	mp->strings[0] = "Expense";
	mp->strings[1] = "Income";

	/* Transaction type 0 = expense, 1 = income. */
	retval = nc_input_menu(mp);

	free(mp);

	return retval;
}

double nc_input_amount(void)
{
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInput pui_, *pui = &pui_;
	double amount;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Amount");
	keypad(wptr_input, true);

	nc_user_input(MAX_LEN_AMOUNT, wptr_input, pui);
	while (pui->str == NULL && pui->flag != QUIT) {
		nc_user_input(MAX_LEN_AMOUNT, wptr_input, pui);
	}

	nc_exit_window(wptr_input);

	if (pui->str != NULL) {
		amount = atof(pui->str);
		free(pui->str);
		return amount;
	} else {
		return -1;
	}
}

double nc_input_budget_amount(void)
{
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInput pui_, *pui = &pui_;
	double amount;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Planned Amount for this Category");
	keypad(wptr_input, true);

	nc_user_input(MAX_LEN_AMOUNT, wptr_input, pui);
	while (pui->str == NULL && pui->flag != QUIT) {
		nc_user_input(MAX_LEN_AMOUNT, wptr_input, pui);
	}

	nc_exit_window(wptr_input);

	if (pui->str != NULL) {
		amount = atof(pui->str);
		free(pui->str);
		return amount;
	} else {
		return -1;
	}
}

static void draw_scroll_indicator(WINDOW *wptr)
{
	char *etc = "...";
	int y = getmaxy(wptr) - 1;
	int x = getmaxx(wptr) - (strlen(etc) + BOX_OFFSET);
	mvwprintw(wptr, y, x, "%s", etc);
	wrefresh(wptr);
}

char *nc_select_category(int month, int year)
{
	_category_vec_t *pc = get_budget_catg_by_date(month, year);
	WINDOW *wptr_parent = create_category_select_parent(pc->size);
	WINDOW *wptr = create_category_select_subwindow(wptr_parent);
	char *tmp;
	int displayed = 0;
	int cur = 0; // Y=0 is the box and title, datalines start at 1.
	int selection_idx = 0;
	int c = 0;
	int max_y = getmaxy(wptr);
	int sz;

	if (debug_flag) {
		curs_set(1);
	}

	if (pc->size == 0) {
		goto manual_selection;
	}

	if (pc->size > INT_MAX) {
		return NULL;
	} else {
		sz = (int)pc->size;
	}

	/* Print intital data based on window size */
	for (int i = 0; i < getmaxy(wptr) && i < sz; i++) {
		mvwxcprintw(wptr, i, pc->categories[i]);
		displayed++;
	}

	if (sz > displayed) {
		draw_scroll_indicator(wptr_parent);
	}

	wrefresh(wptr);
	mvwchgat(wptr, 0, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
	keypad(wptr, true);

	while (c != '\n' && c != '\r') {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch (c) {
		case ('j'):
		case KEY_DOWN:
			if (selection_idx + 1 < sz) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur++;
				selection_idx++;

				if (displayed < sz && cur == max_y) {
					wmove(wptr, 0, 0);
					wdeleteln(wptr);
					mvwxcprintw(wptr, max_y - 1, pc->categories[selection_idx]);
					cur = max_y - 1;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case ('k'):
		case KEY_UP:
			if (selection_idx - 1 >= 0) {
				mvwchgat(wptr, cur, 0, -1, A_NORMAL, 0, NULL);
				cur--;
				selection_idx--;

				if (selection_idx >= 0 && displayed < sz && cur == -1) {
					wmove(wptr, 0, 0);
					winsertln(wptr);
					mvwxcprintw(wptr, 0, pc->categories[selection_idx]);
					cur = 0;
				}

				mvwchgat(wptr, cur, 0, -1, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;
		case ('c'):
manual_selection:
			free_categories(pc);
			nc_exit_window(wptr_parent);
			nc_exit_window(wptr);
			nc_print_input_footer(stdscr);
			return create_budget_record(year, month);
		case ('\n'):
		case ('\r'):
		case KEY_ENTER:
			break;
		case ('q'):
		case KEY_F(QUIT):
			goto cleanup;
		}
	}

	if (selection_idx >= 0) {
		tmp = strdup(pc->categories[selection_idx]); // Must be free'd
		free_categories(pc);
		nc_exit_window(wptr_parent);
		nc_exit_window(wptr);
		return tmp; // Will return NULL if stdup failed, callee checks
	}

cleanup:
	free_categories(pc);
	nc_exit_window(wptr_parent);
	nc_exit_window(wptr);
	return NULL;
}
