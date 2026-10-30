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

#ifndef TUI_SIDEBAR_H
#define TUI_SIDEBAR_H

#define SIDEBAR_COLUMNS 40
#define MIN_COLUMNS_SIDEBAR MIN_COLUMNS + SIDEBAR_COLUMNS

#include <ncurses.h>

#include "categories.h"
#include "tui.h"
#include "vector.h"

struct sidebar_deque {
	struct sidebar_catg_data *data;
	WINDOW *wptr;
	size_t start;
	size_t end;
	size_t len;
	size_t begin_node;
};

int create_sidebar_windows(struct read_wins *wins);

/* Draws in all of the data for the sidebar body inside of window 'wptr' using
 * data from 'nodes', beginning at node 'i'. */
int init_sidebar_body(struct sidebar_deque *deque);

void draw_parent_box_with_sidebar(WINDOW *wptr);

struct sidebar_deque sidebar_create(struct read_wins *wins,
									struct vec_d *records,
									struct catg_node *head);

#endif
