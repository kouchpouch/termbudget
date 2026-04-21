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

/*
 * Verify data files that termBudget uses comply with the maximum lengths
 * defined in "main.h". Validate data between files as well, with RECORD_DIR
 * being the master.
 */
#ifndef FILEINTEGRITY_H
#define FILEINTEGRITY_H

#include <stdbool.h>

/* Returns true if all fields are initialized with a field value, false if
 * any field value is less than 0. */
bool validate_record_header(void);

/* Returns true if all fields are initialized with a field value, false if
 * any field value is less than 0. */
bool validate_budget_header(void);

/* Returns true if record csv fields are equal to or less than the maximum
 * lengths defined in main. False if not */
bool record_len_verification(void);

/*
 * Ensures that if a category exists in RECORD_DIR(main.h)
 * it will also exist in BUDGET_DIR(main.h). BUDGET_DIR is verified against
 * RECORD_DIR, not the other way around. If a category exists in BUDGET_DIR
 * and not RECORD_DIR leading to an orphaned category--this is not checked.
 *
 * Orphaned categories are expected and a normal part of the program that are
 * used for budget planning.
 *
 * Returns a 0 or positive value of records that were corrected successfully.
 * Returns -1 on failure.
 */
int verify_categories_exist_in_budget(void);

int verify_files_exist(void);

#endif
