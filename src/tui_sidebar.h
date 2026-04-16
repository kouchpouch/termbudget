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

#ifndef TUI_SIDEBAR_H
#define TUI_SIDEBAR_H

#define SIDEBAR_COLUMNS 40
#define MIN_COLUMNS_SIDEBAR MIN_COLUMNS + SIDEBAR_COLUMNS

#include <ncurses.h>
#include "tui.h"
#include "categories.h"
#include "vector.h"

extern void draw_body_border(WINDOW *wptr);

/* Creates the sidebar parent window based on dimensions of stdscr provided
 * by arguments std_y and std_x. Returns a WINDOW * on success or calls
 * window_creation_fail() from tui.h */
extern WINDOW *create_sidebar_parent(WINDOW *wptr_parent, int std_y, int std_x);

/* Creates the sidebar body window based on dimensions of wptr_parent and
 * wptr_sidebar_parent. Returns a WINDOW * on success or calls
 * window_creation_fail() from tui.h */
extern WINDOW *create_sidebar_body(WINDOW *wptr_parent, WINDOW *wptr_sidebar_parent);

/* Draws in all of the data for the sidebar body inside of window 'wptr' using
 * data from 'nodes', beginning at node 'i'. */
extern int init_sidebar_body(WINDOW *wptr, CategoryNode **nodes, size_t i);

/* Draws a box around the sidebar parent. Pass in the WINDOW * which points to 
 * the sidebar parent. Also draws the appropriate text on the boundary */
extern void draw_sidebar_parent_border(WINDOW *wptr);

extern void init_sidebar_parent(WINDOW *wptr, _vector_t *psc, double leftover);

#endif
