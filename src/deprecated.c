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

/* Includes some deprecated functions, they do not link to anything, they
 * are not to be used. This file is for reference only and will not be
 * included in releases. */

// From main.c
/* Returns a calloc'd array of months in which records exist in fptr which
 * match the year of matchyear. The month value is '0' if that month doesn't
 * exist. The size of the array is always 12. */
[[deprecated]] int *list_records_by_month_old(FILE *fptr, int matchyear)
{
	char linebuff[LINE_BUFFER];
	char *str;
	int *months = calloc(12, sizeof(int));
	int year;
	int month;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
	}

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		month = atoi(strsep(&str, ","));
		seek_n_fields(&str, 1);
		year = atoi(strsep(&str, ","));
		if (matchyear == year) {
			if (months[0] == 0) {
				months[0] = month;
			} else if (month != months[i]) {
				i++;
				months[i] = month;
			}
		} else if (matchyear < year) {
			break;
		}
	}

	return months;
}

// From main.c
/* Returns an malloc'd array of integers containing the years in which records
 * are found in fptr. A '0' marks the end of the array. */
[[deprecated]] int *list_records_by_year_old(FILE *fptr)
{
	char linebuff[LINE_BUFFER];
	char *str;
	int *years = calloc(1, sizeof(int));
	int year;
	int i = 0;

	if (seek_beyond_header(fptr) == -1) {
		puts("Failed to read header");
		free(years);
		return NULL;
	}

	str = fgets(linebuff, sizeof(linebuff), fptr); // Read first year into index 0
	if (str == NULL) {
		free(years);
		return NULL;
	}
	seek_n_fields(&str, 2);
	years[i] = atoi(strsep(&str, ",")); // year

	while ((str = fgets(linebuff, sizeof(linebuff), fptr)) != NULL) {
		seek_n_fields(&str, 2);
		year = atoi(strsep(&str, ",")); // year
		if (year != years[i]) {
			i++;
			int *tmp = realloc(years, (i + 1) * sizeof(int));
			if (tmp == NULL) {
				free(years);
				memory_allocate_fail();
			}
			years = tmp;
			years[i] = year;
		}
	}
	int *tmp = realloc(years, (i + 2) * sizeof(int));
	if (tmp == NULL) {
		free(years);
		memory_allocate_fail();
	}
	years = tmp;
	/* Place a zero at the end of the array to mark the end */
	years[i + 1] = 0; 

	printf("\n");
	return years;
}
