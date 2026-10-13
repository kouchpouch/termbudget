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
#include <ncurses.h>
#include <string.h>

#include "config_parser.h"

#define NC_COLOR_MULT 3.90625 /* ~ (1000 / 256) */

/* Custom colors start at this offset, as to not change default values. */
#define NC_COLOR_PAIR_OFFSET 11 

/* Returns 'h' * 16^exp */
static unsigned int hex_to_decimal(char h, unsigned int exp)
{
	/* To remain within the range of an int */
	assert(exp <= 8);
	int add_to = 16;

	if (h >= 'a' && h <= 'f') {
		h = (h - 'a');
		h += 10;
	} else if (h >= '0' && h <= '9') {
		h = (h - '0');
	} else {
		return 0;
	}

	if (exp == 0) {
		return h;
	}

	for (unsigned int i = 1; i < exp; i++) {
		add_to *= 16;
	}

	return h * add_to;
}

/* Initializes ncurses color pair 'pair' to the value of 'hex' */
static void color_init_hex(const short pair, const char *hex)
{
	assert(strlen(hex) >= 6UL);
	char buff[6] = { 0 };
	unsigned int i, r, g, b;
	r = b = g = 0;

	if (hex[0] == '#') {
		i = 1;
	} else {
		i = 0;
	}

	for (int j = 0; j < 6; j++, i++) {
		buff[j] = tolower(hex[i]);
	}

	r += hex_to_decimal(buff[1], 0U);
	r += hex_to_decimal(buff[0], 1U);

	g += hex_to_decimal(buff[3], 0U);
	g += hex_to_decimal(buff[2], 1U);

	b += hex_to_decimal(buff[5], 0U);
	b += hex_to_decimal(buff[4], 1U);

	/* Convert 2^8 color value to a 1000 RGB scale for ncurses */
	r *= NC_COLOR_MULT;
	g *= NC_COLOR_MULT;
	b *= NC_COLOR_MULT;

	init_color(pair, r, g, b);
	init_pair(pair, pair, -1);
}

static void color_init_hex_from_slice(const short pair,
									  const struct string_slice slice)
{
	assert(slice.len >= 6UL && slice.len <= 7UL);
	color_init_hex(pair, slice.s);
}

void set_default_colors(void)
{
	short color_pair = 0 + NC_COLOR_PAIR_OFFSET;
   
	/* A clanker picked these colors */
	color_init_hex(color_pair++, "#5fafff");
	color_init_hex(color_pair++, "#5f87ff");
	color_init_hex(color_pair++, "#87afff");
	color_init_hex(color_pair++, "#afafff");
	color_init_hex(color_pair++, "#87ff87");
	color_init_hex(color_pair++, "#5fd7af");
	color_init_hex(color_pair++, "#87af87");
	color_init_hex(color_pair++, "#ffaf87");
	color_init_hex(color_pair++, "#ffaf5f");
	color_init_hex(color_pair++, "#ffd878");
}

void set_color(enum keys_config key_enum, const struct key_value_pair key_val)
{
	short nc_pair = 0 + NC_COLOR_PAIR_OFFSET;
	switch (key_enum) {
		case KEY_UNKNOWN:
			break;
		case KEY_COLOR0:
		case KEY_COLOR1:
		case KEY_COLOR2:
		case KEY_COLOR3:
		case KEY_COLOR4:
		case KEY_COLOR5:
		case KEY_COLOR6:
		case KEY_COLOR7:
		case KEY_COLOR8:
		case KEY_COLOR9:
			color_init_hex_from_slice(key_enum + nc_pair, key_val.value);
			break;
		default:
			break;
	}
}
