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

#ifndef READ_LOOPS_H
#define READ_LOOPS_H

#include "read_init.h"
#include "tui.h"
#include "vector.h"
#include "categories.h"

void nc_read_loop(struct ReadWins *wins, 
				  FILE *fptr, 
				  struct record_select *sr, 
				  struct vec_d *psc,
				  struct catg_node *head);

void nc_read_budget_loop(struct ReadWins *wins,
						 FILE *rfptr,
						 FILE *bfptr,
						 struct record_select *sr,
						 struct vec_d *psc,
						 struct catg_node *head);

#endif
