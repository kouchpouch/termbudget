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

/*
 * CategoryNode is to be used as a doubly linked list which includes members
 * of CategoryNode pointer next and prev, long catg_fp stores the byte offset
 * of the category in BUDGET_DIR (from main.h), and a Vec containing all
 * records in RECORD_DIR (from main.h) that match the category field at catg_fp
 * in BUDGET_DIR.
 */
typedef struct CategoryNode CategoryNode;

struct CategoryNode {
	CategoryNode *next;
	CategoryNode *prev;
	long catg_fp; // Category File Position
	Vec *data;
};

#endif
