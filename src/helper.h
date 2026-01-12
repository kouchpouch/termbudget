#ifndef HELPER_H
#define HELPER_H

/* Returns ltr ascii value to uppercase */
extern int upper(char* ltr);

/* Returns ltr ascii value to lowercase */
extern int lower(char* ltr);

/* Checks to see if a day exists in a month, 
 * returns maximum days in that month */
extern bool dayexists(int d, int m, int y);

#endif
