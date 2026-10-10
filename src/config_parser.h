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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdio.h>

#define CONFIG_BUFFER_SZ 256
#define COMMENT_PREFIX '#'

#ifdef HAS_NCURSES
#include <ncurses.h>
#endif

enum slice_get_which {
	KEY,
	VALUE
};

enum parse_error {
	PARSE_EOF,
	PARSE_BLANK,
	PARSE_OK
};

enum keys_config {
	KEY_UNKNOWN = -1,
	KEY_COLOR0,
	KEY_COLOR1,
	KEY_COLOR2,
	KEY_COLOR3,
	KEY_COLOR4,
	KEY_COLOR5,
	KEY_COLOR6,
	KEY_COLOR7,
	KEY_COLOR8,
	KEY_COLOR9
};

enum keys_type {
	KEY_TYPE_UNKNOWN,
	KEY_TYPE_BOOLEAN,
	KEY_TYPE_STRING,
	KEY_TYPE_INTEGER,
	KEY_TYPE_COLOR,
};

struct string_slice {
	char *s;
	int len;
};

struct key_type_pair {
	char *key;
	enum keys_type t;
};

struct key_value_pair {
	struct string_slice key;
	struct string_slice value;
};

void slice_print(const struct string_slice slice);

#ifdef HAS_NCURSES
void slice_print_window(WINDOW *wptr,
						int y,
						int x,
						const struct string_slice slice);
#endif

enum keys_config parse_config_enumerate_key(const struct string_slice key);

enum keys_type parse_config_enumerate_key_type(const struct string_slice key);

enum parse_error parse_config(FILE *conf,
							  struct string_slice *key,
							  struct string_slice *value,
							  char *buff,
							  size_t buff_sz);

#endif
