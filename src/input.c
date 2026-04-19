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
#include <ctype.h>
#include <string.h>

#include "parser.h"
#include "categories.h"
#include "main.h"
#include "helper.h"
#include "flags.h"

char *user_input(int n)
{
	size_t buffersize = n + 1;
	char *buffer = (char *)malloc(buffersize);

	if (buffer == NULL) {
		mem_alloc_fail();
	}
	if (fgets(buffer, buffersize, stdin) == NULL) {
		printf("Invalid Input\n");
		goto err_fail;
	}

	int length = strnlen(buffer, buffersize);

	if (buffer[length - 1] != '\n' && buffer[length - 1] != '\0') {
		printf("Input is too long, try again\n");
		int c = 0;
		while (c != '\n') {
			c = getchar();
		}
		goto err_fail;
	}

	if (length < MIN_INPUT_CHAR) {
		puts("Input is too short");
		goto err_fail;
	}

	if (strstr(buffer, ",")) {
		puts("No commas allowed, we're using a CSV, after all!");
		goto err_fail;
	}

	return buffer; // Must be free'd

err_fail:
	free(buffer);
	buffer = NULL;
	return buffer;
}

int input_n_digits(size_t max_len, size_t min_len)
{
	char *str = user_input(max_len + 1);

	while (str == NULL) {
		str = user_input(max_len + 1);
	}

	if (strlen(str) <= min_len) {
		puts("Input is too short");
		free(str);
		str = NULL;
		return -1;
	}

	for (size_t i = 0; i < strlen(str); i++) {
		if (!isdigit(*(str + i)) && *(str + i) != '\n') {
			printf("Invalid character \"%c\", must be digit\n", *(str + i));
			free(str);
			str = NULL;
			return -1;
		}
	}

	int digits = atoi(str);
	free(str);
	str = NULL;

	return digits;
}

int confirm_input(void)
{
	char *confirm = user_input(STDIN_SMALL_BUFF);
	if (confirm == NULL) {
		return -1;
	}

	char c_confirm = (char)upper(confirm);
	free(confirm);
	confirm = NULL;

	if (c_confirm == 0) {
		return -1;
	}
	if (c_confirm == 'Y') {
		return 1;
	} else if (c_confirm == 'N') {
		return 0;
	}
	return -1;
}

int input_month(void)
{
	int month;
	puts("Enter Month");
	while ((month = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1
		|| month <= 0
		|| month > 12) {
		puts("Enter a Vaid Month");
	}
	return month;
}

int input_year(void)
{
	int year;
	if (cli_flag) {
		puts("Enter Year");
	}
	while ((year = input_n_digits(MAX_LEN_YEAR, MIN_LEN_YEAR)) == -1);
	return year;
}

int input_day(int month, int year)
{
	int day;
	puts("Enter Day");

	while ((day = input_n_digits(MAX_LEN_DAY_MON, MIN_LEN_DAY_MON)) == -1 ||
			dayexists(day, month, year) == false) {
		if (dayexists(day, month, year) == false) { 
			puts("Invalid Day");
		}
	}
	return day;
}

int input_transaction_type(void)
{
	int t;

	/* 0 is an expense and 1 is income in the CSV */
	puts("Enter 1 or 2");
	puts("1. Expense");
	puts("2. Income");

	while ((t = input_n_digits(1, 1)) == -1 || (t != 1 && t != 2)) {
		puts("Invalid");
	}
	return t - 1; // sub 1 to convert human readable to CSV format
}

double input_amount(void)
{
	puts("$ Amount:");
	char *str = user_input(MAX_LEN_AMOUNT);
	while (str == NULL) {
		puts("Invalid");
		str = user_input(MAX_LEN_AMOUNT);
	}
	double amount = atof(str);
	free(str);
	str = NULL;
	return amount;
}

/* 
 * Takes a user's input and displays msg, on failure to read the user's
 * input the user's input is read again. The newline character is removed 
 */
char *input_str_retry(char *msg)
{
	puts(msg);
	int len;
	char *str = user_input(STDIN_LARGE_BUFF);	
	while (str == NULL) {
		str = user_input(STDIN_LARGE_BUFF);
	}
	len = strlen(str);
	if (str[len - 1] == '\n') {
		str[len - 1] = 0;
	}
	return str;
}

/* Returns a malloc'd char * */
char *input_category(int month, int year)
{
	char *str;
	bool cat_exists = false;
	_category_vec_t *pc = get_categories(month, year);

	if (pc->size > 0) {
		puts("Categories:");
		for (size_t i = 0; i < pc->size; i++) {
			printf("%s ", pc->categories[i]);
		}
		printf("\n");
	} else {
		puts("No categories exist for this month");
	}

retry_input:
	cat_exists = false;
	str = input_str_retry("Enter Category:");

	for (size_t i = 0; i < pc->size; i++) {
		if (strcasecmp(str, pc->categories[i]) == 0) {
			cat_exists = true;
			break;
		}
	}

	if (cat_exists != true) {
		puts("That category doesn't exist for this month, create it? [y/n]");
		if (confirm_input() != 1) {
			free(str);
			str = NULL;
			goto retry_input;
		}
	}

	free_categories(pc);
	return str;
}
