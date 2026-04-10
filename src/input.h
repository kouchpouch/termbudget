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

extern char *user_input(int n);

extern int input_n_digits(size_t max_len, size_t min_len);

extern int confirm_input(void);

extern int input_month(void);

extern int input_year(void);

extern int input_day(int month, int year);

extern int input_transaction_type(void);

extern double input_amount(void);

extern char *input_str_retry(char *msg);

extern char *input_category(int month, int year);

#endif
