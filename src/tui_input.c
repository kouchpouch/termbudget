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
#include "tui.h"
#include "tui_input.h"
#include "main.h"
#include "helper.h"

static void init_input_window(int n, WINDOW *wptr) {
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);

	int center = max_x / 2 - n / 2;

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

	if (input[length] != '\0') {
		mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "Input is too long");
		int c = 0;
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
	init_input_window(n, wptr);
	pui->flag = 0;
	int max_y, max_x;
	getmaxyx(wptr, max_y, max_x);
	size_t buffersize = n + 1; // Plus 1 to hold null terminator
	int center = max_x / 2 - n / 2;

	char temp[buffersize];
	int c = 0;
	int idx = 0;
	int xcursor = center;
	wmove(wptr, max_y - 4, center);
	while (c != '\n' && c != '\r') {
		wrefresh(wptr);
		if (idx < n) {
			echo();
		} else {
			noecho();
		}
		c = wgetch(wptr);
		switch (c) {
		case('\n'):
		case('\r'):
			temp[idx] = '\0';
			break;
		case(KEY_F(QUIT)):
			noecho();
			pui->flag = QUIT;
			pui->str = NULL;
			return;
		case(KEY_BACKSPACE):
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

bool nc_confirm_input(char *msg) {
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, 3, msg);
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");
	wrefresh(wptr);

	int c = 0;

	while (1) {
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
		case('q'):
		case('Q'):
		case(KEY_F(QUIT)):
			return false;
		default:
			break;
		}
	}

	nc_exit_window(wptr);
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

// Returns -1 on quit
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

// Returns -1 on quit
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

int nc_input_transaction_type(void) {
	WINDOW *wptr_input = create_input_subwindow();
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Choose Expense/Income");
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET + 2, "(1)Expense (2)Income");

	/* Transaction type 0 = expense, 1 = income. */
	int t = 0; 

	while (t != '1' && t != '2') {
		t = wgetch(wptr_input);	
		if (t == KEY_EXIT || t == KEY_F(QUIT)) {
			return -1;
		}
	}

	nc_exit_window(wptr_input);

	/* Have these statements here to explictly return a 0 or 1 without
	 * having to do an integer/char conversion */
	if (t == '1') {
		return 0;
	} else if (t == '2') {
		return 1;
	}
	
	return -1;
}

double nc_input_amount(void) {
	WINDOW *wptr_input = create_input_subwindow();
	struct UserInput pui_, *pui = &pui_;
	mvwxcprintw(wptr_input, INPUT_MSG_Y_OFFSET, "Enter Amount");
	keypad(wptr_input, true);
	double amount;

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
