/*
 * Includes functions which find the appropriate line to insert a record
 * into a CSV with the first three fields of every record being
 * "month,day,year".
 */
#ifndef SORTER_H
#define SORTER_H

/* Returns total lines in the CSV */
extern int get_total_csv_lines();

/* Returns the line sorted by date to insert data for budget.csv */
extern unsigned int sort_budget_csv(int month, int year);

/* A new version of sort_csv() with less chaos */
extern unsigned int sort_record_csv(int month, int day, int year);

/* Returns a the line in the CSV to insert the data sorted by date,
 * assumes the CSV is already sorted by date */
[[deprecated]]extern int sort_csv(int month, int day, int year); 

#endif
