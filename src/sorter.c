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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "sorter.h"
#include "main.h"
#include "parser.h"

int get_total_csv_lines() {
	FILE *fptr = fopen(RECORD_DIR, "r");
	int lines = 0;
	char buff[256];
	while (fgets(buff, sizeof(buff), fptr) != NULL) {
		lines++;
	}
	return lines;
}

/* We assume that the CSV is sorted by date already. Because every operation
 * to edit or add a transaction will go through the sorting function to
 * determine where to insert the record. */
unsigned int sort_budget_csv(int month, int year) {
	FILE *fptr = open_budget_csv("r");
	unsigned int result_line = 1;
	unsigned int line = 1;

	if (seek_beyond_header(fptr) == -1) {
		perror("Failed to read header");
		fclose(fptr);
		exit(1);
	}

	char linebuff[LINE_BUFFER];
	char *str;

	unsigned int monthtok, yeartok;
	bool greateryear = false;
	bool matchingyear = false;
	bool lesseryear = false;

	while((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;

		monthtok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));

		if (yeartok < year) {
			lesseryear = true;
		} else if (yeartok == year) {
			matchingyear = true;
		} else if (yeartok > year) {
			greateryear = true;
		}

		if (yeartok < year) {
			result_line = line;
		} else if (yeartok > year) {
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month) {
			result_line = line;
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

	if (!lesseryear && !matchingyear && greateryear) {
		result_line = 1;
	}

	fclose(fptr);

	return result_line;
}

/* We assume that the CSV is sorted by date already. Because every operation
 * to edit or add a transaction will go through the sorting function to
 * determine where to insert the record. */
unsigned int sort_record_csv(int month, int day, int year) {
	FILE *fptr = open_record_csv("r");
	unsigned int line = 1; // Line starts at 1 to skip the header
	unsigned int result_line = 1;
	unsigned int lessdayline = 0;
	
	/* Read the header */
	if (seek_beyond_header(fptr) == -1) {
		perror("Failed to read header");
		fclose(fptr);
		exit(1);
	}
	
	char linebuff[LINE_BUFFER];
	char *str;

	unsigned int daytok, monthtok, yeartok;
	bool greateryear = false;
	bool matchingyear = false;
	bool lesseryear = false;
	/* lessermonth isn't used, but I'm keeping it here until I prove I don't
	 * need it */
	bool lessermonth = false;
	bool lesserday = false;

	while((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		line++;

		monthtok = atoi(strsep(&str, ","));
		daytok = atoi(strsep(&str, ","));
		yeartok = atoi(strsep(&str, ","));

		if (yeartok < year) {
			lesseryear = true;
		} else if (yeartok == year) {
			matchingyear = true;
		} else if (yeartok > year) {
			greateryear = true;
		}

		if (yeartok < year) {
			result_line = line;
		} else if (yeartok > year) {
			result_line = line - 1;
			break;
		}
//		if (yeartok > year) {
//			result_line = line - 1;
//			break;
//		}

		if (yeartok == year && monthtok == month && daytok == day) {
			lesserday = false;
			lessermonth = false;
			result_line = line;
			break;
		}

		if (yeartok == year && monthtok == month && daytok > day) {
			lesserday = false;
			lessermonth = false;
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok == month && daytok < day) {
			lesserday = true;
			lessermonth = false;
			lessdayline = line;
		}
		
		if (yeartok == year && monthtok > month) {
			lessermonth = false;
			result_line = line - 1;
			break;
		}

		if (yeartok == year && monthtok < month) {
			lessermonth = true;
			result_line = line;
		}
	}

	if (!lesseryear && !matchingyear && greateryear) {
		result_line = 1;
	} else if (lesserday) {
		result_line = lessdayline;
	}

	fclose(fptr);

	return result_line;
}

/* Returns a line number to insert a record sorted by year, month, day */
[[deprecated("Do not use, replaced by sort_csv_new")]]
int sort_csv(int month, int day, int year) {
	FILE *fptr = fopen("data.csv", "r");
	if (fptr == NULL) exit(1);

	int result_line = -1;
	int line = 1;
	char buff[200];
	int buffsize = sizeof(buff);
	char *str;
	int maxyear = 0;
	int minyear = INT_MAX;
	int yeartoken, monthtoken, daytoken;
	bool yearmatch = false;
	bool monthmatch = false;
	bool lessermonthsfound = false;
	bool greatermonthfound = false;
	bool yearlessthanmin = false;

	int realloc_counter = 0;
	/* Initial alloc of 64 integers */
	int *arr = calloc(REALLOC_INCR, sizeof(int));
	int idx = 0;

	(void)fgets(buff, buffsize, fptr); // Header, throwaway
	str = fgets(buff, buffsize, fptr);
	while (str != NULL) {
		line++;
		monthtoken = atoi(strsep(&str, ","));
		daytoken = atoi(strsep(&str, ","));
		yeartoken = atoi(strsep(&str, ","));

		if (yeartoken < minyear) {
			minyear = yeartoken;
		}
		if (year < minyear) {
			yearlessthanmin = true;	
		} else {
			yearlessthanmin = false;
		}

		if (yeartoken > maxyear) {
			maxyear = yeartoken;
		}

		if (yeartoken == year) {
			/* If the year matches, descend into the month field */
			realloc_counter++;
			if (realloc_counter >= REALLOC_INCR) {
				realloc_counter = 0;
				int *tmp = realloc(arr, ((idx + REALLOC_INCR) * sizeof(int)));
				if (tmp == NULL) {
					free(arr);
				}
				arr = tmp;
			}
			arr[idx] = line;

			/* Fill the array with line numbers to use in the event that no
			 * month is less than or equal to month; in order to go back to the
			 * first occurance of year.
			 * E.G. Year = 2022, Month = 2, Day = 1;
			 * E.G. CSV does not contain the months 1 or 2, but we already
			 * searched over those lines until a later month is found
			 * arr contains line numbers where the years match
			 * If no month matches, go back to the first occurance of the year
			 * [63] [64] [65] [66] [67]
			 *  ^ -- Found at arr[0] 
			 */

			idx++;
			yearmatch = true;
			if (monthtoken < month) {
				result_line = line;
				lessermonthsfound = true;
			} else if (monthtoken == month) {
				lessermonthsfound = false;
				monthmatch = true;
				if (daytoken <= day) {
					result_line = line;
				} else if (daytoken > day) { 
					result_line = line - 1; // Went too far, go back one line
					break;
				}
			/* Handle edge case where a line has only 1 matching month */
			} else if (monthmatch == true && monthtoken > month) {
				lessermonthsfound = false;
				result_line = line;
				break;
			} else if (monthmatch == false && monthtoken > month) {
				greatermonthfound = true;
				result_line = line;
				break;	
			} else {
				result_line = arr[0];
				break;
			}
		}
		if (yeartoken > year) {
			break; // We went far enough, stop the search
		}
		str = fgets(buff, buffsize, fptr);
	}

	if (yearmatch == false && yearlessthanmin == false) { 
		if (year < maxyear) {
			free(arr);
			fclose(fptr);
			return line - 1;
		} else if (year > maxyear) {
			result_line = line;
		}
	} else if (yearmatch == false) {
		year > maxyear ? (result_line = line) : (result_line = 1);
	}
	if (yearmatch && !monthmatch && lessermonthsfound && greatermonthfound) {
		free(arr);
		fclose(fptr);
		return result_line - 1;
	} else if (yearmatch && !monthmatch && lessermonthsfound) {
		free(arr);
		fclose(fptr);
		return result_line;
	} else if (yearmatch && !monthmatch) {
		free(arr);
		fclose(fptr);
		return result_line - 1;
	}

	free(arr);
	fclose(fptr);
	return result_line;
}
