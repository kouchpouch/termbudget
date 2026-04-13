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

#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "main.h"
#include "helper.h"
#include "create.h"
#include "flags.h"

static void init_input_window(int n, WINDOW *wptr) {
	int max_y, max_x;
	int center;

	getmaxyx(wptr, max_y, max_x);
	center = max_x / 2 - n / 2;

	/* Print a line of underscores to accept the user input */
	for (int i = 0; i < n; i++) {
		mvwprintw(wptr, max_y - 4, center + i, "%c", '_');
	}

	keypad(wptr, true);
	wmove(wptr, max_y - 4, center);
	echo();
	curs_set(1);
	wrefresh(wptr);
}

static bool validate_input_len(size_t buffer, char *input, WINDOW *wptr) {
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

void nc_user_input(int n, WINDOW *wptr, struct UserInput *pui) {
	int max_y, max_x;
	size_t buffersize = n + 1; // Plus 1 to hold null terminator
	int center;
	char temp[buffersize];
	int c = 0;
	int idx = 0;
	int xcursor;

	getmaxyx(wptr, max_y, max_x);
	center = max_x / 2 - n / 2;
	xcursor = center;

	init_input_window(n, wptr);
	pui->flag = 0;
	getmaxyx(wptr, max_y, max_x);
	wmove(wptr, max_y - 4, center);
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
				wmove(wptr, max_y - 4, xcursor);
				wprintw(wptr, "%c", '_');
				wmove(wptr, max_y - 4, xcursor);
			} else {
				wmove(wptr, max_y - 4, xcursor);
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
				wmove(wptr, max_y - 4, xcursor);
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

void nc_input_n_digits(struct UserInputDigit *puid, 
					   WINDOW *wptr, size_t max_len, size_t min_len) 
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

bool nc_confirm_input_loop(WINDOW *wptr) {
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

bool nc_confirm_input(char *msg) {
	WINDOW *wptr = create_input_subwindow();
	bool retval;

	mvwxcprintw(wptr, 3, msg);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");
	wrefresh(wptr);

	retval = nc_confirm_input_loop(wptr);
	nc_exit_window(wptr);
	return retval;
}

bool nc_confirm_record(struct LineData *ld) {
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

int nc_input_month(void) {
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Month");
	wrefresh(wptr_input);

	nc_input_n_digits(puid, wptr_input, MAX_LEN_DAY_MON, 1);
	while (puid->flag != QUIT && (puid->data <= 0 || puid->data > 12)) {
		clear_input_error_message(wptr_input);
		mvwxcprintw(wptr_input, getmaxy(wptr_input) - BOX_OFFSET, "Invalid Month");
		nc_input_n_digits(puid, wptr_input, MAX_LEN_DAY_MON, 1);
	} 

	nc_exit_window(wptr_input);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

int nc_input_year(void) {
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Year");
	wrefresh(wptr_input);

	do {
		nc_input_n_digits(puid, wptr_input, MAX_LEN_YEAR, MIN_LEN_YEAR);
	} while (puid->data < 0 && puid->flag != QUIT);

	nc_exit_window(wptr_input);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

int nc_input_day(int month, int year) {
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInputDigit puid_, *puid = &puid_;

	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Day");
	wrefresh(wptr_input);

	nc_input_n_digits(puid, wptr_input, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	while (puid->flag != QUIT && 
		(puid->data == -1 || dayexists(puid->data, month, year) == false)) 
	{
		clear_input_error_message(wptr_input);
		if (dayexists(puid->data, month, year) == false) { 
			mvwxcprintw(wptr_input, getmaxy(wptr_input) - BOX_OFFSET, "Not a valid day");
			wrefresh(wptr_input);
		}
		nc_input_n_digits(puid, wptr_input, MAX_LEN_DAY_MON, MIN_LEN_DAY_MON);
	}

	nc_exit_window(wptr_input);

	if (puid->flag == QUIT) {
		return -1;
	} else {
		return puid->data;
	}
}

// Returns NULL on quit
char *nc_input_string(char *msg) {
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

int nc_input_category_type(void) {
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

int nc_input_transaction_type(void) {
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

double nc_input_amount(void) {
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

double nc_input_budget_amount(void) {
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

char *nc_select_category(int month, int year) {
	struct Categories *pc = get_budget_catg_by_date(month, year);
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
