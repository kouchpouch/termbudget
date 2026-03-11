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

#include "fileintegrity.h"
#include "filemanagement.h"
#include "helper.h"
#include "main.h"
#include "parser.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool validate_record_header(void) {
	FILE *fptr = open_record_csv("r");
	struct RecordHeader *rh = parse_record_header(fptr);
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

bool validate_budget_header(void) {
	FILE *fptr = open_budget_csv("r");
	struct BudgetHeader *bh = parse_budget_header(fptr);
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

bool record_len_verification(void) {
	FILE *fptr = open_record_csv("r");

	struct LineData ld, *pld = &ld;

	char linebuff[1024];
	char *str;

	seek_beyond_header(fptr);

	/* This is a human readable line number, not to be used for indexing or
	 * other parsing functions. This number is for error printing only 
	 * starting at 2. Header was 1. */
	unsigned int linenumber = 2;

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

		linenumber++;
	}

	return true;
}
