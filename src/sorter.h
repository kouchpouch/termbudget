/*
 * Includes functions which find the appropriate line to insert a record
 * into a CSV with the first three fields of every record being
 * "month,day,year".
 *
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
#ifndef SORTER_H
#define SORTER_H

/* Returns total lines in the CSV */
extern int get_total_csv_lines(void);

/* Returns the line sorted by date to insert data for budget.csv */
extern unsigned int sort_budget_csv(int month, int year);

/* A new version of sort_csv() with less chaos */
extern unsigned int sort_record_csv(int month, int day, int year);

/* Returns a the line in the CSV to insert the data sorted by date,
 * assumes the CSV is already sorted by date */
[[deprecated]]extern int sort_csv(int month, int day, int year); 

#endif
