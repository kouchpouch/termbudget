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

#ifndef EDIT_TRANSACTION_H
#define EDIT_TRANSACTION_H

enum EditRecordFields {
	NO_SEL,
	E_DATE,
	E_CATG,
	E_DESC,
	E_TYPE,
	E_AMNT,
	DELETE,
};

void nc_edit_transaction(long b);

void cli_edit_transaction(void);

#endif
