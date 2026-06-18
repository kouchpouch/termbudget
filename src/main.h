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

#ifndef MAIN_H
#define MAIN_H

#define REALLOC_INCR 64
#define INDEX_ALLOC 1024
#define MAX_ALLOC 1024 * 1024

#define CSV_FIELDS 7
#define LINE_BUFFER 256
#define STDIN_LARGE_BUFF 64
#define STDIN_SMALL_BUFF 8

#define MIN_YEAR 2000
#define MAX_YEAR 2100
#define MONTHS_IN_YEAR 12

#define MAX_LEN_AMOUNT 9
#define MAX_LEN_DAY_MON 2
#define MIN_LEN_DAY_MON 1
#define MAX_LEN_YEAR 4
#define MIN_LEN_YEAR 4
#define MIN_INPUT_CHAR 2

#define PAGE_KEY_ROWS 10

#define ESCAPE_ASCII 27

/* The word 'september' is the longest month by character count */
#define LONGEST_LEN_MONTH "september"

#define INPUT_IS_QUIT(x) \
	((x) == 'q' || (x) == 'Q' || (x) == KEY_F(QUIT) || \
	(x) == ESCAPE_ASCII) ? 1 : 0 \

#define INPUT_IS_ENTER(x) \
	((x) == '\n' || (x) == '\r') ? 1 : 0 \

#define CASE_QUIT \
	case ESCAPE_ASCII: \
	case KEY_F(QUIT): \
	case 'Q': \
	case 'q': \

#define CASE_ENTER \
	case ('\n'): \
	case ('\r'): \
	case KEY_ENTER: \

/* Bitwise Stuff */
#define SET_KEEP_BIT(x) (x) |= 0x80
#define UNSET_KEEP_BIT(x) (x) &= 0x7F
#define KEEP_BIT_IS_SET(x) ((x) & 0x80) == 0x80 ? 1 : 0

extern const char *abbr_months[];
extern const char *fullname_months[];

enum read_return_flags {
	RRET_DEFAULT,
	RRET_BYDATE,
	RRET_QUIT
};

enum sort_by {
	SORT_DATE = 0,
	SORT_CATG = 1
};

/* Forward declaration for data structures in vector.h */
struct vec_d;
struct vec2f_fin;

/* Exits the program with "exit(1)" and prints the error message. */
void mem_alloc_fail(void);

/* Sets the values of struct members 'pb' using records in RECORD_DIR
 * at file positions of 'pbo' */
void calculate_balance(struct vec2f_fin *pb, struct vec_d *pbo);

int show_help_subwindow(void);

#endif
