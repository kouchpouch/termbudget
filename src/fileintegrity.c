#include "fileintegrity.h"
#include "helper.h"
#include "main.h"
#include "parser.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool record_len_verification() {
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


