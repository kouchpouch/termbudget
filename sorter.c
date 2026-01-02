#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "sorter.h"

/* We assume that the CSV is sorted by date already. Because every operation
 * to edit or add a transaction will go through the sorting function to
 * determine where to insert the record. */

/* Returns a line number to insert a record sorted by year, month, day */
int sort_csv(int month, int day, int year, int maxlines) {
	FILE *fptr = fopen("data.csv", "r");
	if (fptr == NULL) exit(1);

	//printf("Target: %d/%d/%d\n", month, day, year);
	int result_line = -1;
	int line = 1;
	char buff[200];
	int buffsize = sizeof(buff);
	char *str;
	int maxyear = 0;
	int minyear = INT_MAX;
	int yeartoken, monthtoken, daytoken;
	bool foundmatch = false;
	bool monthmatch = false;
	int *arr = calloc(maxlines, sizeof(int));
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
		} else if (yeartoken > maxyear) {
			maxyear = yeartoken;
		}

		/* If the year is not found in the CSV, foundmatch will remain false
		 * and the result line will be overwritten to either the end of the 
		 * file or the beginning of the file. */

		if (yeartoken == year) {
			/* If the year matches, descend into the month field */
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
			foundmatch = true;
			if (monthtoken < month) {
				result_line = line;
			} else if (monthtoken == month) { 
				monthmatch = true;
				if (daytoken <= day) {
					result_line = line;
				} else if (daytoken > day) { 
					result_line = line - 1; // Went too far, go back one line
					break;
				}
			/* Handle edge case where E.G. line 18 has only 1 matching month */
			} else if (monthmatch == true && monthtoken > month) {
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

	if (foundmatch == false) { 
		// No match was found for year, so the year must
		// not exist in the CSV. Either the record will be placed at the 
		// beginning or the end of the CSV
		//puts("No matches");
		//printf("Max year: %d\n", maxyear);
		//printf("Min year: %d\n", minyear);
		year > maxyear ? (result_line = line) : (result_line = 2);
	}

	free(arr);
	fclose(fptr);
	//printf("Iterations %d\n", line);
	return result_line;
}
//
//int main(void) {
//	int line = sort_csv(2022, 10, 1, 2048);
//	//printf("RESULTANT LINE %d\n", line);
//	return 0;
//}
