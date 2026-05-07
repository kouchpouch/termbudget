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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "sorter.h"
#include "main.h"
#include "parser.h"
#include "filemanagement.h"

struct sorter_search_for {
	bool greater_year;
	bool matching_year;
	bool lesser_year;
	bool lesser_day;
};

static void set_to_false(struct sorter_search_for *s)
{
	s->greater_year = false;
	s->matching_year = false;
	s->lesser_year = false;
	s->lesser_day = false;
}

static int lines_to_last_occurance(FILE *fptr, int month, int year)
{
	char *str;
	char linebuff[LINE_BUFFER];
	int i = 0;
	int monthtok;
	int yeartok;
	while (1) {
		str = fgets(linebuff, sizeof(linebuff), fptr);
		if (str == NULL) {
			break;
		}
		monthtok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));
		if (monthtok == month && yeartok == year) {
			i++;
		} else {
			break;
		}
	}

	return i;
}

/* We assume that the CSV is sorted by date already. Because every operation
 * to edit or add a transaction will go through the sorting function to
 * determine where to insert the record. */
unsigned int sort_budget_csv(int month, int year)
{
	struct sorter_search_for found;
	FILE *fptr = open_budget_csv("r");
	unsigned int result_line = 1;
	unsigned int line = 1;
	int monthtok, yeartok;
	char linebuff[LINE_BUFFER];
	char *str;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		fclose(fptr);
		exit(1);
	}

	set_to_false(&found);

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;

		monthtok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));

		if (yeartok < year) {
			found.lesser_year = true;
		} else if (yeartok == year) {
			found.matching_year = true;
		} else if (yeartok > year) {
			found.greater_year = true;
		}

		if (yeartok < year) {
			result_line = line;
		} else if (yeartok > year) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month) {
			result_line = line;
			result_line += lines_to_last_occurance(fptr, month, year);
			break;
		}
		
		if (yeartok == year && monthtok > month) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok < month) {
			result_line = line;
		}
	}

	if (!found.lesser_year && !found.matching_year && found.greater_year) {
		result_line = 1;
	}

	fclose(fptr);

	return result_line;
}

/* We assume that the CSV is sorted by date already. Because every operation
 * to edit or add a transaction will go through the sorting function to
 * determine where to insert the record. */
unsigned int sort_record_csv(int month, int day, int year)
{
	struct sorter_search_for found;
	FILE *fptr = open_record_csv("r");
	unsigned int line = 1; // Line starts at 1 to skip the header
	unsigned int result_line = 1;
	unsigned int lessdayline = 0;
	char linebuff[LINE_BUFFER];
	char *str;
	int daytok, monthtok, yeartok;
	
	rewind(fptr);
	/* Read the header */
	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		fclose(fptr);
		exit(1);
	}

	set_to_false(&found);

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;

		monthtok = atoi(strsep(&str, ","));
		daytok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));

		if (yeartok < year) {
			found.lesser_year = true;
		} else if (yeartok == year) {
			found.matching_year = true;
		} else if (yeartok > year) {
			found.greater_year = true;
		}

		if (yeartok < year) {
			result_line = line;
		} else if (yeartok > year) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month && daytok == day) {
			found.lesser_day = false;
			result_line = line;
			break;
		}

		if (yeartok == year && monthtok == month && daytok > day) {
			found.lesser_day = false;
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month && daytok < day) {
			found.lesser_day = true;
			lessdayline = line;
		}
		
		if (yeartok == year && monthtok > month) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok < month) {
			result_line = line;
		}
	}

	if (!found.lesser_year && !found.matching_year && found.greater_year) {
		result_line = 1;
	} else if (found.lesser_day) {
		result_line = lessdayline;
	}

	fclose(fptr);

	return result_line;
}

unsigned int sort_converted_csv(int month, int day, int year, FILE *fptr)
{
	struct sorter_search_for found;
	char linebuff[LINE_BUFFER];
	char *str;
	unsigned int line = 1; // Line starts at 1 to skip the header
	unsigned int result_line = 1;
	unsigned int lessdayline = 0;
	int daytok, monthtok, yeartok;
	
	rewind(fptr);
	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		fclose(fptr);
		exit(1);
	}

	set_to_false(&found);

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;

		monthtok = atoi(strsep(&str, ","));
		daytok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));

		if (yeartok < year) {
			found.lesser_year = true;
		} else if (yeartok == year) {
			found.matching_year = true;
		} else if (yeartok > year) {
			found.greater_year = true;
		}

		if (yeartok < year) {
			result_line = line;
		} else if (yeartok > year) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month && daytok == day) {
			found.lesser_day = false;
			result_line = line;
			break;
		}

		if (yeartok == year && monthtok == month && daytok > day) {
			found.lesser_day = false;
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month && daytok < day) {
			found.lesser_day = true;
			lessdayline = line;
		}
		
		if (yeartok == year && monthtok > month) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok < month) {
			result_line = line;
		}
	}

	if (!found.lesser_year && !found.matching_year && found.greater_year) {
		result_line = 1;
	} else if (found.lesser_day) {
		result_line = lessdayline;
	}

	return result_line;
}
