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

#ifndef SORTER_H
#define SORTER_H

#include <stdio.h>

/* Returns the line sorted by date to insert data for budget.csv */
unsigned int sort_budget_csv(int month, int year);

/* Returns the FIRST resultant line of the CSV containing the month, day, and
 * year arguments. Or the line where data matching the date arguments should
 * be placed. */
unsigned int sort_record_csv(int month, int day, int year);

/* Same behavior as sort_record_csv(), however the return value is the
 * line FOLLOWING the last matching record by date. I.E. returns the line
 * sorted for the purpose of appending data. */
unsigned int sort_record_csv_append(int month, int day, int year);

unsigned int sort_converted_csv(int month, int day, int year, FILE *fptr);

#endif
