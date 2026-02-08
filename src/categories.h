#ifndef CATEGORIES_H
#define CATEGORIES_H

#include "main.h"

typedef struct categorymember CategoryMember;

struct categorymember {
	CategoryMember *next;
	long byteoffset;
};

typedef struct categoryroot CategoryRoot;

struct categoryroot {
 	CategoryMember **pcc;
	CategoryRoot *next;
	long nmembers;
	long byteoffset;
};

typedef struct CategoryNode CategoryNode;

struct CategoryNode {
	CategoryNode *next;
	CategoryNode *prev;
	long byteoffset;
	Vec *data;
};

#endif
