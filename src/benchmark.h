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

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <time.h>

extern long double benchmark_begin, benchmark_end;
extern struct timespec benchmark_tp;

#define BENCHMARK_START() \
	clock_gettime(CLOCK_MONOTONIC, &benchmark_tp); \
	benchmark_begin = (long double)(benchmark_tp.tv_sec + (benchmark_tp.tv_nsec * 1e-9)); \

#define BENCHMARK_END() \
	clock_gettime(CLOCK_MONOTONIC, &benchmark_tp); \
	benchmark_end = (long double)(benchmark_tp.tv_sec + (benchmark_tp.tv_nsec * 1e-9)); \

#define BENCHMARK_RESULT (benchmark_end - benchmark_begin)

#endif
