#ifndef SORTER_H
#define SORTER_H

/* Returns a the line in the CSV to insert the data sorted by date,
 * assumes the CSV is already sorted by date */
extern int sort_csv(int month, int day, int year, int maxlines); 

#endif
