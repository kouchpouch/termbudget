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
#include <assert.h>
#include <limits.h>

#include "main.h"
#include "helper.h"
#include "create.h"
#include "sorter.h"
#include "input.h"
#include "filemanagement.h"
#include "file_write.h"
#include "flags.h"

static void print_record_vert(struct transaction_tokens *ld)
{
	printf(
		"1.) Date-->  %d/%d/%d\n"
		"2.) Cat.-->  %s\n"
		"3.) Desc-->  %s\n"
		"4.) Type-->  %s\n"
		"5.) Amt.-->  $%.2f\n",
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype == 0 ? "Expense" : "Income", 
		ld->amount
	);
}

static void print_record_hr(struct transaction_tokens *ld)
{
	printf(
		"%d.) %d/%d/%d Category: %s Description: %s, %s, $%.2f\n",
		ld->linenum, 
		ld->month, 
		ld->day, 
		ld->year, 
		ld->category, 
		ld->desc, 
		ld->transtype == 0 ? "Expense" : "Income", 
		ld->amount
	 );
}

static int delete_csv_record(int linetodelete)
{
	if (linetodelete == 0) {
		puts("Cannot delete line 0");
		return -1;
	}
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_record_file(tmpfptr, fptr);
	return 0;
}

/* An old CLI function */
static void add_transaction(void)
{
	struct transaction_tokens userlinedata_, *uld = &userlinedata_;
	FILE *fptr = open_record_csv("r");
	struct vec_d *pidx = index_csv(fptr);
	fclose(fptr);

	uld->year = input_year();
	uld->month = input_month();
	uld->day = input_day(uld->month, uld->year);
	uld->category = input_category(uld->month, uld->year);
	if (uld->category == NULL) {
		goto err_category;
	}
	uld->desc = input_str_retry("Description:");
	uld->transtype = input_transaction_type();
	uld->amount = input_amount();

	puts("Verify Data is Correct:");
	print_record_vert(uld);
	printf("Y/N:  ");

	int result = confirm_input();
	if (result != 1) {
		goto err_confirm;
	}

	unsigned int resultline = sort_record_csv(uld->month, uld->day, uld->year);

	if (debug_flag) {
		printf("Result line: %d\n", resultline);
	}

	insert_transaction_record(resultline, uld);

err_confirm:
	free(uld->category);
	free(uld->desc);
err_category:
	free(pidx);
}

static int edit_csv_record
(int replace_line, struct transaction_tokens *ld, int field)
{
	if (replace_line == 0) {
		puts("Cannot delete line 0");
		return -1;
	}

	replace_line += 1;

	char replace_str[LINE_BUFFER];
	FILE *fptr;
	FILE *tmpfptr;

	switch (field) {

	case 1:
		ld->year = input_year();
		ld->month = input_month();
		ld->day = input_day(ld->month, ld->year);

		/* Have to add and delete here because the record will be placed
		 * in a new position when the date changes */
		fptr = open_record_csv("r");
		tmpfptr = delete_in_file(fptr, replace_line);
		mv_tmp_to_record_file(tmpfptr, fptr);
		insert_transaction_record(sort_record_csv(ld->month, ld->day, ld->year), ld);
		return 0;

	case 2:
		ld->category = input_category(ld->month, ld->year);
		break;

	case 3:
		ld->desc = input_str_retry("Enter Description");	
		break;

	case 4:
		ld->transtype = input_transaction_type();
		break;

	case 5:
		ld->amount = input_amount();
		break;

	default:
		puts("Not a valid choice, exiting");
		return -1;
	}

	fptr = open_record_csv("r");
	line_data_to_string(replace_str, sizeof(replace_str), ld);
	tmpfptr = replace_in_file(fptr, replace_str, replace_line);

	/* mv_tmp_to_record_file() closes the file pointers */
	mv_tmp_to_record_file(tmpfptr, fptr);

	if (field == 2) free(ld->category);
	if (field == 3) free(ld->desc);
	return 0;
}

/* Prints a 2 bar graphs showing the difference between income and expense */
static void print_bar_graph(double expense, double income)
{
	char income_bar[10];
	char expense_bar[10];

	for (size_t i = 0; i < sizeof(income_bar); i++) {
		income_bar[i] = '#';
		expense_bar[i] = '#';
	}

	if (income > expense) {
		double diff = expense / income;
		diff *= 10;
		for (size_t i = 0; i < sizeof(expense_bar); i++) {
			i < diff ? 
			(expense_bar[i] = '#') : (expense_bar[i] = '-');
		}
	} else {
		double diff = income / expense;
		diff *= 10;
		for (size_t i = 0; i < sizeof(income_bar); i++) {
			i < diff ? 
			(income_bar[i] = '#') : (income_bar[i] = '-');
		}
	}

	printf("Income:  $%.2f [", income);
	for (size_t i = 0; i < sizeof(income_bar); i++) {
		printf("%c", income_bar[i]);
	}
	printf("]\n");

	printf("Expense: $%.2f [", expense);
	for (size_t i = 0; i < sizeof(expense_bar); i++) {
		printf("%c", expense_bar[i]);
	}
	printf("]\n");

	printf("Total:   $%.2f\n", income - expense);
}

static void cli_read_csv(void)
{
	int useryear;
	int usermonth;
	double income = 0;
	double expense = 0;
	int linenum = 0;
	char linebuff[LINE_BUFFER] = {0};
	FILE *fptr = open_record_csv("r");
	bool month_record_exists = false;
	bool year_record_exists = false;

	struct transaction_tokens linedata_, *ld = &linedata_;

	struct vec_d *years = get_years_with_data(fptr, 2);
	rewind(fptr);

	while (year_record_exists == false) {
		useryear = input_year();
		for (size_t i = 0; i < years->size; i++) {
			if (years->data[i] == useryear) {
				year_record_exists = true;
				break;
			}
		}

		if (year_record_exists == false) {
			printf("%s '%d'\n", "No records match the year", useryear);
		}
	}

	free(years);
	years = NULL;
	rewind(fptr);

	struct vec_d *months = get_months_with_data(fptr, useryear, 1);

	while (month_record_exists == false) {
		usermonth = input_month();
		for (size_t i = 0; i < months->size; i++) {
			if (months->data[i] == usermonth) {
				month_record_exists = true;
				break;
			}
		}

		if (month_record_exists == false) {
			printf("%s '%d'\n", "No records match the month", usermonth);
		}
	}

	free(months);
	months = NULL;
	rewind(fptr);

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	char *line;
	while (1) {
		linenum++;
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) {
			break;
		}
		tokenize_record(ld, &line);
		ld->linenum = linenum;
		if (ld->month == usermonth && ld->year == useryear) {
			month_record_exists = true;
			print_record_hr(ld);
			if(ld->transtype == 1) {
				income+=ld->amount;
			} else if (ld->transtype == 0) {
				expense+=ld->amount;
			}
		}
		free_tokenized_record_strings(ld);
	}

	if (month_record_exists) {
		print_bar_graph(expense, income);
	} else {
		printf("No records match the entered date\n");
	}
	fclose(fptr);
	fptr = NULL;
}

static void cli_edit_transaction(void)
{
	int target;
	int humantarget;
	struct transaction_tokens linedata, *ld = &linedata;

	FILE *fptr = open_record_csv("r+");
	assert(ftell(fptr) == 0);

	cli_read_csv();
	
	struct vec_d *pidx = index_csv(fptr);

	assert(pidx->size < INT_MAX);

	do {
		puts("Enter line number");
		humantarget = input_n_digits(sizeof(size_t) + 1, 2);
	} while (humantarget <= 0 || humantarget > (int)pidx->size);

	target = humantarget - 1;

	if (debug_flag) {
		printf("TARGET: %d\n", target);
		printf("TARGET OFFSET: %ld\n", pidx->data[target]);
	}

	fseek(fptr, pidx->data[target], SEEK_SET);

	if (debug_flag) {
		printf("COMMANDED SEEK OFFSET: %ld\n", pidx->data[target]);
	}
	
	char linebuff[LINE_BUFFER];
	char *str = fgets(linebuff, sizeof(linebuff), fptr);
	if (str == NULL) {
		puts("failed to read line");
		exit(1);
	}

	tokenize_record(ld, &str);

	struct transaction_tokens *pLd = malloc(sizeof(*ld));
	if (pLd == NULL) {
		free(pidx);
		fclose(fptr);
		fptr = NULL;
		mem_alloc_fail();
	}

	memcpy(pLd, ld, sizeof(*ld));

	print_record_vert(ld);
	
	int fieldtoedit;
	do {
		puts("Enter field to edit or press \"0\" to delete this transaction");
		fieldtoedit = input_n_digits(1, 1); // Only input 1 digit
	} while (fieldtoedit > 5 || fieldtoedit < 0);

	switch (fieldtoedit) {

	case 0:
		if (delete_csv_record(humantarget) == 0) {
			puts("Successfully Deleted Transaction");
		}
		break;

	case 1:
		edit_csv_record(humantarget, pLd, 1);
		break;

	case 2:
		edit_csv_record(humantarget, pLd, 2);
		break;

	case 3:
		edit_csv_record(humantarget, pLd, 3);
		break;

	case 4:
		edit_csv_record(humantarget, pLd, 4);
		break;

	case 5:
		edit_csv_record(humantarget, pLd, 5);
		break;
	default:
		return;
	}

	free_tokenized_record_strings(pLd);
	free(pLd);
	pLd = NULL;
	free(pidx);
	pidx = NULL;
	fclose(fptr);
	fptr = NULL;
}

void cli_main_menu(void)
{
	int choice;
	printf("Make a selection:\n");
	printf("a - Add Transaction\n");
	printf("e - Edit Transaction\n"); 
	printf("r - Read CSV\n");
	printf("s - DEBUG Sort CSV\n");
	printf("q - Quit\n");

	char *userstr = user_input(STDIN_SMALL_BUFF); // Must be free'd
	
	while (userstr == NULL) {
		cli_main_menu();
	}

	if ((choice = upper(userstr)) == 0) {
		puts("Invalid character");
		free(userstr);
		userstr = NULL;
		cli_main_menu();
	}
	
	free(userstr);
	userstr = NULL;

	switch (choice) {

	case ('A'):
		printf("-*-ADD TRANSACTION-*-\n");
		add_transaction();
		break;

	case ('E'):
		printf("-*-EDIT TRANSACTION-*-\n");
		cli_edit_transaction();
		break;

	case ('R'):
		printf("-*-READ CSV-*-\n");
		cli_read_csv();
		break;

	case ('Q'):
		printf("Quiting\n");
		exit(0);
	default:
		puts("Invalid character");
		printf("\n");
		cli_main_menu();
	}
	return;
}
