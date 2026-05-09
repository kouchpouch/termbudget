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

#ifndef VECTOR_H
#define VECTOR_H
#include <stdio.h>

/* 2-float vector, financial */
struct vec2f_fin {
	double expense;
	double income;
};

/* 2-float vector */
struct vec2f {
	double a;
	double b;
};

/* 2-int vector */
struct vec2i {
	int a;
	int b;
};

/* Dynamic "std vector"/array of long */
struct vec_d {
	size_t capacity;
	size_t size;
	long data[];
};

/* Dynamic "std vector"/array of char * */
struct catg_vec {
	size_t capacity;
	size_t size;
	char *categories[];
};

struct vec_d *vec_d_create(void);

struct catg_vec *catg_vec_create(void);

void vec_d_append_many(struct vec_d **v, long *data, size_t data_len);

/* Appends "data" to the end of the v->data array and reallocs if required */
void catg_vec_append(struct catg_vec **v, char *data);

/* Appends "data" to the end of the v->data array and reallocs if required */
void vec_d_append(struct vec_d **v, long data);

#endif
