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
 *
 * termbudget 2026
 * Author: kouchpouch <https://github.com/kouchpouch/termbudget>
 */

#ifndef EDIT_TRANSACTION_H
#define EDIT_TRANSACTION_H

enum EditRecordFields {
	NO_RCRD_SELECT,
	EDIT_RCRD_DATE,
	EDIT_RCRD_CATG,
	EDIT_RCRD_DESC,
	EDIT_RCRD_TYPE,
	EDIT_RCRD_AMNT,
	EDIT_RCRD_DELETE,
};

int nc_edit_transaction(long b, struct read_state *rret);

int nc_edit_transaction_opt(long b, 
							int opt,
							struct read_state *rret);

#endif
