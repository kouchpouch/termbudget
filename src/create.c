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
#include "main.h"
#include "categories.h"
#include "edit_categories.h"
#include "parser.h"
#include "sorter.h"
#include "tui.h"
#include "tui_input.h"
#include "tui_input_menu.h"
#include "filemanagement.h"
#include "file_write.h"
#include "vector.h"
#include "vector_generic.h"

enum copy_category_error {
	COPYCATG_ERR_OK,
	COPYCATG_ERR_NO_PREV,
	COPYCATG_ERR_FGETS,
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
void insert_budget_record(char *catg, int m, int y, int transtype, double amt) 
{
	unsigned int insert_line = sort_budget_csv(m, y);
	FILE *fptr = open_budget_csv("r");
	FILE *tmpfptr;
	char insert_str[LINE_BUFFER] = { 0 };

	snprintf(insert_str, sizeof(insert_str), 
		  "%d,%d,%s,%d,%.2f\n", m, y, catg, transtype, amt);

	tmpfptr = insert_into_file(fptr, insert_str, insert_line);

	mv_tmp_to_budget_file(tmpfptr, fptr);
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
void insert_transaction_record(int insert_line, struct transaction_tokens *ld)
{
	if (!category_exists_in_budget(ld->category, ld->month, ld->year)) {
		insert_budget_record_from_ld(ld);
	} 

	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr;
	char insert_str[LINE_BUFFER] = { 0 };

	line_data_to_string(insert_str, sizeof(insert_str), ld);

	tmpfptr = insert_into_file(fptr, insert_str, insert_line);
	
	mv_tmp_to_record_file(tmpfptr, fptr);
}

/* Optional parameters int month, year. If add transaction is selected while
 * on the read screen these will be auto-filled. */
void create_transaction(int year, int month)
{
	struct transaction_tokens userlinedata_, *uld = &userlinedata_;
	unsigned int result_line;
	struct full_date fd;

	nc_print_input_footer(stdscr);

	if (year == 0 || month == 0) {
		if (nc_input_full_date(month, 0, year, &fd) == -1) {
			return;
		}
	} else {
		if (nc_input_full_date_on_day(month, 0, year, &fd) == -1) {
			return;
		}
	}
	uld->month = fd.month;
	uld->day = fd.day;
	uld->year = fd.year;

	uld->category = nc_select_category(uld->month, uld->year);
	if (uld->category == NULL) {
		return;
	}

	uld->desc = nc_input_string("Enter Description");
	if (uld->desc == NULL) {
		free(uld->category);
		return;
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
	insert_transaction_record(result_line, uld);

input_quit:
	free(uld->category);
	free(uld->desc);
// 	This was causing the nc_read_setup function to be called twice
//	nc_read_setup(uld->year, uld->month, sort);
}

void create_transaction_default(void)
{
	create_transaction(0, 0);
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

/* Checks if there is an entry in the budget.csv file */
static bool previous_budget_exists(void) {
	bool retval;
	FILE *fptr = open_budget_csv("r");
	if (get_total_file_lines(fptr) > 1) {
		retval = true;
	} else {
		retval = false;
	}
	fclose(fptr);
	return retval;
}

/* TODO: Create some generic vector to store the three values that we need, 
 * the month, year, and number of categories. */
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

/* TODO: Create a small window selection containing previous budget date 
 * pairs. */
static void select_budget_date_to_copy(struct full_date *fd)
{
	size_t i, y;
	void *tmp = NULL;
	WINDOW *wptr = NULL;
	struct vec2l *v = NULL;
	struct vec_generic *dates = get_dates_to_copy_from(fd);
	if (dates == NULL) {
		return;
	}

	wptr = newwin_centered(16, 32, stdscr);

	VEC_GENERIC_FOREACH(struct vec2l *, item, dates) {
		printw("Item b: %ld, Item a: %ld\n", item->b, item->a);
	}

	refresh();
	getch();

	for (y = 1, i = 0; i < dates->count && i < 14; y++, i++) {
		tmp = get_vec_generic_reverse(i, dates);
		v = (struct vec2l *)tmp;
		/* -1 to zero index the month */
		mvwprintw(wptr, y, BOX_OFFSET, "%s", fullname_months[v->b - 1]);
		mvwprintw(wptr, y, BOX_OFFSET + strlen("september") + 2, "%ld", v->a);
	}

	box(wptr, 0, 0);
	mvwxcprintw(wptr, 0, "Select Date to Copy From");
	refresh();

	nc_exit_window_key(wptr);
	free_vec_generic(dates);
}

/* For creating a new budget. Returns malloc'd struct full_date which must
 * be free'd by the caller. Use nc_create_new_budget_intret to automatically
 * free the return value. */
struct full_date *nc_create_new_budget(void)
{

	struct full_date *date = malloc(sizeof(*date));
	struct full_date prev_month;
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

/* Quick-n-dirty method of getting the absolute previous month, but this 
 * is obviosuly not sufficient */
	if (previous_budget_exists()) {
		if (confirm_copy_categories()) {
			select_budget_date_to_copy(date);
/*			copy_catg_ret = copy_categories_to_new_budget(&prev_month, date); */
			if (copy_catg_ret != COPYCATG_ERR_OK) {
				print_copy_category_error(copy_catg_ret);
				insert_budget_record("Income", date->month, date->year, TT_INCOME, 0);
				insert_budget_record("Saving", date->month, date->year, TT_EXPENSE, 0);
			}
		}
	} else {
		insert_budget_record("Income", date->month, date->year, TT_INCOME, 0);
		insert_budget_record("Saving", date->month, date->year, TT_EXPENSE, 0);
	}

	return date;
}

/* Wrapper around nc_create_new_budget() but frees the struct full_date and 
 * only returns an int to indicate success or failure.
 * Most of the time this function's return value has no use. Use this to avoid 
 * future bugs */
int nc_create_new_budget_intret(void) {
	struct full_date *d = nc_create_new_budget();
	if (d != NULL) {
		free(d);
		return -1;
	} else {
		return 0;
	}
}

void add_main_with_date(struct dates_flags *dates)
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
		create_transaction(dates->year, dates->month);
		break;

	case ADD_CATG:
		ret = create_budget_record(dates->year, dates->month);
		free(ret);
		break;

	default:
		break;
	}
}

void add_main_no_date(void)
{
	enum add_sel {
		ADD_TRNS = 0,
		ADD_CATG,
		ADD_BUDG
	} add_sel;
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
		nc_create_new_budget_intret();
		break;
	
	default:
		break;
	}
}
