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

#include <assert.h>
#include <stdio.h>
#include <ncurses.h>
#include <string.h>

#include "create.h"
#include "dynamic_string.h"
#include "get_date.h"
#include "helper.h"
#include "main.h"
#include "categories.h"
#include "edit_categories.h"
#include "parser.h"
#include "read_init.h"
#include "sorter.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "filemanagement.h"
#include "file_write.h"
#include "vector.h"
#include "vector_generic.h"
#include "flags.h"

enum copy_category_error {
	COPYCATG_ERR_OK,
	COPYCATG_ERR_NO_PREV,
	COPYCATG_ERR_FGETS,
};

enum copy_loop_how {
	COPYLOOP_INIT = 0,
	COPYLOOP_SCROLL_DN,
	COPYLOOP_SCROLL_UP,
};

/* Creates an ncurses subwindow and returns the user's input as a boolean */
static bool confirm_budget_category(char *catg, double amt)
{
	WINDOW *wptr = create_input_subwindow();
	int maxy = getmaxy(wptr);
	int maxx = getmaxx(wptr);
	char *msg = "Confirm Category";
	bool retval;

	mvwprintw(wptr, 0, (maxx / 2 - strlen(msg) / 2), "%s", msg);
	mvwprintw(wptr, 3, (maxx / 2 - strlen(catg) / 2), "%s", catg);
	mvwprintw(wptr, 4, (maxx / 2 - intlen(amt) / 2) - 2, "$%.2f", amt);
	mvwxcprintw(wptr, maxy - BOX_OFFSET, "(Y)es  /  (N)o");

	retval = nc_confirm_input_loop(wptr);

	nc_exit_window(wptr);
	return retval;
}

static void insert_budget_record_from_ld(struct transaction_tokens *ld)
{
	insert_budget_record
	(ld->category, ld->month, ld->year, ld->transtype, 0.0);
}

/* Writes the a csv record containing the data passed into the arguments
 * to budget.csv into the appropriate line sorted by date. */
int insert_budget_record(char *catg, int m, int y, int transtype, double amt) 
{
	unsigned int insert_line = sort_budget_csv(m, y);
	FILE *fptr = open_budget_csv("r");
	FILE *tmpfptr;
	char insert_str[LINE_BUFFER] = { 0 };

	snprintf(insert_str, sizeof(insert_str), 
		  "%d,%d,%s,%d,%.2f\n", m, y, catg, transtype, amt);

	tmpfptr = insert_into_file(fptr, insert_str, insert_line);

	return mv_tmp_to_budget_file(tmpfptr, fptr);
}

/* For a la carte budget category creation, returns a malloc'd char * which
 * is free'd by the caller */
char *create_budget_record(int yr, int mo)
{
	char *catg;
	int transtype;
	struct catg_vec *psc;
	double amt;

	if (mo == 0 || yr == 0) {
		yr = nc_input_year(0);
		if (yr == -1) {
			return NULL;
		}
		mo = nc_input_month(0, yr);
		if (mo == -1) {
			return NULL;
		}
	}

	catg = nc_input_string("Enter Category");
	if (catg == NULL) {
		return NULL;
	}
	transtype = nc_input_category_type();
	if (transtype < 0) {
		free(catg);
		return NULL;
	}

	psc = get_budget_catg_by_date(mo, yr);
	if (duplicate_category_exists(psc, catg)) {
		nc_message("That Category Already Exists");
		free(catg);
		free_categories(psc);
		return NULL;
	}

	amt = nc_input_budget_amount();
	if (confirm_budget_category(catg, amt)) {
		insert_budget_record(catg, mo, yr, transtype, amt);
	}

	free_categories(psc);
	return catg;
}

/* Adds a record to the CSV on line linetoadd */
int insert_transaction_record(int insert_line, struct transaction_tokens *ld)
{
	if (!category_exists_in_budget(ld->category, ld->month, ld->year)) {
		insert_budget_record_from_ld(ld);
	} 

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr;
	char insert_str[LINE_BUFFER] = { 0 };

	line_data_to_string(insert_str, sizeof(insert_str), ld);

	tmpfptr = insert_into_file(fptr, insert_str, insert_line);
	
	return mv_tmp_to_record_file(tmpfptr, fptr);
}

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. Returns 1 on failure, 0 on
 * success */
int create_transaction(int year, int month)
{
	struct transaction_tokens userlinedata_, *uld = &userlinedata_;
	unsigned int result_line;
	struct full_date fd;
	int retval = 1;

	nc_print_input_footer(stdscr);

	if (year == 0 || month == 0) {
		if (nc_input_full_date(month, 0, year, &fd) == -1) {
			return 1;
		}
	} else {
		if (nc_input_full_date_on_day(month, 0, year, &fd) == -1) {
			return 1;
		}
	}
	uld->month = fd.month;
	uld->day = fd.day;
	uld->year = fd.year;

	uld->category = nc_select_category(uld->month, uld->year);
	if (uld->category == NULL) {
		return 1;
	}

	uld->desc = nc_input_string("Enter Description");
	if (uld->desc == NULL) {
		free(uld->category);
		return 1;
	}

	uld->transtype = nc_input_transaction_type();
	if (uld->transtype < 0) {
		goto input_quit;
	}

	uld->amount = nc_input_amount();
	if (uld->amount < 0) {
		goto input_quit;
	}

	if (!nc_confirm_record(uld)) {
		goto input_quit;
	}

	result_line = sort_record_csv(uld->month, uld->day, uld->year);
	retval = insert_transaction_record(result_line, uld);

input_quit:
	free(uld->category);
	free(uld->desc);

// 	This was causing the nc_read_setup function to be called twice
//	nc_read_setup(uld->year, uld->month, sort);

	return retval;
}

int create_transaction_default(void)
{
	return create_transaction(0, 0);
}

static struct MenuParams *init_add_main_menu(void)
{
	int n_str = 3;
	int idx = 0;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * n_str));
	if (mp == NULL) {
		mem_alloc_fail();
	}

	mp->items = n_str;
	mp->title = "Select Data Type to Add";
	mp->strings[idx++] = "Add Transaction";
	mp->strings[idx++] = "Add Category";
	mp->strings[idx++] = "Create New Budget";

	assert(idx == n_str);

	return mp;
}

/* Creates an ncurses selection menu and returns the value of the selected
 * item */
int get_add_selection(void)
{
	enum AddMainMenu add_sel;
	struct MenuParams *mp = init_add_main_menu();
	add_sel = nc_input_menu(mp);
	free(mp);
	return add_sel;
}

static struct MenuParams *init_add_menu(void)
{
	int n_str = 2;
	int idx = 0;
	struct MenuParams *mp = malloc(sizeof(*mp) + (sizeof(char *) * n_str));
	if (mp == NULL) {
		mem_alloc_fail();
	}

	mp->items = n_str;
	mp->title = "Select Data Type to Add";
	mp->strings[idx++] = "Add Transaction";
	mp->strings[idx++] = "Add Category";

	assert(idx == n_str);

	return mp;
}

/* Tokenize the budget string, find the line number to insert the date on, 
 * change date fields, write to tmp file and move. Use the budget tokens to 
 * string func in file write.c */
enum copy_category_error copy_categories_to_new_budget
(struct full_date *date_src, struct full_date *date_dst)
{
	struct vec_d *old_fpis = get_budget_catg_by_date_bo(date_src->month,
													    date_src->year);
	struct d_string *d_str = create_d_string(old_fpis->size + 
										  (old_fpis->size * LINE_BUFFER));
	if (old_fpis->size == 0) {
		free(old_fpis);
		free(d_str);
		return COPYCATG_ERR_NO_PREV;
	}

	struct budget_tokens_buff tokens;
	FILE *bfptr = NULL;
	FILE *tmpfptr = NULL;
	char *old_str;
	char linebuff[LINE_BUFFER] = { 0 };
	char new_str[LINE_BUFFER] = { 0 };
	unsigned int insert_line = sort_budget_csv(date_dst->month, date_dst->year);

	for (size_t i = 0; i < old_fpis->size; i++) {
		bfptr = open_budget_csv("r");
		fseek(bfptr, old_fpis->data[i], SEEK_SET);
		old_str = fgets(linebuff, sizeof(linebuff), bfptr);
		if (old_str == NULL) {
			fclose(bfptr);
			free(old_fpis);
			free(d_str);
			return COPYCATG_ERR_FGETS;
		} else {
			tokenize_budget_string(&tokens, old_str);
			tokens.m = date_dst->month;
			tokens.y = date_dst->year;
			budget_tokens_buffer_to_string(new_str, sizeof(new_str), &tokens);
			concatenate_d_string(&d_str, new_str, strlen(new_str));
			memset(new_str, 0, sizeof(new_str));
		}
	}

	tmpfptr = insert_into_file(bfptr, d_str->string, insert_line);
	mv_tmp_to_budget_file(tmpfptr, bfptr);

	free(old_fpis);
	old_fpis = NULL;

	free(d_str);
	d_str = NULL;

	return COPYCATG_ERR_OK;
}

bool confirm_copy_categories(void)
{
	int c = 0;
	WINDOW *wptr = create_input_subwindow();
	mvwxcprintw(wptr, INPUT_MSG_Y_OFFSET, "Copy Categories from Previous Month?");
	mvwxcprintw(wptr, getmaxy(wptr) - BOX_OFFSET, "(Y)es  /  (N)o");

	while (c != 'q') {
		c = wgetch(wptr);
		switch (c) {
		case ('Y'):
		case ('y'):
			nc_exit_window(wptr);
			return true;

		case ('N'):
		case ('n'):
			nc_exit_window(wptr);
			return false;

		default:
			break;
		}
	}

	nc_exit_window(wptr);
	return false;
}

void print_copy_category_error(enum copy_category_error e)
{
	switch (e) {
	case COPYCATG_ERR_OK:
		printw("ERROR OK");
		break;
	case COPYCATG_ERR_FGETS:
		printw("COULD NOT READ LINE");
		break;
	case COPYCATG_ERR_NO_PREV:
		printw("NO PREVIOUS DATE");
		break;
	}
	refresh();
	getch();
}

static bool previous_budget_exists(struct full_date *date)
{
	FILE *fptr = open_budget_csv("r");
	unsigned int total_lines = get_total_file_lines(fptr);
	unsigned int insert_line = sort_budget_csv(date->month, date->year);
	bool retval = false;

	if (debug_flag) {
		printw("Insertion Line: %u, Total Lines: %u\n",
		 insert_line,
		 total_lines);
	}

	if (total_lines > 1 && sort_budget_csv(date->month, date->year) != 1) {
		retval = true;
	}

	fclose(fptr);
	return retval;
}

static struct vec_generic *get_dates_to_copy_from(struct full_date *fd)
{
	FILE *bfptr = open_budget_csv("r");
	struct vec_generic *vg = create_vec_generic(sizeof(struct vec2l), 0);
	struct vec2l selections = { 0 };

	/* TODO: Create an enumeration to handle the field value, this manual
	 * way is prone to bugs. */
	struct vec_d *years  = get_years_with_data(bfptr, 1);
	if (years == NULL) {
		free_vec_generic(vg);
		fclose(bfptr);
		return NULL;
	}
	struct vec_d *months = NULL;

	for (size_t i = 0; i < years->size; i++) {
		selections.a = years->data[i];
		months = get_months_with_data(bfptr, years->data[i], 0);
		if (months == NULL) {
			break;
		}
		for (size_t j = 0; j < months->size; j++) {
			selections.b = months->data[j];
			if (selections.a <= fd->year) {
				if ((selections.a < fd->year && selections.b != 0) || 
					(selections.a == fd->year && selections.b < fd->month 
					&& selections.b != 0)) {
					push_vec_generic(&selections, sizeof(struct vec2l), vg);
				}
			}
		}
		free(months);
		months = NULL;
	};

	free(years);
	fclose(bfptr);
	return vg;
}

static void draw_borders(WINDOW *parent,
						 WINDOW *left,
						 WINDOW *right,
						 int x_split)
{
	char *l_header  = "Budget to Copy";
	char *r_header = "Preview";
	int l_header_pos = (getmaxx(left) / 2) - (strlen(l_header) / 2) + 1;
	int r_header_pos = x_split + (getmaxx(right) / 2 - strlen(l_header) / 2) + 2;
	mvwvline(parent, 1, x_split, 0, getmaxx(parent) - BOX_OFFSET);
	box(parent, 0, 0);
	mvwhline(parent, 0, x_split, ACS_TTEE, 1);
	mvwhline(parent, getmaxy(parent) - 1, x_split, ACS_BTEE, 1);

	mvwprintw(parent, 0, l_header_pos, "%s", l_header); 
	mvwprintw(parent, 0, r_header_pos, "%s", r_header); 

	refresh();
	wrefresh(parent);
}

static int create_copy_windows(WINDOW **parent,
							   WINDOW **left,
							   WINDOW **right,
							   int *max_x,
							   int *max_y,
							   int x_split)
{
	int ret = 0;
	if (*max_x > MIN_ROWS || *max_y > MIN_COLUMNS) {
		if (*max_x > getmaxx(stdscr)) {
			*max_x = getmaxx(stdscr);
			ret = 1;
		}
		if (*max_y > getmaxy(stdscr)) {
			*max_y = getmaxy(stdscr);
			ret = 1;
		}
	}

	int begin_y = (getmaxy(stdscr) / 2) - (*max_y / 2);
	int begin_x = (getmaxx(stdscr) / 2) - (*max_x / 2);
	*parent = newwin(*max_y, *max_x, begin_y, begin_x); 
	*left = newwin(*max_y - BOX_OFFSET, x_split - 1, begin_y + 1, begin_x + 1);
	*right = newwin(*max_y - BOX_OFFSET, *max_x - BOX_OFFSET - x_split, \
				    begin_y + 1, begin_x + x_split + 1);

	return ret;
}

static void print_preview(struct catg_vec *catg_str, WINDOW *rightsw)
{
	int j;
	int maxx;
	size_t i;

	wclear(rightsw);
	maxx = getmaxx(rightsw);

	for (i = 0, j = 0; i < catg_str->size && j < getmaxy(rightsw); i++, j++) {
		/* Only print as many characters as can fix on a single 
		 * line in rightsw */
		mvwprintw(rightsw, i, 0, "%.*s", maxx, catg_str->categories[i]);
		mvwchgat(rightsw, i, 0, -1, A_NORMAL, category_color(i), NULL); 
	}
	wrefresh(rightsw);
}

static int print_copy_loop(size_t *scrl_idx,
						   struct full_date *new_date,
						   struct vec_generic *dates,
						   WINDOW *leftsw,
						   WINDOW *rightsw,
						   enum copy_loop_how how)
{
	struct catg_vec *catg_str = NULL;
	struct vec2l *curr = NULL;
	switch (how) {
	
	case(COPYLOOP_INIT):
		if (*scrl_idx != 0) {
			return -1;
		}
		break;

	case(COPYLOOP_SCROLL_DN):
		unhighlight(leftsw, *scrl_idx, 0, getmaxx(leftsw));
		(*scrl_idx)++;
		break;

	case(COPYLOOP_SCROLL_UP):
		unhighlight(leftsw, *scrl_idx, 0, getmaxx(leftsw));
		(*scrl_idx)--;
		break;
	}

	curr = get_vec_generic_reverse(*scrl_idx, dates);
	new_date->month = curr->b;
	new_date->year = curr->a;
	catg_str = get_budget_catg_by_date(curr->b, curr->a);

	print_preview(catg_str, rightsw);
	highlight(leftsw, *scrl_idx, 0, getmaxx(leftsw));
	free_categories(catg_str);
	catg_str = NULL;

	wrefresh(leftsw);
	wrefresh(rightsw);
	return 0;
}

static struct full_date *copy_loop(struct vec_generic *dates,
								   WINDOW *leftsw,
								   WINDOW *rightsw)
{
	int c = 0;
	size_t scrl_idx = 0;
	struct full_date *new_date = malloc(sizeof(struct full_date));

	print_copy_loop(&scrl_idx, new_date, dates, leftsw, rightsw, COPYLOOP_INIT);

	while (c != 'q') {
		c = getch();
		switch(c) {

		case ('j'):
		case KEY_DOWN:
			if (scrl_idx < dates->count - 1) {
				print_copy_loop(&scrl_idx, 
					new_date, dates, leftsw, rightsw, COPYLOOP_SCROLL_DN);
			}
			break;

		case ('k'):
		case KEY_UP:
			if (scrl_idx > 0) {
				print_copy_loop(&scrl_idx, 
					new_date, dates, leftsw, rightsw, COPYLOOP_SCROLL_UP);
			}
			break;

		CASE_QUIT
			free(new_date);
			return NULL;
			break;

		CASE_ENTER
			return new_date;

		default:
			break;
		}
	}

	return new_date;
}

static struct full_date *select_budget_date_to_copy(struct full_date *fd)
{
	int print_y = 0;
	int x_split = strlen(LONGEST_LEN_MONTH) + MAX_LEN_YEAR + 8;
	int max_y = 16;
	int max_x = MIN_COLUMNS + 20;
	WINDOW *parentw, *leftsw, *rightsw;
	struct vec_generic *dates = get_dates_to_copy_from(fd);
	struct full_date *new_date = NULL;
	if (dates == NULL) {
		return NULL;
	}

	create_copy_windows(&parentw, &leftsw, &rightsw, &max_x, &max_y, x_split);
	draw_borders(parentw, leftsw, rightsw, x_split);

	/* Print each month and year pair */
	VEC_GENERIC_FOREACH_REVERSE(struct vec2l *, item, dates) {
		mvwprintw(leftsw, print_y, 0, "%s", fullname_months[item->b - 1]);
		mvwprintw(leftsw, print_y, getmaxx(leftsw) - MAX_LEN_YEAR, "%ld", item->a);
		wclear(rightsw);
		print_y++;
		if (print_y > max_y - BOX_OFFSET) {
			break;
		}
	}

	new_date = copy_loop(dates, leftsw, rightsw);

	free_vec_generic(dates);
	nc_exit_window(parentw);
	nc_exit_window(leftsw);
	nc_exit_window(rightsw);

	return new_date;
}

static void create_default_budget(struct full_date *date)
{
	insert_budget_record("Income", date->month, date->year, TT_INCOME, 0);
	insert_budget_record("Saving", date->month, date->year, TT_EXPENSE, 0);
}

/* For creating a new budget. Returns malloc'd struct full_date which must
 * be free'd by the caller. Use create_new_budget_intret to automatically
 * free the return value. */
struct full_date *create_new_budget(void)
{

	struct full_date *date = malloc(sizeof(*date));
	struct full_date *copy_date = NULL;
	int copy_catg_ret = 0;
	if (date == NULL) {
		mem_alloc_fail();
	}

	date->month = get_current_month() + 1;
	date->year = get_current_year();

	if (nc_input_month_and_year(date->month, date->year, date) != 0) {
		free(date);
		return NULL;
	}

	if (month_or_year_exists(date->month, date->year)) {
		free(date);
		nc_message("A budget already exists for that month");
		return NULL;
	}

	if (previous_budget_exists(date) && confirm_copy_categories()) {
		copy_date = select_budget_date_to_copy(date);
		if (copy_date != NULL) {
			copy_catg_ret = copy_categories_to_new_budget(copy_date, date); 
			if (copy_catg_ret != COPYCATG_ERR_OK) {
				print_copy_category_error(copy_catg_ret);
				create_default_budget(date);
			}
		}
	} else {
		create_default_budget(date);
	}

	return date;
}

/* Wrapper around create_new_budget() but frees the struct full_date and 
 * only returns an int to indicate success or failure.
 * Most of the time this function's return value has no use. Use this to avoid 
 * future bugs */
int create_new_budget_intret(void) {
	struct full_date *d = create_new_budget();
	if (d != NULL) {
		free(d);
		return -1;
	} else {
		return 0;
	}
}

void add_main_with_date(struct short_date *date)
{
	enum add_sel {
		ADD_TRNS = 0,
		ADD_CATG
	} add_sel;
	char *ret;
	struct MenuParams *mp = init_add_menu();
	add_sel = nc_input_menu(mp);
	free(mp);

	switch (add_sel) {
	case ADD_TRNS:
		create_transaction(date->year, date->month);
		break;

	case ADD_CATG:
		ret = create_budget_record(date->year, date->month);
		free(ret);
		break;

	default:
		break;
	}
}

void add_main_no_date(struct read_state *rs)
{
	enum add_sel {
		ADD_TRNS = 0,
		ADD_CATG,
		ADD_BUDG
	} add_sel;
	struct full_date *date;
	char *ret;
	struct MenuParams *mp = init_add_main_menu();
	add_sel = nc_input_menu(mp);
	free(mp);

	switch (add_sel) {
	case ADD_TRNS:
		create_transaction_default();
		break;

	case ADD_CATG:
		ret = create_budget_record(0, 0);
		free(ret);
		break;

	case ADD_BUDG:
		date = create_new_budget();
		if (date != NULL) {
			rs->year = date->year;
			rs->month = date->month;
			rs->flag = RRET_BYDATE;
			free(date);
		}
		break;
	
	default:
		break;
	}
}
