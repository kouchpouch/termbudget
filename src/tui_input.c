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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <ctype.h>
#include <limits.h>

#include "parser.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "main.h"
#include "helper.h"
#include "create.h"
#include "flags.h"

struct __full_date {
	int day;
	int month;
	int year;
};

struct __prev_date_exist {
	bool day_exists;
	bool month_exists;
	bool year_exists;
};

struct __fd_input_scroll {
	int tmp_x;
	int print_x;
	int date_idx;
	int incr_x;
	int day_field_len;
	int month_field_len;
	int year_field_len;
	int y;
	int day_x;
	int month_x;
	int year_x;
};

enum _input_full_date_fields {
	F_MONTH = 0,
	F_DAY,
	F_YEAR,
	F_END = 3
} fd_fields;

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

void nc_user_input(int n, WINDOW *wptr, struct UserInput *pui)
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

		case('\n'):
		case('\r'):
			temp[idx] = '\0';
			break;

		case(27):
		case(KEY_F(QUIT)):
			noecho();
			pui->flag = QUIT;
			pui->str = NULL;
			return;

		case(KEY_BACKSPACE):
		case(127):
		case('\b'):
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

void nc_input_n_digits
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
		switch(c) {

		case('y'):
		case('Y'):
			return true;	

		case('n'):
		case('N'):
			return false;

		case('q'):
		case('Q'):
		case(KEY_F(QUIT)):
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
		switch(c) {

		case('y'):
		case('Y'):
			nc_exit_window(wptr);
			return true;

		case('n'):
		case('N'):
		case(KEY_F(QUIT)):
		case('q'):
		case('Q'):
			nc_exit_window(wptr);
			return false;

		default:
			break;
		}
	}

	nc_exit_window_key(wptr);
	return false;
}

static void unhighlight(WINDOW *wptr, int y)
{
	int o = BOX_OFFSET;
	mvwchgat(wptr, y, o, getmaxx(wptr) - (o * 2), A_NORMAL, 0, NULL);
}

static void scroll_prev_field
(WINDOW *wptr, struct __fd_input_scroll *scrl)
{
	switch (scrl->date_idx) {

	case (F_MONTH): /* Scroll back to the year field */
		mvwchgat(wptr, scrl->y, scrl->year_x, scrl->year_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx = F_YEAR;
		break;

	case (F_DAY): 
		mvwchgat(wptr, scrl->y, scrl->month_x, scrl->month_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx--;
		break;

	case (F_YEAR):
		mvwchgat(wptr, scrl->y, scrl->day_x, scrl->day_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx--;
		break;

	default:
		break;
	}
	scrl->tmp_x = getcurx(wptr);
}

static void scroll_next_field
(WINDOW *wptr, struct __fd_input_scroll *scrl)
{
	switch (scrl->date_idx) {

	case (F_MONTH):
		mvwchgat(wptr, scrl->y, scrl->day_x, scrl->day_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx++;
		break;

	case (F_DAY): 
		mvwchgat(wptr, scrl->y, scrl->year_x, scrl->year_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx++;
		break;

	case (F_YEAR): /* Scroll back to the month field */
		mvwchgat(wptr, scrl->y, scrl->month_x, scrl->month_field_len, A_REVERSE, 0, NULL);
		scrl->date_idx = F_MONTH;
		break;

	default:
		break;
	}
	scrl->tmp_x = getcurx(wptr);
}

static void get_prev_date_existence(struct __prev_date_exist *prev, int d, int m, int y)
{
	if (d > 0) {
		prev->day_exists = true;
	}
	if (m  > 0) {
		prev->month_exists = true;
	}
	if (y > 0) {
		prev->year_exists = true;
	}
}

static void init_fd_input_scroll(WINDOW *wptr, struct __fd_input_scroll *s)
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

	/* Initially 4, due to the first field being the month field, which is 
	 * two characters plus 1 space of padding on either side. Year field will
	 * require a field_len of 6.                                         */
	s->day_field_len = 4;
	s->month_field_len = 4;
	s->year_field_len = 6;
	s->y = INPUT_MSG_Y_OFFSET;

	/* Absolute Positions, minus padding */
	s->month_x = getmaxx(wptr) / 2 - s->print_x - 1;
	s->day_x = getmaxx(wptr) / 2 - s->print_x + s->incr_x - 1;
	s->year_x = getmaxx(wptr) / 2 - s->print_x + (s->incr_x * 2) - 1;
}

static void print_previous_dates
(WINDOW *wptr, struct __fd_input_scroll *s, int m, int d, int y)
{
	int center_x = (getmaxx(wptr) / 2) - s->print_x;
	mvwxcprintw(wptr, s->y++, "Modifying Date");

	s->y++; /* New Line */

	/* Print zero padded */
	mvwprintw(wptr, s->y, center_x, "%02d / %02d / %04d", m, d, y);	

	/* Highlight first field (month) */
	mvwchgat(wptr, s->y, s->month_x, s->month_field_len, A_REVERSE, 0, NULL);
	s->tmp_x = getcurx(wptr);
}

/* An interactive alternative to the current way of taking user input for
 * the day, month, and year individually. This is an ncurses menu. Tab to move
 * between the options. Should be a lot easier to use. */
void nc_input_full_date
(int old_mo, int old_yr, int old_day, struct __full_date *new_date)
{
	int c = 0;
	WINDOW *wptr = create_input_subwindow();

	struct __fd_input_scroll scrl = { 0 };
	init_fd_input_scroll(wptr, &scrl);

	struct __prev_date_exist prev_date = { false };
	get_prev_date_existence(&prev_date, old_day, old_mo, old_yr); 

	print_previous_dates(wptr, &scrl, old_mo, old_day, old_yr);

	while (c != 'q' && c != KEY_F(QUIT)) {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch (c) {
			case (KEY_STAB):
			case (KEY_CTAB):
			case (KEY_BTAB):
			case (KEY_CATAB):
			case (KEY_RIGHT):
			case ('\t'):
				unhighlight(wptr, scrl.y);
				scroll_next_field(wptr, &scrl);
				break;
			case (KEY_LEFT):
				unhighlight(wptr, scrl.y);
				scroll_prev_field(wptr, &scrl);
				break;
			case (KEY_ENTER):
			case ('\n'):
			case ('\r'):
				break;
			default:
				break;
		}
	}

	nc_exit_window(wptr);
}

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

int nc_input_day(int month, int year, int old_day)
{
	nc_input_full_date(month, year, old_day, NULL);
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

// Returns NULL on quit
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
		switch(c) {
		case('j'):
		case(KEY_DOWN):
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
		case('k'):
		case(KEY_UP):
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
		case('c'):
manual_selection:
			free_categories(pc);
			nc_exit_window(wptr_parent);
			nc_exit_window(wptr);
			nc_print_input_footer(stdscr);
			return create_budget_record(year, month);
		case('\n'):
		case('\r'):
		case(KEY_ENTER):
			break;
		case('q'):
		case(KEY_F(QUIT)):
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
