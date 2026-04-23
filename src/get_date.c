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

#include <time.h>

int get_current_day(void)
{
	time_t timer;
	time(&timer);
	struct tm *tm = localtime(&timer);
	return tm->tm_mday;
}

int get_current_month(void)
{
	time_t timer;
	time(&timer);
	struct tm *tm = localtime(&timer);
	return tm->tm_mon;
}

int get_current_year(void)
{
	time_t timer;
	time(&timer);
	struct tm *tm = localtime(&timer);
	return tm->tm_year + 1900;
}
