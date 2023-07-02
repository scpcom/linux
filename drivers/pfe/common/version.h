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

#ifndef _VERSION_H_
#define _VERSION_H_

// Helper macros to create version string
#ifdef GCC_TOOLCHAIN
#define MAKE_VERS(v)	TOSTR(v)
#else
#define cat3(x,y,z)			x##y##z
#define MAKE_VERS(v)		TOSTR(cat3($Version:\40,v,\40$))
#endif

// The version string needs to be compliant to the Unix ident
// program.
// A version string that can be queried by the Unix ident program
// has to be of the format
// $<Tag>: <String> $
// Note the spaces after the : and before the trailing $
// The following macros guarantees that the final version string
// stored in FPP_version_string will always be in the right
// format. To generate a new version string, only the value of
// #define VERSION needs to be changed
//

#define VERSION mainline
#define VERSION_LENGTH 32

#endif /* _VERSION_H_ */
