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

#include "fileintegrity.h"
#include "categories.h"
#include "filemanagement.h"
#include "create.h"
#include "helper.h"
#include "main.h"
#include "parser.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool validate_record_header(void)
{
	FILE *fptr = open_record_csv("r");
	struct record_header *rh = parse_record_header(fptr);
	fclose(fptr);

	if (rh->n_fields < 0) {
		free(rh);
		return false;
	}

	int arr[] = {rh->month, rh->day, rh->year, rh->catg, rh->desc, rh->transtype, rh->value};
	for (int i = 0; i < rh->n_fields; i++) {
		if (arr[i] < 0) {
			free(rh);
			return false;
		}
	}

	free(rh);
	return true;
}

bool validate_budget_header(void)
{
	FILE *fptr = open_budget_csv("r");
	struct budget_header *bh = parse_budget_header(fptr);
	fclose(fptr);

	if (bh->n_fields < 0) {
		free(bh);
		return false;
	}

	int arr[] = {bh->month, bh->year, bh->catg, bh->transtype, bh->value};
	for (int i = 0; i < bh->n_fields; i++) {
		if (arr[i] < 0) {
			free(bh);
			return false;
		}
	}

	free(bh);
	return true;
}

bool record_len_verification(void)
{
	FILE *fptr = open_record_csv("r");
	_transact_tokens_t ld, *pld = &ld;
	char linebuff[1024];
	char *str;

	/* This is a human readable line number, not to be used for indexing or
	 * other parsing functions. This number is for error printing only 
	 * starting at 2. Header was 1. */
	unsigned int linenumber = 2;

	seek_beyond_header(fptr);


	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}

		tokenize_record(pld, &str);
		if (intlen(pld->day) > 2) {
			printf("Day on line %d in %s exceeds maximum length\n", 
				   linenumber, RECORD_DIR);
			return false;
		}

		if (intlen(pld->month) > 2) {
			printf("Month on line %d in %s exceeds maximum length\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		if (pld->year > MAX_YEAR || pld->year < MIN_YEAR) {
			printf("Year on line %d in %s out of range\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		if (strlen(pld->category) > MAX_LEN_CATG) {
			printf("Category on line %d in %s exceeds maximum length\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		if (strlen(pld->desc) > MAX_LEN_DESC) {
			printf("Description on line %d in %s exceeds maximum length\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		if (pld->transtype != 0 && pld->transtype != 1) {
			printf("Invalid transcation type on line %d in %s\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		if (intlen((int)pld->amount) > MAX_LEN_AMOUNT) {
			printf("Amount on line %d in %s exceeds maximum length\n", 
		  		   linenumber, RECORD_DIR);
			return false;
		}

		free_tokenized_record_strings(pld);

		linenumber++;
	}

	return true;
}

static int cmp_catg_and_fix
(struct catg_vec *prc, struct catg_vec *pbc, int m, int y) 
{
	int corrected = 0;
	bool cat_exists = false;

	for (size_t i = 0; i < prc->size; i++) {
		cat_exists = false;
		for (size_t j = 0; j < pbc->size; j++) {
			if (strcasecmp(prc->categories[i], pbc->categories[j]) == 0) {
				cat_exists = true;
			}
		}
		if (!cat_exists) {
			insert_budget_record(prc->categories[i], m, y, 0, 0.0);
			corrected++;
		}
	}
	return corrected;
}

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
int verify_categories_exist_in_budget(void)
{
	FILE *rfptr = open_record_csv("r");
	struct vec_t *years;
	struct vec_t *months;
	int corrected = 0;
	struct catg_vec prc_, *prc = &prc_;
	struct catg_vec pbc_, *pbc = &pbc_;

	/* Go through each year and find the matching months, then find the
	 * matching categories, then compare. */
	years = get_years_with_data(rfptr, 2);
	if (years == NULL) {
		return 0;
	}

	for (size_t i = 0; i < years->size; i++) {
		rewind(rfptr);
		months = get_months_with_data(rfptr, years->data[i], 1);
		for (size_t j = 0; j < months->size; j++) {
			prc = get_categories(months->data[j], years->data[i]);
			pbc = get_budget_catg_by_date(months->data[j], years->data[i]);
			corrected += cmp_catg_and_fix(prc, pbc, months->data[j], years->data[i]);
			free_categories(prc);
			free_categories(pbc);
		}
		free(months);
	}

	free(years);
	fclose(rfptr);

	return corrected;
}

static void fix_budget_header(void)
{
	int linetodelete = 0;
	FILE *fptr = open_budget_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		} else {
			fputs("month,year,category,transtype,value\n", tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_budget_file(tmpfptr, fptr);
}

static void fix_record_header(void)
{
	int linetodelete = 0;
	FILE *fptr = open_record_csv("r");
	FILE *tmpfptr = open_temp_csv();

	char linebuff[LINE_BUFFER * 2];
	char *line;
	int linenum = 0;
	do {
		line = fgets(linebuff, sizeof(linebuff), fptr);
		if (line == NULL) break;
		if (linenum != linetodelete) {
			fputs(line, tmpfptr);
		} else {
			fputs("month,day,year,category,description,transtype,value\n", tmpfptr);
		}
		linenum++;	
	} while (line != NULL);
	mv_tmp_to_record_file(tmpfptr, fptr);
}

/* Verifies that the files needed to run termbudget exist and writes headers */
int verify_files_exist(void)
{
	FILE *rfptr = open_record_csv("a");
	if (rfptr == NULL) {
		perror("Failed to open/create record file");
		return -1;
	}
	fseek(rfptr, 0, SEEK_END);
	if (ftell(rfptr) == 0) {
		fputs("month,day,year,category,description,transtype,value\n", rfptr);
	}
	fclose(rfptr);

	if (!validate_record_header()) {
		printf("data.csv header failed validation, has it been edited?");
		fix_record_header();
		getchar();
	}

	FILE *bfptr = open_budget_csv("a");
	if (bfptr == NULL) {
		perror("Failed to open/create budget file");
		return -1;
	}
	fseek(bfptr, 0, SEEK_END);
	if (ftell(bfptr) == 0) {
		fputs("month,year,category,transtype,value\n", bfptr);
	}
	fclose(bfptr);

	if (!validate_budget_header()) {
		printf("budget.csv header failed validation, has it been edited?");
		fix_budget_header();
		getchar();
	}

	return 0;
}
