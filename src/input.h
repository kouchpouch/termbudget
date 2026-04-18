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

#ifndef INPUT_H
#define INPUT_H

#include <stdio.h>

char *user_input(int n);

int input_n_digits(size_t max_len, size_t min_len);

int confirm_input(void);

int input_month(void);

int input_year(void);

int input_day(int month, int year);

int input_transaction_type(void);

double input_amount(void);

char *input_str_retry(char *msg);

char *input_category(int month, int year);

#endif
