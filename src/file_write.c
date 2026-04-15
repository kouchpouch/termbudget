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

#include <assert.h>
#include <stdio.h>

#include "main.h"
#include "filemanagement.h"
#include "flags.h"

typedef struct __file_write_t {
	char *string;
	int line;
	bool replace;
	bool delete;
} _file_write_t;

static void print_lines_written_debug(int lines_written)
{
	if (debug_flag) {
		printw("Lines written: %d\n", lines_written);
		getch();
	};
}

int budget_tokens_to_string
(char *buffer, size_t size, struct BudgetTokens *bt)
{
	return snprintf(buffer, size, 
		"%d,%d,%s,%d,%.2f\n", 
		bt->m, 
		bt->y, 
		bt->catg, 
		bt->transtype,
		bt->amount);
}

int line_data_to_string 
(char *buffer, size_t size, struct LineData *ld)
{
	return snprintf(buffer, size,
		  "%d,%d,%d,%s,%s,%d,%.2f\n",
		  ld->month, 
		  ld->day, 
		  ld->year, 
		  ld->category, 
		  ld->desc, 
		  ld->transtype, 
		  ld->amount);
}

/*
 * If the string "write_str" is NULL and delete is false OR vice/versa, return
 * false.
 */
static bool assert_null_check_when_deleting
(char *write_str, bool delete)
{
	if (write_str == NULL) {
		if (!delete) {
			return false;
		} else {
			return true;
		}
	} else {
		if (delete) {
			return false;
		} else {
			return true;
		}
	}
}

static void print_invalid_opt_error_and_exit(void)
{
	puts("Cannot write a NULL string");
	puts("OR");
	puts("Attempting to delete a line from file");
	puts("whilst providing a string to write");
	exit(1);
}

/* 
 * Writes "write_str" to "tmpfptr" on line "write_line", the contents of
 * "fptr" will be written to "tmpfptr" as well.
 *
 * If "replace" is true, the contents at "write_line" will be replaced with
 * "write_str".
 *
 * If "delete" is true:
 * 		-The contents at "write_line" will be deleted.
 * 		-"write_str" must be NULL.
 *
 * If "delete" is false:
 * 		-"write_str" must NOT be NULL.
 *
 * Returns number of lines written.
 */
static void write_string_to_file
(FILE *fptr, FILE* tmpfptr, _file_write_t *opts)
{
	char line_buffer[LINE_BUFFER];
	char *line_str;
	int current_line = 0;

	if (!assert_null_check_when_deleting(opts->string, opts->delete)) {
		print_invalid_opt_error_and_exit();
	}

	rewind(fptr);

	while ((line_str = fgets(line_buffer, sizeof(line_buffer), fptr)) != NULL) {
		current_line++;	

		if (current_line != opts->line) {
			fputs(line_str, tmpfptr);
		} else if (current_line == opts->line && opts->delete == false) {
			if (opts->replace == false) {
				fputs(line_str, tmpfptr);
			}
			fputs(opts->string, tmpfptr);
		}
	}

	print_lines_written_debug(current_line);
}

/* 
 * Returns a temporary FILE* with the all of the contents of "fptr" plus
 * "insert_str" written to line "insert_line"
 */
FILE *insert_into_file(FILE *fptr, char *insert_str, int insert_line)
{
	_file_write_t opts = {
		.string = insert_str,
		.line = insert_line,
		.replace = false,
		.delete = false
	};

	FILE *tmpfptr = open_temp_csv();
	write_string_to_file(fptr, tmpfptr, &opts);

	return tmpfptr;
}

/* 
 * Returns a temporary FILE* with the all of the contents of "fptr" except
 * "replace_str" overwrites the contents of "replace_line".
 */
FILE *replace_in_file(FILE *fptr, char *replace_str, int replace_line)
{
	_file_write_t opts = {
		.string = replace_str,
		.line = replace_line,
		.replace = true,
		.delete = false
	};

	FILE *tmpfptr = open_temp_csv();
	write_string_to_file(fptr, tmpfptr, &opts);

	return tmpfptr;
}

/* 
 * Returns a temporary FILE* with the all of the contents of "fptr" minus
 * the contents at "delete_line".
 */
FILE *delete_in_file(FILE *fptr, int delete_line)
{
	_file_write_t opts = {
		.string = NULL,
		.line = delete_line,
		.replace = false,
		.delete = true
	};

	FILE *tmpfptr = open_temp_csv();
	write_string_to_file(fptr, tmpfptr, &opts);

	return tmpfptr;
}
