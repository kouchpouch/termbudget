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
#include <ncurses.h>

#include "overview.h"
#include "read_init.h"
#include "read_loops.h"
#include "create.h"
#include "main.h"
#include "edit_transaction.h"
#include "edit_categories.h"
#include "get_date.h"
#include "filemanagement.h"
#include "helper.h"
#include "tui.h"
#include "tui_sidebar.h"
#include "vector.h"
#include "parser.h"
#include "categories.h"
#include "flags.h"

// 'R'ead 'RET'urn values
#define RRET_DEFAULT 0
#define RRET_BYDATE 1
#define RRET_QUIT 2

struct Plannedvals {
	double inc;
	double exp;
};

static const char *abbr_months[] = {
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

struct date_select {
	int selected_date;
	int date_idx;
	int scroll_idx;
	int tmp;
};

static bool duplicate_data_exists(struct vec_d *vec, long y)
{
	for (size_t i = 0; i < vec->size; i++) {
		if (vec->data[i] == y) {
			return true;
		}
	}
	return false;
}

static void combine_dedup_vectors(struct vec_d *vec1,
								  struct vec_d *vec2,
								  struct vec_d *result)
{
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
				if (!duplicate_data_exists(result, tmp1)) {
					result->data[result->size] = tmp1;
					result->size++;
				}
				if (!duplicate_data_exists(result, tmp2)) {
					result->data[result->size] = tmp2;
					result->size++;
				}
			}

		} else if (tmp1 > 0 && tmp2 == 0) {
			if (!duplicate_data_exists(result, tmp1)) {
				result->data[result->size] = tmp1;
				result->size++;
			}
		} else if (tmp2 > 0 && tmp1 == 0) {
			if (!duplicate_data_exists(result, tmp2)) {
				result->data[result->size] = tmp2;
				result->size++;
			}
		}
	}
}

/* Returns a struct vec_d of deduplicated and sorted data from record_years and 
 * budget_years */
static struct vec_d *consolidate_years(struct vec_d *vec1, struct vec_d *vec2)
{
	struct vec_d *result = malloc(sizeof(*result) 
					     + (sizeof(long) * vec1->size) 
					     + (sizeof(long) * vec2->size));

	if (result == NULL) {
		free(vec1);
		free(vec2);
		mem_alloc_fail();
	}

	result->size = 0;
	result->capacity = vec1->size + vec2->size;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

/* Retrieves years with data from RECORD_DIR and BUDGET_DIR CSVs, combines
 * and deduplicates the data, and returns a malloc'd struct vec_d containing an array
 * of years that can be selected for viewing. */
static struct vec_d *get_all_years(void)
{
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	struct vec_d *pr = get_years_with_data(rfptr, 2);
	struct vec_d *pb = get_years_with_data(bfptr, 1);
	struct vec_d *retval;

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

	retval = consolidate_years(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

static struct vec_d *consolidate_months(struct vec_d *vec1, struct vec_d *vec2)
{
	struct vec_d *result = malloc(sizeof(*result) + 
							      (sizeof(long) * 
							      MONTHS_IN_YEAR));
	if (result == NULL) {
		free(vec1);
		free(vec2);
		mem_alloc_fail();
	}

	result->size = 0;
	result->capacity = MONTHS_IN_YEAR;

	combine_dedup_vectors(vec1, vec2, result);

	qsort(result->data, result->size, sizeof(result->data[0]), compare_for_sort);

	return result;
}

/* Retrieves months with data from RECORD_DIR and BUDGET_DIR CSVs, combines
 * and deduplicates the data, and returns a malloc'd struct vec_d containing an array
 * of months that can be selected for viewing. */
static struct vec_d *get_all_months(int year)
{
	FILE *rfptr = open_record_csv("r");
	FILE *bfptr = open_budget_csv("r");

	struct vec_d *pr = get_months_with_data(rfptr, year, 1);
	struct vec_d *pb = get_months_with_data(bfptr, year, 0);
	struct vec_d *retval;

	fclose(rfptr);
	fclose(bfptr);

	retval = consolidate_months(pr, pb);

	free(pr);
	pr = NULL;
	free(pb);
	pb = NULL;

	return retval;
}

/* Iterates through 'months' vector's data member and returns the index of
 * the current month */
static int get_current_mo_idx(struct vec_d *months)
{
	int mo = get_current_month();

	/* months->size can never be more than MONTHS_IN_YEAR or less than zero,
	 * so this cast is safe */
	for (int i = 0; i < (int)months->size; i++) {
		if (months->data[i] - 1 == mo) {
			return i;
		}
	}
	return -1;
}

static void scroll_month(WINDOW *wptr,
						 int tmp,
						 int monlen,
						 int *idx,
						 bool next)
{
	mvwchgat(wptr, tmp, BOX_OFFSET, monlen, A_NORMAL, 0, NULL);
	next ? tmp++ : tmp--;
	mvwchgat(wptr, tmp, BOX_OFFSET, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	wrefresh(wptr);
	next ? (*idx)++ : (*idx)--;
}

/* On a non-select, the return value is the inverted menukeys value */
static int select_month(WINDOW *wptr, int year)
{
	struct vec_d *months_data = get_all_months(year);
	struct date_select scroll = { 0 };
	int monlen = strlen(abbr_months[0]);
	int c = 0;
	int current_mo_idx = get_current_mo_idx(months_data);

	wmove(wptr, BOX_OFFSET, BOX_OFFSET);
	for (size_t i = 0; i < months_data->size; i++) {
		scroll.tmp = getcury(wptr);
		if (months_data->data[i] != 0) {
			wmove(wptr, scroll.tmp, BOX_OFFSET);
			wprintw(wptr, "%s\n", abbr_months[months_data->data[i] - 1]);
			scroll.scroll_idx++;
		}
	}

	if (year == get_current_year() && current_mo_idx != -1) {
		wmove(wptr, BOX_OFFSET + current_mo_idx, BOX_OFFSET);
		scroll.date_idx = current_mo_idx;
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	} else {
		wmove(wptr, BOX_OFFSET, BOX_OFFSET);
		wchgat(wptr, monlen, A_REVERSE, REVERSE_COLOR, NULL);
	}

	box(wptr, 0, 0);
	wrefresh(wptr);

	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		scroll.tmp = getcury(wptr);
		switch (c) {

		case ('j'):
		case KEY_DOWN:
			if (scroll.tmp - BOX_OFFSET + 1 < scroll.scroll_idx) {
				scroll_month(wptr, scroll.tmp, monlen, &scroll.date_idx, true);
			}
			break;

		case ('k'):
		case KEY_UP:
			if (scroll.tmp - 1 >= BOX_OFFSET) {
				scroll_month(wptr, scroll.tmp, monlen, &scroll.date_idx, false);
			}
			break;

		case ('a'):
		case KEY_F(ADD):
			free(months_data);
			months_data = NULL;
			return -(ADD);

		case KEY_RESIZE:
			free(months_data);
			months_data = NULL;

			/* This RESIZE macro has to be greater than 12, which I have no
			 * intention of changing. I hate this. However--I'm lazy. Plus,
			 * all of the resizes in this program suck. */
			return -(RESIZE);

		case ('\n'):
		case ('\r'):
			scroll.selected_date = months_data->data[scroll.date_idx];
			free(months_data);
			months_data = NULL;
			return scroll.selected_date;

		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			free(months_data);
			months_data = NULL;
			return -(QUIT);

		default: 
			break;
		}
	}

	free(months_data);
	months_data = NULL;

	return scroll.selected_date;
}

static void scroll_year(WINDOW *wptr,
						int print_y,
						int tmp,
						int *idx,
						bool next)
{
	mvwchgat(wptr, print_y, tmp, 4, A_NORMAL, 0, NULL);
	next ? (tmp += 5) : (tmp -= 5);
	mvwchgat(wptr, print_y, tmp, 4, A_REVERSE, REVERSE_COLOR, NULL);
	wrefresh(wptr);
	next ? (*idx)++ : (*idx)--;
}

/* On a non-select, the return value is the inverted menukeys value */
static int select_year(WINDOW *wptr)
{
	keypad(wptr, true);	

	struct vec_d *years = get_all_years();
	if (years == NULL) {
		return -(NO_RCRD);
	}
	int n_years = size_to_int(years->size);
	if (n_years < 0) {
		printf("Too many years");
		exit(1);
	}
	struct date_select scroll = { 0 };
	int print_y = 1;
	int print_x = 2;
	int c = 0;
	int init_rv_x = print_x;

	box(wptr, 0, 0);
	wrefresh(wptr);
	wmove(wptr, print_y, print_x);

	for (size_t i = 0; i < years->size; i++) {
		if (years->data[i] == get_current_year()) {
			scroll.date_idx = i;
		}
		wprintw(wptr, "%ld ", years->data[i]);
		wrefresh(wptr);
	}

	/* Initially highlight the current year and move cursor to it */
	if (scroll.date_idx > 0) {
		init_rv_x += (4 * scroll.date_idx) + scroll.date_idx;
		scroll.scroll_idx = scroll.date_idx;
	}
	
	mvwchgat(wptr, print_y, init_rv_x, 4, A_REVERSE, REVERSE_COLOR, NULL);
	wrefresh(wptr);

	while (c != KEY_F(QUIT) && c != 'q') {
		c = wgetch(wptr);
		scroll.tmp = getcurx(wptr);
		switch (c) {

		case ('l'):
		case KEY_RIGHT:
			if (scroll.scroll_idx + 1 < n_years) {
				scroll_year(wptr, print_y, scroll.tmp, &scroll.scroll_idx, true);
			}
			break;

		case ('h'):
		case KEY_LEFT:
			if (scroll.scroll_idx > 0) {
				scroll_year(wptr, print_y, scroll.tmp, &scroll.scroll_idx, false);
			}
			break;

		case ('a'):
		case KEY_F(ADD):
			free(years);
			years = NULL;
			return -(ADD);

		case KEY_RESIZE:
			free(years);
			years = NULL;
			return -(RESIZE);

		case ('\n'):
		case ('\r'):
			scroll.selected_date = years->data[scroll.scroll_idx];
			free(years);
			years = NULL;
			return scroll.selected_date;

		case ('Q'):
		case ('q'):
		case KEY_F(QUIT):
			free(years);
			return -(QUIT);

		default: 
			break;
		}
	}

	free(years);
	years = NULL;

	return scroll.selected_date;
}

static int input_year(WINDOW *wptr)
{
	int yr = select_year(wptr);

	if (yr == -(NO_RCRD)) {
		mvwxcprintw(wptr, getmaxy(wptr) / 2, 
			  "No records exist, add (F1) to get started");
		wgetch(wptr);
		return -(NO_RCRD);
	} else if (yr < 0) {
		return yr;
	}

	return yr;
}

static void get_dates(struct record_select *sr, struct month_year *dates)
{
	WINDOW *wptr = newwin(LINES - 1, 0, 0, 0);
	box(wptr, 0, 0);

	if (!month_or_year_exists(dates->month, dates->year)) {
		dates->month = 0;
		dates->year = 0;
	}

	if (!dates->year) {
		dates->year = input_year(wptr);
		if (dates->year < 0) {
			dates->month = -1;
			sr->flag = -(dates->year);
			nc_exit_window(wptr);
			return;
		}
	}

	if (!dates->month) {
		dates->month = select_month(wptr, dates->year);
		if (dates->month < 0) {
			sr->flag = -(dates->month);
			nc_exit_window(wptr);
			return;
		}
	}
	
	nc_exit_window(wptr);
}

static void debug_fields(void)
{
	FILE *bfptr = open_budget_csv("r");
	struct budget_header *bh = parse_budget_header(bfptr);
	FILE *fptr = open_record_csv("r");
	struct record_header *rh = parse_record_header(fptr);

	move(0,0);

	printw("Budget Fields: %d, %d, %d, %d, %d\n",
	 bh->month,
	 bh->year,
	 bh->catg,
	 bh->transtype, 
	 bh->value);

	printw("Record Fields: %d, %d, %d, %d, %d, %d, %d\n", 
	 rh->month,
	 rh->day,
	 rh->year,
	 rh->catg,
	 rh->desc,
	 rh->transtype,
	 rh->value);

	free(bh);
	fclose(bfptr);
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
static struct vec_d *sort_by_date(FILE *fptr,
								  struct vec_d *pidx,
								  struct vec_d *plines)
{
	struct vec_d *psbd = malloc(sizeof(*psbd) + (sizeof(long) * plines->size));
	if (psbd == NULL) {
		mem_alloc_fail();
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
static struct vec_d *sort_by_category(FILE *fptr,
									  struct vec_d *pidx,
									  struct vec_d *plines,
									  int yr,
									  int mo)
{
	struct vec_d *prsc = malloc(sizeof(*prsc) + (sizeof(long) * REALLOC_INCR));
	if (prsc == NULL) {
		free(pidx);
		free(plines);
		mem_alloc_fail();
	}

	prsc->capacity = REALLOC_INCR;
	prsc->size = 0;

	struct catg_vec *pc = get_categories(mo, yr);

	char linebuff[LINE_BUFFER];
	char *line;
	char *token;

	rewind(fptr);

	for (size_t i = 0; i < pc->size; i++) { // Iterate categories
		prsc->data[prsc->size] = 0;
		prsc->size++;
		for (size_t j = 0; j < plines->size; j++) { // Iterate records
			/* Check prsc->size + 1 because a zero will be added to the array
			 * to mark the spaces between categories */
			if (prsc->size + 1 >= prsc->capacity) {
				prsc->capacity += REALLOC_INCR;
				struct vec_d *tmp = realloc(prsc, sizeof(*prsc) 
					    		   + (prsc->capacity * sizeof(char *)));
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
static void draw_parent_box_with_sidebar(WINDOW *wptr)
{
	wborder(wptr, 0, 0, 0, 0, 0, ACS_TTEE, 0, ACS_BTEE);
	wrefresh(wptr);
}

static void print_date(WINDOW *wptr, int yr, int mo)
{
	int y = 0;
	int x = BOX_OFFSET;
	mvwprintw(wptr, y, x, "%s %d", abbr_months[mo - 1], yr);
	wrefresh(wptr);
}

static void print_sort_text(WINDOW *wptr, int sort)
{
	char *text;
	sort == SORT_DATE ? (text = "Date") : (text = "Category");
	int y = 0;
	int x = getmaxx(wptr) - strlen(text) - strlen("Sort By: ") - BOX_OFFSET;

	mvwprintw(wptr, y, x, "Sort By: %s", text);
	wrefresh(wptr);
}

static void free_windows(struct ReadWins *wins)
{
	nc_exit_window(wins->data);
	nc_exit_window(wins->parent);
	if (wins->sidebar_parent != NULL) {
		nc_exit_window(wins->sidebar_parent);
		nc_exit_window(wins->sidebar_body);
	}
	free(wins);
}

static struct Plannedvals *get_total_planned(struct catg_node *head)
{
	struct catg_node *tmp = head;
	struct Plannedvals *pv = malloc(sizeof(*pv));
	if (pv == NULL) {
		mem_alloc_fail();
	}

	pv->exp = 0.0;
	pv->inc = 0.0;

	while (1) {
		struct budget_tokens *bt = tokenize_budget_fpi(tmp->catg_fp);
		if (tmp->next == NULL) {
			if (bt->transtype == TT_INCOME) {
				pv->inc += bt->amount;
			} else {
				pv->exp += bt->amount;
			}
			free_budget_tokens(bt);
			bt = NULL;
			break;

		} else {
			if (bt->transtype == TT_INCOME) {
				pv->inc += bt->amount;
			} else {
				pv->exp += bt->amount;
			}
			free_budget_tokens(bt);
			bt = NULL;
		}
		tmp = tmp->next;
	}
	return pv;
}

static double get_left_to_budget(struct catg_node *head)
{
	struct Plannedvals *pv = get_total_planned(head);
	double ret = pv->inc - pv->exp;
	free(pv);
	pv = NULL;
	return ret;
}

static void cleanup_read_setup(struct vec_d *rec_fpis,
							   struct vec_d *rec_line_nums,
							   struct vec_d *pidx,
							   struct ReadWins *wins,
							   FILE *fptr)
{
	free(rec_fpis);
	free(rec_line_nums);
	free(pidx);
	free_windows(wins);
	fclose(fptr);

	rec_fpis = NULL;
	rec_line_nums = NULL;
	pidx = NULL;
}

void nc_read_setup(int sel_year,
				   int sel_month, 
				   int sort, 
				   struct read_retvals *rret)
{
	nc_print_main_menu_footer(stdscr);
	if (debug_flag) {
		nc_print_debug_flag(stdscr);
	}
	refresh();

	struct record_select rs = {
		.flag = -1
	};
	struct month_year date = {
		.month = sel_month,
		.year = sel_year
	};

	get_dates(&rs, &date);
	if (date.year < 0 || date.month < 0) {
		goto err_select_date_fail;
	}

	FILE *fptr = open_record_csv("r");
	struct catg_node *catg_head = NULL;
	struct vec_d *pidx = index_csv(fptr);
	struct vec_d *rec_line_nums = NULL;
	struct vec_d *rec_fpis = NULL;
	size_t n_records;
	bool sidebar_exists;
	/* To hold the return value of wgetch()/getch() */
	int c;
	struct ReadWins *wins = create_read_windows();

	if (debug_flag) {
		debug_fields();
	}

	if (wins->sidebar_parent == NULL || wins->sidebar_body == NULL) {
		sidebar_exists = false;
	} else {
		sidebar_exists = true;
	}

	/* If rec_line_nums->size is zero, whatever function cannot return to an
	 * nc_read_setup with month and year parameters, as these dates no longer
	 * hold any records. */
	rec_line_nums = get_matching_line_nums(fptr, date.month, date.year);
	if (rec_line_nums == NULL) {
		free(pidx);
		fclose(fptr);
		return;
	}

	n_records = rec_line_nums->size;
	if (rec_line_nums->size == 0 && sort == SORT_DATE) {
		sort = SORT_CATG;
	}

	if (sort == SORT_DATE) {
		rec_fpis = sort_by_date(fptr, pidx, rec_line_nums);	
	} else {
		rec_fpis = sort_by_category(fptr, pidx, rec_line_nums, date.year, date.month);
	}

	catg_head = create_catg_node_list(date.month, date.year);

	if (debug_flag) {
		debug_print_catg_node_data(catg_head);
		getch();
	}

	if (sidebar_exists) {
		init_sidebar_parent(wins->sidebar_parent, 
					        rec_fpis,
					        get_left_to_budget(catg_head));
		init_sidebar_body(wins->sidebar_body, catg_head, 0);
		draw_parent_box_with_sidebar(wins->parent);
	} else {
		box(wins->parent, 0, 0);
	}

	print_column_headers(wins->parent, BOX_OFFSET);
	print_date(wins->parent, date.year, date.month);
	print_sort_text(wins->parent, sort);

	if (sort == SORT_DATE) {
		nc_read_loop(wins, fptr, &rs, rec_fpis, catg_head);
	} else if (sort == SORT_CATG) {
		FILE *bfptr = open_budget_csv("r");
		nc_read_budget_loop(wins, fptr, bfptr, &rs, rec_fpis, catg_head); 
		fclose(bfptr);
	}

	cleanup_read_setup(rec_fpis, rec_line_nums, pidx, wins, fptr);
	if (rs.flag != EDIT_CATG) {
		free_catg_nodes(catg_head);
		catg_head = NULL;
	}

	rret->mo = date.month;
	rret->yr = date.year;
	rret->sort = sort;

err_select_date_fail:
	switch (rs.flag) {

	case NO_SELECT:
		rret->flag = RRET_DEFAULT;
		break;

	case ADD:
		nc_print_input_footer(stdscr);
		if (date.year < 0 || date.month < 0) {
			add_main_no_date();
			rret->flag = RRET_DEFAULT;
		} else {
			add_main_with_date(&date);
			rret->flag = RRET_BYDATE;
		}
		break;

	case EDIT:
		nc_print_quit_footer(stdscr);
		nc_edit_transaction(rs.index);
		rret->flag = RRET_BYDATE;
		break;

	case READ:
		rret->flag = RRET_DEFAULT;
		break;

	case QUIT:
		rret->flag = RRET_QUIT;
		break;

	case SORT:
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
		break;

	case OVERVIEW:
		nc_overview_setup(date.year);
		rret->flag = RRET_BYDATE;
		break;

	case EDIT_CATG:
		nc_edit_category(rs.index, rs.opt, catg_head); 
		if (n_records > 0 || catg_head != NULL) {
			free_catg_nodes(catg_head);
			catg_head = NULL;
			rret->flag = RRET_BYDATE;
		} else {
			free_catg_nodes(catg_head);
			catg_head = NULL;
			rret->flag = RRET_DEFAULT;
		}
		break;

	case RESIZE:
		while (test_terminal_size() == -1) {
			getch();
		}
		if (date.month > 0 && date.year > 0) {
			rret->flag = RRET_BYDATE;
		} else {
			rret->flag = RRET_DEFAULT;
		}
		break;

	case NO_RCRD:
		c = getch();
		switch (c) {
		case KEY_F(1):
		case ('a'):
		case ('A'):
			rs.flag = ADD;
			goto err_select_date_fail;
			break;
		case KEY_F(4):
		case ('q'):
		case ('Q'):
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

void nc_read_setup_default(struct read_retvals *rret)
{
	nc_read_setup(0, 0, SORT_CATG, rret);
}

void nc_read_setup_year(int sel_year, struct read_retvals *rret)
{
	nc_read_setup(sel_year, 0, SORT_CATG, rret);
}
