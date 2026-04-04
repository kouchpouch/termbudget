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
#include <stdbool.h>
#include <ncurses.h>

#include "read.h"
#include "main.h"
#include "edit_categories.h"
#include "get_date.h"
#include "filemanagement.h"
#include "helper.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "tui_sidebar.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "flags.h"

enum SortBy {
	SORT_DATE = 0,
	SORT_CATG = 1
} sortby;

const char *abbr_months[] = {
	"JAN", 
	"FEB", 
	"MAR", 
	"APR",
	"MAY",
	"JUN",
	"JUL",
	"AUG",
	"SEP",
	"OCT",
	"NOV",
	"DEC"
};

static bool duplicate_vector_data(Vec *vec, long y) {
	for (size_t i = 0; i < vec->size; i++) {
		if (vec->data[i] == y) {
			return true;
		}
	}
	return false;
}

static void combine_dedup_vectors(Vec *vec1, Vec *vec2, Vec *result) {
	size_t tmp1;
	size_t tmp2;
	size_t max = max_val(vec1->size, vec2->size);

	for (size_t i = 0; i < max; i++) {
		if (i < vec2->size) {
			tmp1 = vec2->data[i];
		} else {
			tmp1 = 0;
		}

		if (i < vec1->size) {
			tmp2 = vec1->data[i];
		} else {
			tmp2 = 0;
		}

		if (tmp1 > 0 && tmp2 > 0) {
			if (tmp1 == tmp2) {
				result->data[result->size] = tmp1;
				result->size++;
			} else {
				if (!duplicate_vector_data(result, tmp1)) {
					result->data[result->size] = tmp1;
					result->size++;
				}
				if (!duplicate_vector_data(result, tmp2)) {
					result->data[result->size] = tmp2;
					result->size++;
				}
			}

		} else if (tmp1 > 0 && tmp2 == 0) {
			if (!duplicate_vector_data(result, tmp1)) {
				result->data[result->size] = tmp1;
				result->size++;
			}
		} else if (tmp2 > 0 && tmp1 == 0) {
			if (!duplicate_vector_data(result, tmp2)) {
				result->data[result->size] = tmp2;
				result->size++;
			}
		}
	}
}

/* Returns a Vec of deduplicated data from record_years and budget_years */
static Vec *consolidate_year_vectors(Vec *vec1, Vec *vec2) {
	Vec *result = malloc(sizeof(*result) + (sizeof(long) * vec1->size) +
				  	 sizeof(long) * vec2->size);
	if (result == NULL) {
		free(vec1);
		free(vec2);
		memory_allocate_fail();
	}

	result->size = 0;
	result->capacity = vec1->size + vec2->size;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

static Vec *init_nc_read_select_year(void) {
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	Vec *pr = get_years_with_data(rfptr, 2);
	Vec *pb = get_years_with_data(bfptr, 1);

	if (pr == NULL && pb != NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return pb;
	} else if (pb == NULL && pr != NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return pr;
	} else if (pr == NULL && pb == NULL) {
		fclose(rfptr);
		fclose(bfptr);
		return NULL;
	}

	fclose(rfptr);
	fclose(bfptr);

	Vec *retval = consolidate_year_vectors(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

static Vec *consolidate_month_vectors(Vec *vec1, Vec *vec2) {
	Vec *result = malloc(sizeof(*result) + (sizeof(long) * MONTHS_IN_YEAR));
	if (result == NULL) {
		free(vec1);
		free(vec2);
		memory_allocate_fail();
	}

	result->size = 0;
	result->capacity = MONTHS_IN_YEAR;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

static Vec *init_nc_read_select_month(int year) {
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	Vec *pr = get_months_with_data(rfptr, year, 1);
	Vec *pb = get_months_with_data(bfptr, year, 0);

	fclose(rfptr);
	fclose(bfptr);

	Vec *retval = consolidate_month_vectors(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

static int get_current_mo_idx(Vec *months) {
	int mo = get_current_month();
	// months->size can never be more than MONTHS_IN_YEAR so this cast is
	for (int i = 0; i < (int)months->size; i++) {
		if (months->data[i] - 1 == mo) {
			return i;
		}
	}
	return -1;
}

/* On a non-select, the return value is the inverted menukeys value */
static int nc_read_select_month(WINDOW *wptr, int year) {
	Vec *months_data = init_nc_read_select_month(year);

	int selected_month = 0;
	int monlen = strlen(abbr_months[0]);

	int temp_y = 0;
	int scr_idx = 0;
	int cur_idx = 0;
	int current_mo_idx = get_current_mo_idx(months_data);

	wmove(wptr, BOX_OFFSET, BOX_OFFSET);
	for (size_t i = 0; i < months_data->size; i++) {
		temp_y = getcury(wptr);
		if (months_data->data[i] != 0) {
			wmove(wptr, temp_y, BOX_OFFSET);
			wprintw(wptr, "%s\n", abbr_months[months_data->data[i] - 1]);
			scr_idx++;
		}
	}

	if (year == get_current_year() && current_mo_idx != -1) {
		wmove(wptr, BOX_OFFSET + current_mo_idx, BOX_OFFSET);
		cur_idx = current_mo_idx;
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	} else {
		wmove(wptr, BOX_OFFSET, BOX_OFFSET);
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	}

	box(wptr, 0, 0);
	wrefresh(wptr);

	int c = 0;
	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		temp_y = getcury(wptr);
		switch(c) {
		case('j'):
		case(KEY_DOWN):
			if (temp_y - BOX_OFFSET + 1 < scr_idx) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y + 1, BOX_OFFSET, monlen, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				cur_idx++;
			}
			break;
		case('k'):
		case(KEY_UP):
			if (temp_y - 1 >= BOX_OFFSET) {
				mvwchgat(wptr, temp_y, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
				mvwchgat(wptr, temp_y - 1, BOX_OFFSET, monlen, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				cur_idx--;
			}
			break;
		case('a'):
		case(KEY_F(ADD)):
			free(months_data);
			months_data = NULL;
			return -(ADD);
		case(KEY_RESIZE):
			free(months_data);
			months_data = NULL;

			/* This RESIZE macro has to be greater than 12, which I have no
			 * intention of changing. I hate this. However--I'm lazy. Plus,
			 * all of the resizes in this program suck. */
			return -(RESIZE);
		case('\n'):
		case('\r'):
			selected_month = months_data->data[cur_idx];
			free(months_data);
			months_data = NULL;
			return selected_month;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			free(months_data);
			months_data = NULL;
			return -(QUIT);
		default: 
			break;
		}
	}

	free(months_data);
	months_data = NULL;

	return selected_month;
}

/* On a non-select, the return value is the inverted menukeys value */
static int nc_read_select_year(WINDOW *wptr) {
	keypad(wptr, true);	

	Vec *years = init_nc_read_select_year();
	if (years == NULL) {
		return -(NO_RCRD);
	}

	int selected_year = 0;
	int print_y = 1;
	int print_x = 2;
	size_t year_index = 0;
	size_t scr_idx = 0;

	box(wptr, 0, 0);
	wrefresh(wptr);
	wmove(wptr, print_y, print_x);

	for (size_t i = 0; i < years->size; i++) {
		if (years->data[i] == get_current_year()) {
			year_index = i;
		}
		wprintw(wptr, "%ld ", years->data[i]);
		wrefresh(wptr);
	}

	/* Initially highlight the current year and move cursor to it */
	int init_rv_x = print_x;
	if (year_index > 0) {
		init_rv_x += (4 * year_index) + year_index;
		scr_idx = year_index;
	}
	
	mvwchgat(wptr, print_y, init_rv_x, 4, A_REVERSE, REVERSE_COLOR, NULL);
	wrefresh(wptr);

	int c = 0;
	int temp_x;
	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		temp_x = getcurx(wptr);
		switch(c) {
		case('h'):
		case(KEY_LEFT):
			if (scr_idx > 0) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x - 5, 4, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				scr_idx--;
			}
			break;
		case('l'):
		case(KEY_RIGHT):
			if (scr_idx + 1 < years->size) {
				mvwchgat(wptr, print_y, temp_x, 4, A_NORMAL, 0, NULL);
				mvwchgat(wptr, print_y, temp_x + 5, 4, A_REVERSE, REVERSE_COLOR, NULL);
				wrefresh(wptr);
				scr_idx++;
			}
			break;
		case('a'):
		case(KEY_F(ADD)):
			free(years);
			years = NULL;
			return -(ADD);
		case(KEY_RESIZE):
			free(years);
			years = NULL;
			return -(RESIZE);
		case('\n'):
		case('\r'):
			selected_year = years->data[scr_idx];
			free(years);
			years = NULL;
			return selected_year;
		case('Q'):
		case('q'):
		case(KEY_F(QUIT)):
			free(years);
			return -(QUIT);
		default: 
			break;
		}
	}

	free(years);
	years = NULL;

	return selected_year;
}

static int nc_read_setup_input_year(WINDOW *wptr) {
	int yr = nc_read_select_year(wptr);
	if (yr == -(NO_RCRD)) {
		mvwxcprintw(wptr, getmaxy(wptr) / 2, 
			  "No records exist, add (F1) to get started");
		wgetch(wptr);
		return -(NO_RCRD);
	} else if (yr < 0) {
		return yr;
	};
	return yr;
}

static void get_dates(struct SelRecord *sr, struct Datevals *dates) {
	WINDOW *wptr = newwin(LINES - 1, 0, 0, 0);
	box(wptr, 0, 0);

	if (!month_or_year_exists(dates->month, dates->year)) {
		dates->month = 0;
		dates->year = 0;
	}

	if (!dates->year) {
		dates->year = nc_read_setup_input_year(wptr);
		if (dates->year < 0) {
			dates->month = -1;
			sr->flag = -(dates->year);
			nc_exit_window(wptr);
			return;
		}
	}

	if (!dates->month) {
		dates->month = nc_read_select_month(wptr, dates->year);
		if (dates->month < 0) {
			sr->flag = -(dates->month);
			nc_exit_window(wptr);
			return;
		}
	}
	
	nc_exit_window(wptr);
}

static void debug_fields(void) {
	move(0,0);
	FILE *bfptr = open_budget_csv("r");
	struct BudgetHeader *bh = parse_budget_header(bfptr);

	printw("Budget Fields: %d, %d, %d, %d, %d\n",
	 bh->month,
	 bh->year,
	 bh->catg,
	 bh->transtype, 
	 bh->value);

	free(bh);
	fclose(bfptr);
	FILE *fptr = open_record_csv("r");
	struct RecordHeader *rh = parse_record_header(fptr);

	printw("Record Fields: %d, %d, %d, %d, %d, %d, %d\n", 
	 rh->month,
	 rh->day,
	 rh->year,
	 rh->catg,
	 rh->desc,
	 rh->transtype,
	 rh->value);

	free(rh);
	fclose(fptr);
	getch();
}

/*
 * Returns an vector of byte offsets sorted by date. No actual sorting is done
 * in this function, it is assumed the CSV is already sorted. This is basically
 * just moving memory around for the sake of portability and other sorting
 * selections.
 */
static Vec *sort_by_date(FILE *fptr, Vec *pidx, Vec *plines) {
	Vec *psbd = malloc(sizeof(*psbd) + (sizeof(long) * plines->size));

	if (psbd == NULL) {
		memory_allocate_fail();
	}

	psbd->capacity = plines->size;
	psbd->size = 0;
	rewind(fptr);

	for (size_t i = 0; i < plines->size; i++) {
		psbd->data[i] = pidx->data[plines->data[i]];
		psbd->size++;
	}

	return psbd;
}

/* Returns an array of integers representing the byte offsets of records 
 * sorted by category */
static Vec *sort_by_category
(FILE *fptr, Vec *pidx, Vec *plines, int yr, int mo)
{
	Vec *prsc = malloc(sizeof(*prsc) + (sizeof(long) * REALLOC_INCR));
	prsc->capacity = REALLOC_INCR;
	prsc->size = 0;

	rewind(fptr);
	struct Categories *pc = get_categories(mo, yr);

	char linebuff[LINE_BUFFER];
	char *line;
	char *token;

	for (size_t i = 0; i < pc->size; i++) { // Iterate categories
		prsc->data[prsc->size] = 0;
		prsc->size++;
		for (size_t j = 0; j < plines->size; j++) { // Iterate records
			/* Check prsc->size + 1 because a zero will be added to the array
			 * to mark the spaces between categories */
			if (prsc->size + 1 >= prsc->capacity) {
				prsc->capacity += REALLOC_INCR;
				Vec *tmp =
					realloc(prsc, sizeof(*prsc) + (prsc->capacity * sizeof(char *)));
				if (tmp == NULL) {
					free(prsc);
					prsc = NULL;
					goto err_null;
				}
				prsc = tmp;
			}

			fseek(fptr, pidx->data[plines->data[j]], SEEK_SET);
			line = fgets(linebuff, sizeof(linebuff), fptr);
			if (line == NULL) {
				free(prsc);
				prsc = NULL;
				goto err_null;
			}

			seek_n_fields(&line, 3);
			token = strsep(&line, ",");
			
			if (token == NULL) {
				free(prsc);
				prsc = NULL;
				goto err_null;
			}
			if (strcasecmp(token, pc->categories[i]) == 0) {
				prsc->data[prsc->size] = pidx->data[plines->data[j]];
				prsc->size++;
			}
		}
	}

err_null:
	free_categories(pc);
	return prsc;
}

/* Draws a border around the window 'wptr' with "T" shaped characters to
 * mesh seamlessly with the window border of the sidebar. */
void draw_parent_box_with_sidebar(WINDOW *wptr) {
	wborder(wptr, 0, 0, 0, 0, 0, ACS_TTEE, 0, ACS_BTEE);
	wrefresh(wptr);
}

void nc_read_setup
(int sel_year, int sel_month, int sort, struct ReadRet *rret)
{
	nc_print_main_menu_footer(stdscr);
	if (debug_flag) {
		nc_print_debug_flag(stdscr);
	}
	refresh();

	struct SelRecord sr_, *sr = &sr_;
	struct Datevals dv_, *dates = &dv_;
	dates->month = sel_month;
	dates->year = sel_year;
	sr->flag = -1;

	get_dates(sr, dates);
	if (dates->year < 0 || dates->month < 0) {
		goto err_select_date_fail;
	}

	CategoryNode **nodes;
	FILE *fptr = open_record_csv("r");
	Vec *pidx = index_csv(fptr);
	Vec *plines;
	Vec *psc;
	size_t n_records;
	char *ret;
	bool sidebar_exists;
	// To hold the return value of wgetch()/getch()
	int c;

	if (debug_flag) {
		debug_fields();
	}

	struct ReadWins *wins = create_read_windows();
	if (wins->sidebar_parent == NULL || wins->sidebar_body == NULL) {
		sidebar_exists = false;
	} else {
		sidebar_exists = true;
	}

	/* If plines->size is zero, whatever function cannot return to an
	 * nc_read_setup with month and year parameters, as these dates no longer
	 * hold any records. */
	plines = get_matching_line_nums(fptr, dates->month, dates->year);
	if (plines == NULL) {
		free(pidx);
		fclose(fptr);
		return;
	}

	n_records = plines->size;
	if (plines->size == 0 && sort == SORT_DATE) {
		sort = SORT_CATG;
	}

	if (sort == SORT_DATE) {
		psc = sort_by_date(fptr, pidx, plines);	
	} else {
		psc = sort_by_category(fptr, pidx, plines, dates->year, dates->month);
	}

	nodes = create_category_nodes(dates->month, dates->year);
	if (debug_flag) {
		debug_category_nodes(wins->data, nodes);
	}

	if (sidebar_exists) {
		init_sidebar_parent(wins->sidebar_parent, psc, get_left_to_budget(nodes));
		init_sidebar_body(wins->sidebar_body, nodes, 0);
		draw_parent_box_with_sidebar(wins->parent);
	} else {
		box(wins->parent, 0, 0);
	}

	print_column_headers(wins->parent, BOX_OFFSET);
	nc_print_date(wins->parent, dates->year, dates->month);
	nc_print_sort_text(wins->parent, sort);

	if (sort == SORT_DATE) {
		nc_read_loop(wins, fptr, sr, psc, nodes);
	} else if (sort == SORT_CATG) {
		FILE *bfptr = open_budget_csv("r");
		nc_read_budget_loop(wins, fptr, bfptr, sr, psc, nodes); 
		fclose(bfptr);
	}

	free(psc);
	psc = NULL;
	free(plines);
	plines = NULL;
	if (sr->flag != EDIT_CATG) {
		free_category_nodes(nodes);
		nodes = NULL;
	}
	free_windows(wins);
	wins = NULL;
	free(pidx);
	pidx = NULL;
	fclose(fptr);
	fptr = NULL;

	rret->mo = dates->month;
	rret->yr = dates->year;
	rret->sort = sort;

err_select_date_fail:
	switch(sr->flag) {
	case(NO_SELECT):
		rret->flag = RRET_DEFAULT;
//		nc_read_setup_default();
		break;
	case(ADD):
		nc_print_input_footer(stdscr);
		if (dates->year < 0 || dates->month < 0) {
			struct MenuParams *mp = init_add_main_menu();
			c = nc_input_menu(mp);
			switch (c) {
			case 0:
				nc_add_transaction_default();
				break;
			case 1:
				ret = nc_add_budget_category(0, 0);
				free(ret);
				break;
			case 2:
				nc_create_new_budget();
				break;
			}
			free(mp);
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		} else {
			struct MenuParams *mp = init_add_menu();
			int c = nc_input_menu(mp);
			if (c == 0) {
				nc_add_transaction(dates->year, dates->month);
			} else if (c == 1) {
				ret = nc_add_budget_category(dates->year, dates->month);
				free(ret);
			}
			free(mp);
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		}
		break;
	case(EDIT):
		nc_print_quit_footer(stdscr);
		nc_edit_transaction(sr->index);
		rret->flag = RRET_BYDATE;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(READ):
		rret->flag = RRET_DEFAULT;
//		nc_read_setup_default(rret);
		break;
	case(QUIT):
		rret->flag = RRET_QUIT;
		break;
	case(SORT):
		if (sort == SORT_CATG && n_records == 0) {
			nc_message("Cannot sort by date, no records exist");
			sort = SORT_CATG;
		} else if (sort == SORT_CATG) {
			sort = SORT_DATE;
		} else {
			sort = SORT_CATG;
		}
		rret->flag = RRET_BYDATE;
		rret->sort = sort;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(OVERVIEW):
		nc_overview_setup(dates->year);
		rret->flag = RRET_BYDATE;
//		nc_read_setup(dates->year, dates->month, sort);
		break;
	case(EDIT_CATG):
		nc_edit_category(sr->index, sr->opt, nodes); 
		free_category_nodes(nodes);
		nodes = NULL;
		if (n_records > 0) {
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		} else {
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		}
		break;
	case(RESIZE):
		while (test_terminal_size() == -1) {
			getch();
		}
		if (dates->month > 0 && dates->year > 0) {
			rret->flag = RRET_BYDATE;
//			nc_read_setup(dates->year, dates->month, sort);
		} else {
			rret->flag = RRET_DEFAULT;
//			nc_read_setup_default();
		}
		break;
	case(NO_RCRD):
		c = getch();
		switch(c) {
		case(KEY_F(1)):
		case('a'):
		case('A'):
			sr->flag = ADD;
			goto err_select_date_fail;
			break;
		case(KEY_F(4)):
		case('q'):
		case('Q'):
			rret->flag = RRET_QUIT;
			break;
		default:
			rret->flag = RRET_QUIT;
			break;
		}
		break;
	default:
		rret->flag = RRET_QUIT;
		break;
	}
	refresh();
}

void nc_read_setup_default(struct ReadRet *rret) {
	nc_read_setup(0, 0, SORT_CATG, rret);
}

void nc_read_setup_year(int sel_year, struct ReadRet *rret) {
	nc_read_setup(sel_year, 0, SORT_CATG, rret);
}
