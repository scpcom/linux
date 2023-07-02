/*
 *  Copyright (c) 2011, 2014 Freescale Semiconductor, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
*/
#ifndef ___PFE_CTRL_H_
#define ___PFE_CTRL_H_

/** @file
* PFE control code entry points for upper layer linux driver.
*/


/** Control code lower layer command handler.
* Parses and executes commands coming from the upper layers, possibly
* modifying control code internal data structures and well as those used by the PFE engine firmware.
*
*/
void __pfe_ctrl_cmd_handler(u16 fcode, u16 length, u16 *payload, u16 *rlen, u16 *rbuf);


/** Control code lower layer initialization function.
* Initalizes all internal data structures by calling each of the control code modules
* init functions.
*
*/
int __pfe_ctrl_init(void);


/** Control code lower layer cleanup function.
* Frees all the resources allocated by the control code lower layer,
* resets all it's internal data structures and the data structures of the PFE engine firmware
* by calling each of the control code modules exit functions.
*
*/
void __pfe_ctrl_exit(void);

#endif /* ___PFE_CTRL_H_ */
