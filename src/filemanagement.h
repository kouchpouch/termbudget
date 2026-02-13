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
#ifndef FILEMANAGEMENT_H
#define FILEMANAGEMENT_H

#define RECORD_DIR "./data.csv"
#define RECORD_BAK_DIR "./data.csv.bak"
#define TEMP_FILE_DIR "./tmp"
#define BUDGET_DIR "./budget.csv"
#define BUDGET_BAK_DIR "./budget.csv.bak"

#include <stdio.h>

/* Opens file located at RECORD_DIR in mode mode. Exits on failure */
extern FILE *open_record_csv(char *mode);

/* Opens file located at BUDGET_DIR in mode mode. Exits on failure */
extern FILE *open_budget_csv(char *mode);

/* Opens file located at TMP_DIR in w+ mode. Exits on failure */
extern FILE *open_temp_csv(void);

#endif
