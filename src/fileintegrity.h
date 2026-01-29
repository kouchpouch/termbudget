/*
 * Verify data files that termBudget uses comply with the maximum lengths
 * defined in "main.h". Validate data between files as well, with RECORD_DIR
 * being the master.
 */
#ifndef FILEINTEGRITY_H
#define FILEINTEGRITY_H

#include <stdbool.h>

/* Returns true if record csv fields are equal to or less than the maximum
 * lengths defined in main. False if not */
extern bool record_len_verification();


#endif
