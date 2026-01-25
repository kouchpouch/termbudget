#ifndef SORTER_H
#define SORTER_H

/* Returns total lines in the CSV */
extern int get_total_csv_lines();

/* A new version of sort_csv() with less chaos */
extern unsigned int sort_csv_new(int month, int day, int year);

/* Returns a the line in the CSV to insert the data sorted by date,
 * assumes the CSV is already sorted by date */
extern int sort_csv(int month, int day, int year); 

#endif
