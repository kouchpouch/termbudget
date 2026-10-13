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
 * Copyright (c) 2026 termbudget
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_parser.h"
#include "file_management.h"
#include "flags.h"
#include "helper.h"

#ifdef USING_NCURSES
#include <ncurses.h>
#endif // USING_NCURSES

struct key_type_pair key_type_pair[] = {
	{"color0", KEY_TYPE_COLOR},
	{"color1", KEY_TYPE_COLOR},
	{"color2", KEY_TYPE_COLOR},
	{"color3", KEY_TYPE_COLOR},
	{"color4", KEY_TYPE_COLOR},
	{"color5", KEY_TYPE_COLOR},
	{"color6", KEY_TYPE_COLOR},
	{"color7", KEY_TYPE_COLOR},
	{"color8", KEY_TYPE_COLOR},
	{"color9", KEY_TYPE_COLOR},
	{"config_dir", KEY_TYPE_STRING},
	{"data_dir", KEY_TYPE_STRING},
};

inline static char char_to_lower(char c)
{
	if (c >= 65 && c <= 90) {
		c += 32;
	}
	
	return c;
}

/* Ignores case */
static bool slice_eq_to_string(const struct string_slice slice,
							   const char *string)
{
	char slice_char  = 0;
	char string_char = 0;

	if (slice.len != strlen(string)) {
		return false;
	}
	
	for (size_t i = 0; i < slice.len; i++) {
		slice_char  = slice.s[i];
		string_char = string[i];

		slice_char = char_to_lower(slice_char);
		string_char = char_to_lower(string_char);

		if (slice_char != string_char) {
			return false;
		}
	}

	return true;
}

enum keys_config parse_config_enumerate_key(const struct string_slice key)
{
	enum keys_config ret = KEY_UNKNOWN;
	size_t n_keys = sizeof(key_type_pair) / sizeof(struct key_type_pair);

	for (size_t i = 0; i < n_keys; i++) {
		if (slice_eq_to_string(key, key_type_pair[i].key)) {
			ret = i;
		}
	}

	return ret;
}

enum keys_type parse_config_enumerate_key_type(const struct string_slice key)
{
	enum keys_type ret = KEY_TYPE_UNKNOWN;
	size_t n_keys = sizeof(key_type_pair) / sizeof(struct key_type_pair);

	for (size_t i = 0; i < n_keys; i++) {
		if (slice_eq_to_string(key, key_type_pair[i].key)) {
			ret = key_type_pair[i].t;
		}
	}

	return ret;
}

/* Returns true if the string 's' starts with the macro COMMENT_PREFIX.
 * Whitespace preceding the string is ignored. */
inline static bool is_comment(char *s)
{
	size_t i;
	size_t len = strlen(s);

	for (i = 0; i < len; i++) {
		if (isspace(s[i])) {
			continue;
		}
		break;
	}

	if (s[i] == COMMENT_PREFIX) {
		return true;
	}

	return false;
}

/* Returns true if the string slice has a length greater than 0 */
inline static bool slice_is_nonzero(const struct string_slice slice)
{
	return (slice.len > 0 ? true : false);
}

/* Trims whitespace preceding any text in string slice 'slice' by setting
 * a new memory address for the string pointer and creating a new len value */
static struct string_slice slice_trim_front(const struct string_slice slice)
{
	struct string_slice trimmed = slice;
	size_t trim_len = 0;

	if (! slice_is_nonzero(slice) || ! isspace(slice.s[0])) {
		return trimmed;
	}

	for (size_t i = 0; i < slice.len; i++) {
		if (isspace(slice.s[i])) {
			trim_len++;
		} else {
			break;
		}
	}

	trimmed.s 	+= trim_len;
	trimmed.len -= trim_len;

	return trimmed;
}

/* Trims whitespace from the end (higher index) of string slice 'slice' by
 * creating a new len value. */
static struct string_slice slice_trim_reverse(const struct string_slice slice)
{
	struct string_slice trimmed = slice;
	int trim_len = 0;

	if (! slice_is_nonzero(slice) || ! isspace(slice.s[slice.len - 1])) {
		return trimmed;
	}

	for (int i = slice.len; i > 0; i--) {
		if (isspace(slice.s[i - 1])) {
			trim_len++;
		} else {
			break;
		}
	}

	trimmed.len -= trim_len;

	return trimmed;
}

/* Trims inline comments. Comments start with the COMMENT_PREFIX symbol and
 * must be preceded by a space.
 * EXAMPLE:
 * key=value # This is a comment
 * key=value# This is not a comment
 */
static struct string_slice slice_trim_inline_comment
(const struct string_slice slice)
{
	struct string_slice trimmed = slice;

	for (size_t i = 0; i < slice.len; i++) {
		if (slice.s[i] == COMMENT_PREFIX) {
			if (isspace(slice.s[i - 1])) {
				trimmed.len = i;
				break;
			}
		}
	}

	return trimmed;
}

/* Trim whitespace from the front and back of the string slice 'slice' */
static struct string_slice slice_trim_all(const struct string_slice slice)
{
	struct string_slice trimmed = slice;
	trimmed = slice_trim_inline_comment(trimmed);
	trimmed = slice_trim_front(trimmed);
	trimmed = slice_trim_reverse(trimmed);
	return trimmed;
}

/* Return a string slice that contains the string which follows 'delim' */
static struct string_slice slice_get_value(const struct string_slice slice,
										   const char delim)
{
	struct string_slice value = slice;
	size_t i;

	for (i = 0; i < value.len; i++) {
		if (value.s[i] == delim) {
			break;
		}
	}

	if (i + 1 <= value.len) {
		i++;
	}

	assert(i <= value.len);

	value.s   += i;
	value.len -= i;

	return value;
}

/* Return a string slice that contains the string which precedes 'delim' */
static struct string_slice slice_get_key(const struct string_slice slice,
										 const char delim)
{
	struct string_slice key = slice;

	for (size_t i = 0; i < key.len; i++) {
		if (key.s[i] == delim) {
			key.len = i;
			return key;
		}
	}

	return key;
}

static struct string_slice slice_get_trimmed(const struct string_slice slice,
											 enum slice_get_which which)
{
	struct string_slice ret = slice;

	switch (which) {
		case KEY:
			ret = slice_get_key(ret, '=');
			ret = slice_trim_all(ret);
			break;
		case VALUE:
			ret = slice_get_value(ret, '=');
			ret = slice_trim_all(ret);
			break;
	}

	return ret;
}

/* Creates a string slice from a char pointer */
static struct string_slice slice_create(char *s)
{
	struct string_slice slice = {
		.s   = s,
		.len = strlen(s),
	};

	return slice;
}

/* Prints the string slice inside of [brackets] to view any whitespace,
 * use for debugging */
void slice_print(const struct string_slice slice)
{
	int n = size_to_int(slice.len);
	printf("[%.*s]\n", n, slice.s);
}

#ifdef USING_NCURSES
void slice_print_window(WINDOW *wptr,
						int y,
						int x,
						const struct string_slice slice)
{
	int n = size_to_int(slice.len);
	mvwprintw(wptr, y, x, "[%.*s]", n, slice.s);
}
#endif // USING_NCURSES

/* Parses one line of the config file, subsequent calls will parse the
 * following line, returns PARSE_EOF at the end of the file */
enum parse_error parse_config(FILE *conf,
							  struct string_slice *key,
							  struct string_slice *value,
							  char *buff,
							  size_t buff_sz)
{
	struct string_slice slice;
	char *str;

	str = fgets(buff, buff_sz, conf);
	if (str == NULL) {
		key->len = 0;
		value->len = 0;
		return PARSE_EOF;
	} 

	if (is_comment(str) || str[0] == '\n') {
		key->len = 0;
		value->len = 0;
		return PARSE_BLANK;
	}

	slice = slice_create(str);
	*key = slice_get_trimmed(slice, KEY);
	*value = slice_get_trimmed(slice, VALUE);
#ifdef USING_NCURSES
	if (debug_flag) {
		wclear(stdscr);
		slice_print_window(stdscr, 1, 0, *key);
		slice_print_window(stdscr, 1, key->len + 3, *value);
		refresh();
		getch();
	}
#else
	if (debug_flag) {
		slice_print(*key);
		slice_print(*value);
	}
#endif // USING_NCURSES
	return PARSE_OK;
}

static bool value_is_valid(enum keys_type type, struct key_value_pair pair)
{
	switch (type) {
		case KEY_TYPE_UNKNOWN:
			break;
		case KEY_TYPE_BOOLEAN:
			break;
		case KEY_TYPE_STRING:
			break;
		case KEY_TYPE_INTEGER:
			break;
		case KEY_TYPE_COLOR:
			if (pair.value.len <= 7 && pair.value.len >= 6) {
				return true;
			} else {
				return false;
			}
	}

	return false;
}

int get_colors_from_config(void)
{
	FILE *conf = open_config_file("r");
	struct key_value_pair pair;
	char buff[CONFIG_BUFFER_SZ];
	enum parse_error e;
	enum keys_config key_enum;
	enum keys_type key_type;

	set_default_colors();

	while (1) {
		e = parse_config(conf, &pair.key, &pair.value, buff, sizeof(buff));
		switch (e) {
			case PARSE_OK:
				key_enum = parse_config_enumerate_key(pair.key);
				key_type = parse_config_enumerate_key_type(pair.key);
				switch (key_type) {
					case KEY_TYPE_UNKNOWN:
					case KEY_TYPE_BOOLEAN:
					case KEY_TYPE_STRING:
					case KEY_TYPE_INTEGER:
						break;
					case KEY_TYPE_COLOR:
						if (value_is_valid(key_type, pair)) {
							set_color(key_enum, pair);
						}
						break;
					default:
						break;
				}
				break;
			case PARSE_BLANK:
				break;
			case PARSE_EOF:
				fclose(conf);
				return EOF;
		}
	}

	return 0;
}
