#include "fileintegrity.h"
#include "main.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

bool record_len_verification() {
	FILE *fptr = open_record_csv("r");

	struct LineData ld, *pld = &ld;

	return true;
}
