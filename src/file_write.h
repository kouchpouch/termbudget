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

#ifndef FILE_WRITE_H
#define FILE_WRITE_H

#include "parser.h"
#include <stdio.h>
#include <stdbool.h>

int budget_tokens_to_string(char *buffer, size_t size, struct budget_tokens *bt);
int line_data_to_string(char *buffer, size_t size, struct transaction_tokens *ld);
FILE *delete_in_file(FILE *fptr, int delete_line);
FILE *replace_in_file(FILE *fptr, char *replace_str, int replace_line);
FILE *insert_into_file(FILE *fptr, char *insert_str, int insert_line);

#endif
