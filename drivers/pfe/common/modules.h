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


#ifndef _MODULES_H_
#define _MODULES_H_

#include "types.h"



#ifdef COMCERTO_2000
#include "mtd.h"

#define NUM_CHANNEL EVENT_MAX

typedef struct tModuleDesc {
	void (*entry)(void);
	int (*get_fill_level)(void);
} ModuleDesc, *PModuleDesc;
#else

typedef struct tModuleDesc {
	void (*entry)(void);
	U16 (*cmdproc)(U16 cmd_code, U16 cmd_len, U16 *pcmd);
} ModuleDesc, *PModuleDesc;
#endif

/* modules core functions */
typedef BOOL (*MODULE_INIT)(PModuleDesc);
void module_register(U32 event_id, MODULE_INIT init) __attribute__ ((noinline));


#endif /* _MODULES_H_ */
