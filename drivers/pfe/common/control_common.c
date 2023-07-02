/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */

#include "control_common.h"
#include "hal.h"


#if defined (COMCERTO_2000)

/** Gets the a field in an hardware  entry.
* The function read a hw entry field atomically
*
* @param field	pointer to a hardware entry field
* @return		field value
*
*/
U32 hw_entry_get_field(U32 *field)	
{
	return readl(field);
}

/** Sets the a field in an hardware  entry.
* The function write to hw entry field atomically
*
* @param field	pointer to a hardware entry field
* @param val	field value to set
*
*/
void hw_entry_set_field(U32 *field, U32 val)
{
	writel(val, field);
}

/** Sets the flags field in an hardware  entry.
* The function converts the value written from host format to PFE/DDR format (endianess conversion).
*
* @param hw_flags	pointer to an hardware entry
* @param flags		flags value to set
*
*/
void hw_entry_set_flags(U32 *hw_flags, U32 flags)
{
	/* FIXME if the entire conntrack is in non-cacheable/non-bufferable memory,
	  there should be no need for the memory barrier */
	wmb();
	writel(cpu_to_be32(flags), hw_flags);
}
#endif

