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

#ifndef EDIT_CATEGORIES_H
#define EDIT_CATEGORIES_H

#include "main.h"
#include "categories.h"

/* Moves the category found at FPI nodes[i]->catg_fp to the top of its
 * siblings with the same date range. */
extern void mv_category_to_top(CategoryNode **nodes, size_t i);

/* Allows the user to edit the category at file position b. */
extern void nc_edit_category(long b, long nmembers, CategoryNode **nodes);

#endif
