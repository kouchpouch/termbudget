#ifndef HELPER_H
#define HELPER_H

extern int upper(char* ltr); // Returns ascii value to uppercase
extern int lower(char* ltr); // Returns ascii value to lowercase
// Checks to see if a day exists in a month, returns maximum days in that month
extern bool dayexists(int d, int m, int y);

#endif
