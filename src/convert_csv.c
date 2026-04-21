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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "parser.h"
#include "sorter.h"
#include "filemanagement.h"

static void write_temp_header(FILE *convfptr)
{
	char *header = "month,day,year,category,description,transtype,value";
	fprintf(convfptr, "%s\n", header); 
}

static void create_csv(void)
{
	FILE *fptr = fopen(CONVERTED_FILE_DIR, "w+");
	if (fptr == NULL) {
		perror("Failed to open file");
		exit(1);
	}
	write_temp_header(fptr);
	fclose(fptr);
}

static FILE *open_csv(char *dir)
{
	FILE *fptr = fopen(dir, "r");
	if (fptr == NULL) {
		perror("Failed to open file");
		exit(1);
	}
	return fptr;
}

static void insert_record(_transact_tokens_t *ld)
{
	FILE *convfptr = fopen(CONVERTED_FILE_DIR, "r");
	if (convfptr == NULL) {
		perror("Failed to open file");
		exit(1);
	}

	unsigned int linenum_insert = 
		sort_converted_csv(ld->month, ld->day, ld->year, convfptr);

	rewind(convfptr);

	FILE *tmpfptr = open_temp_csv();
	unsigned int linenum = 0;
	char linebuff[LINE_BUFFER];
	char *str;

	while ((str = fgets(linebuff, sizeof(linebuff), convfptr)) != NULL) { 
		if (str == NULL) {
			break;
		}
		linenum++;
		if (linenum != linenum_insert) {
			fputs(str, tmpfptr);
		} else if (linenum == linenum_insert) {
			fputs(str, tmpfptr);
			fprintf(tmpfptr, "%d,%d,%d,%s,%s,%d,%.2f\n",
					ld->month,
					ld->day,
					ld->year,
					ld->category,
					ld->desc,
					ld->transtype,
					ld->amount
			);
		}
	}

	rename(TEMP_FILE_DIR, CONVERTED_FILE_DIR); 
	fclose(convfptr);
	fclose(tmpfptr);
}

static size_t tokenize_and_convert(FILE *fptr)
{
	_transact_tokens_t _ld, *ld = &_ld;
	char *str;
	char buffer[LINE_BUFFER];
	double amount;
	size_t count = 0;

	// Skip the header
	fgets(buffer, sizeof(buffer), fptr); 

	while (1) {
		str = fgets(buffer, sizeof(buffer), fptr); 
		if (str == NULL)
			break;
		ld->month = atoi(strsep(&str, "/"));
		ld->day = atoi(strsep(&str, "/"));
		ld->year = atoi(strsep(&str, "/"));
		(void)strsep(&str, ",");
		ld->desc = strsep(&str, ",");
		ld->category = strsep(&str, ",");
		(void)strsep(&str, ",");
		amount = atof(strsep(&str, ","));

		if (amount < 0.0) {
			ld->transtype = 0;
			ld->amount = -(amount);
		} else {
			ld->transtype = 1;
			ld->amount = amount;
		}

		insert_record(ld);

		count++;
	}

	return count;
}

size_t convert_chase_csv(char *dir)
{
	printf("FILE: %s\n", dir);
	printf("CONVERTED: %s\n", CONVERTED_FILE_DIR);
	FILE *fptr = open_csv(dir);
	create_csv();
	size_t count = tokenize_and_convert(fptr);
	fclose(fptr);
	return count;
}
