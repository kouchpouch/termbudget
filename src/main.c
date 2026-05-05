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
#include <stdbool.h>
#include <limits.h>
#include <assert.h>
#include <ncurses.h>
#include <limits.h>

#include "main.h"
#include "cli.h"
#include "create.h"
#include "read_init.h"
#include "fileintegrity.h"
#include "filemanagement.h"
#include "tui.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "convert_csv.h"
#include "flags.h"

/* 'R'ead 'RET'urn values */
#define RRET_DEFAULT 0
#define RRET_BYDATE 1
#define RRET_QUIT 2

/*
#if (NCURSES_VERSION_MAJOR >= 6) && (NCURSES_VERSION_MINOR > 0)
#define HAS_RESET_COLOR_PAIRS
#endif
*/

/* GLOBAL FLAGS. Defined in flags.h, initialized here in main.c */
int debug_flag;
int cli_flag;
int verify_flag;

void mem_alloc_fail(void) 
{
	perror("Failed to allocate memory\n");
	exit(1);
}

/* Prints the value of each struct ColWidth memeber to the window wptr */
void debug_columns(WINDOW *wptr, struct ColWidth *cw)
{
	wmove(wptr, 5, 10);
	wprintw(wptr, "DATE: %d CATG: %d, DESC: %d, TRNS: %d, AMNT: %d\n",
	cw->date, cw->catg,	cw->desc, cw->trns, cw->amnt);
	wrefresh(wptr);
	wgetch(wptr);
}

void calculate_balance(struct balances *pb, struct vec_d *pbo)
{
	FILE *fptr = open_record_csv("r");
	pb->income = 0.0;
	pb->expense = 0.0;
	int type;
	char linebuff[LINE_BUFFER];
	char *line;

	for (size_t i = 0; i < pbo->size; i++) {
		fseek(fptr, pbo->data[i], SEEK_SET);
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) {
			break;
		}
		seek_n_fields(&line, 5);
		type = atoi(strsep(&line, ","));
		if (type == 0) {
			pb->expense += atof(strsep(&line, ","));
		} else {
			pb->income += atof(strsep(&line, ","));
		}
	}
	fclose(fptr);
}

void print_debug_node(WINDOW *wptr, struct catg_nodes *node)
{
	wprintw(wptr, "Data sz: %lu, Next: %p, Prev: %p, FPI: %lu\n", 
		 node->data->size, 
		 (void *)node->next, 
		 (void *)node->prev, 
		 node->catg_fp);
}

static int n_spaces(int max_x, char *str1, char *str2)
{
	return (int)(max_x - (BOX_OFFSET * 2) - strlen(str1) - strlen(str2));
}

int show_help_subwindow(void)
{
	int rows = 15;
	WINDOW *wptr = create_input_subwindow_n_rows(rows);
	int y = 1;
	int mx = getmaxx(wptr);
	int my = getmaxy(wptr);

	char line[mx];
	for (size_t i = 0; i < sizeof(line); i++) {
		line[i] = ' ';
	}
	line[sizeof(line) - 1] = '\0';

	char *key[] = {
		"a, A, F1",
		"e, E, F2",
		"r, R, F3",
		"q, Q, F4",
		"s, S, F5",
		"o, O, F6",
		"k, ARROW UP",
		"j, ARROW DN",
		"PAGE UP",
		"PAGE DN",
		"HOME",
		"END",
		"ENTER",
		"K, ^HOME",
		"?, h, H"
	};

	char *help[] = {
		"Add a category, record, or new budget",
		"Edit a record or category",
		"Select a new date to read data from",
		"Go back/quit",
		"Change sorted by method",
		"Show the yearly overview graphs",
		"Scroll up",
		"Scroll down",
		"Scroll up many",
		"Scroll down many",
		"Scroll to beginning",
		"Scroll to end",
		"Select date/view record details",
		"Move a category to the beginning",
		"Show this menu"
	};

	assert(sizeof(key) == sizeof(help));

	for (size_t i = 0; i < sizeof(key) / sizeof(char *); i++) {
		mvwprintw(wptr, y++, BOX_OFFSET, "%s%.*s%s\n", help[i], n_spaces(mx, help[i], key[i]), line, key[i]);
	}

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Help");
	wrefresh(wptr);
	nc_exit_window_key(wptr);
	return my;
}

int nc_main_menu(WINDOW *wptr)
{
	struct read_retvals rr_, *rret = &rr_;
	struct full_date *date;
	int mo, yr;

	enum add_sel {
		ADD_TRNS = 0,
		ADD_CATG,
		ADD_BUDG
	} add_sel;

	char *ret;
	int c = 0;

	while (c != KEY_F(QUIT) && c != 'q') {
		nc_print_welcome(wptr);
		nc_print_main_menu_footer(wptr);
		if (debug_flag) {
			nc_print_debug_flag(wptr);
		}
		wrefresh(wptr);
		c = getch();

		switch (c) {
		case ('A'):
		case ('a'):
		case KEY_F(ADD):
			wclear(wptr);
			add_sel = get_add_selection();

			switch (add_sel) {
				case ADD_TRNS:
					create_transaction_default();
					break;
				case ADD_CATG:
					ret = create_budget_record(0, 0);
					free(ret);
					break;
				case ADD_BUDG:
					date = nc_create_new_budget();
					if (date != NULL) {
						mo = date->month;
						yr = date->year;
						free(date);
						date = NULL;
						nc_read_setup(yr, mo, SORT_CATG, rret);
					}
					break;
				default:
					break;
			}
			break;

		case ('E'):
		case ('e'):
		case KEY_F(EDIT):
			wclear(wptr);
			break;

		case ('R'):
		case ('r'):
		case KEY_F(READ):
			wclear(wptr);
			nc_read_setup_default(rret);
			while (rret->flag != RRET_QUIT) {
				switch (rret->flag) {
				case RRET_DEFAULT:
					nc_read_setup_default(rret);
					break;
				case RRET_BYDATE:
					nc_read_setup(rret->yr, rret->mo, rret->sort, rret);
					break;
				default:
					nc_read_setup_default(rret);
					break;
				}
			}
			break;
		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			wclear(wptr);
			return 1;

		case KEY_RESIZE:
			wclear(wptr);
			break;
		case ('H'):
		case ('h'):
		case ('?'):
			show_help_subwindow();
			break;
		}
	}
	endwin();
	return 0;
}

static void print_usage(void)
{
	printf("\x1b[1mUsage: termbudget [OPTIONS]\x1b[0m\n\n");
	printf("OPTIONS:\n");
	printf("-c               CLI MODE\n");
	printf("-d               DEBUG\n");
	printf("-v               NO VERIFY, does not verify csv records formatting.\n");
	printf("--convert FILE   Converts FILE to termbudget compatible CSV\n");
}

int main(int argc, char **argv)
{
	if (verify_files_exist() == -1) {
		exit(1);
	}

	debug_flag = 0;
	cli_flag = 0;
	verify_flag = 1;

	char opt[LINE_BUFFER];
	int corrected = 0;
	size_t count;

	if (argc > 1) {
		strncpy(opt, argv[1], LINE_BUFFER);
		for (size_t i = 1; i < strlen(opt); i++) { // Args start at 1
			if (opt[0] == '-') {
				switch (opt[i]) {

				case ('d'):
					debug_flag = 1;
					break;

				case ('c'):
					cli_flag = 1;
					break;

				case ('v'):
					verify_flag = 0;
					break;

				case ('-'):
					if (strncmp(argv[1], "--convert", LINE_BUFFER) == 0) {
						count = convert_chase_csv(argv[i + 1]);
						printf("Converted %ld records", count);
						getc(stdin);
						exit(0);
					}
					break;

				default:
					print_usage();
					exit(0);
					break;
				}
			}
		}
	}

	if (verify_flag) {
		assert(record_len_verification());
		corrected = verify_categories_exist_in_budget();
	}

	if (debug_flag && verify_flag) {
		printf("Corrected %d records\n", corrected);
		printf("Press enter to continue");
		getc(stdin);
	}

	if (!cli_flag) {
		stdscr = nc_init_stdscr();
		int flag = 0;
		while (flag == 0) {
			flag = nc_main_menu(stdscr);
		}
	} else {
		while (1) {
			cli_main_menu();
		}
	}

/*
#ifdef HAS_RESET_COLOR_PAIRS
	reset_color_pairs();
#endif
*/

	endwin();
	return 0;
}
