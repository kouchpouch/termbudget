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

#include "tui.h"
#include "tui_input_menu.h"

#include <ncurses.h>
#include <string.h>

void print_items(struct MenuParams *mp, WINDOW *wptr, int first_y)
{
	mvwxcprintw(wptr, 0, mp->title);
	for (int i = 0; i < mp->items; i++) {
		mvwxcprintw(wptr, first_y + i, mp->strings[i]);
	}
	wrefresh(wptr);
}

int first_y_pos(int items, int max_items)
{
	return (max_items / 2) - (items / 2) + 1;
}

int nc_input_menu(struct MenuParams *mp)
{
//	WINDOW *wptr = create_input_subwindow();
	WINDOW *wptr = create_input_subwindow_n_rows(mp->items);
	int c = 0;
	int selection_idx = 0;
	int displayed = mp->items;
	int cur_y = first_y_pos(mp->items, getmaxy(wptr) - BOX_OFFSET);
	int first_y = cur_y;
	/* Units on the x axis to change for mvwchgat() */
	int x_traverse = getmaxx(wptr) - BOX_OFFSET * 2;

	print_items(mp, wptr, cur_y);
	mvwchgat(wptr, cur_y, BOX_OFFSET, getmaxx(wptr) - BOX_OFFSET * 2, A_REVERSE, REVERSE_COLOR, NULL); 
	keypad(wptr, true);

	while (c != KEY_F(QUIT) && c != '\n' && c != '\r') {
		wrefresh(wptr);
		c = wgetch(wptr);
		switch (c) {

		case ('j'):
		case KEY_DOWN:
			if (selection_idx + 1 < displayed) {
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_NORMAL, 0, NULL); 
				cur_y++;
				selection_idx++;
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_REVERSE, REVERSE_COLOR, NULL); 
			} else {
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_NORMAL, 0, NULL);
				selection_idx = 0;
				cur_y = first_y;
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;

		case ('k'):
		case KEY_UP:
			if (selection_idx - 1 >= 0) {
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_NORMAL, 0, NULL); 
				cur_y--;
				selection_idx--;
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_REVERSE, REVERSE_COLOR, NULL); 
			} else {
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_NORMAL, 0, NULL);
				selection_idx = mp->items - 1;
				cur_y = first_y + mp->items - 1;
				mvwchgat(wptr, cur_y, BOX_OFFSET, x_traverse, A_REVERSE, REVERSE_COLOR, NULL);
			}
			break;

		case ('\n'):
		case ('\r'):
			nc_exit_window(wptr);
			return selection_idx;

		case ('q'):
		case KEY_F(QUIT):
			nc_exit_window(wptr);
			return -1;
		default:
			break;
		}
	}
	nc_exit_window(wptr);
	return -1;
}
