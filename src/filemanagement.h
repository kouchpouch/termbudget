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

#include <stdio.h>
#include "dynamic_string.h"

#ifdef __linux__
#include <linux/limits.h>
#elif !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#ifdef TB_RELATIVE_DIRS 
#define RECORD_DIR         "./data.csv"
#define RECORD_BAK_DIR     "./data.csv.bak"
#define TEMP_DIR           "./tmp"
#define CONVERTED_FILE_DIR "./converted.csv"
#define BUDGET_DIR         "./budget.csv"
#define BUDGET_BAK_DIR     "./budget.csv.bak"
#else

#define RECORD_FILE     "/data.csv"
#define RECORD_BAK_FILE "/data.csv.bak"
#define TEMP_FILE       "/tmp"
#define CONVERTED_FILE  "/converted.csv"
#define BUDGET_FILE     "/budget.csv"
#define BUDGET_BAK_FILE "/budget.csv.bak"

extern char program_dir        [PATH_MAX];
extern char record_dir         [PATH_MAX];
extern char record_bak_dir     [PATH_MAX];
extern char tmp_file_dir       [PATH_MAX];
extern char converted_file_dir [PATH_MAX];
extern char budget_dir         [PATH_MAX];
extern char budget_bak_dir     [PATH_MAX];

#endif

int create_program_directory(void);
FILE *open_record_csv(char *mode);
FILE *open_budget_csv(char *mode);
FILE *open_temp_csv(void);
int mv_tmp_to_budget_file(FILE *tmp, FILE* main);
int mv_tmp_to_record_file(FILE *tmp, FILE* main);

#endif
