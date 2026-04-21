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

#ifndef FILEMANAGEMENT_H
#define FILEMANAGEMENT_H

#define RECORD_DIR "./data.csv"
#define RECORD_BAK_DIR "./data.csv.bak"
#define TEMP_FILE_DIR "./tmp"
#define CONVERTED_FILE_DIR "./converted.csv"
#define BUDGET_DIR "./budget.csv"
#define BUDGET_BAK_DIR "./budget.csv.bak"

#include <stdio.h>

FILE *open_record_csv(char *mode);
FILE *open_budget_csv(char *mode);
FILE *open_temp_csv(void);
int mv_tmp_to_budget_file(FILE *tmp, FILE* main);
int mv_tmp_to_record_file(FILE *tmp, FILE* main);

#endif
