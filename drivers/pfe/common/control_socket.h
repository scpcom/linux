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


#include "module_socket.h"


PSockEntry SOCKET_bind(U16 socketID, PVOID owner, U16 owner_type)__attribute__ ((noinline));
PSockEntry SOCKET_unbind(U16 socketID) __attribute__ ((noinline));

void SOCKET4_free_entries(void);
void SOCKET4_delete_route(PSockEntry pSocket);

int SOCKET4_HandleIP_Socket_Open (U16 *p, U16 Length);
int SOCKET4_HandleIP_Socket_Update (U16 *p, U16 Length);
int SOCKET4_HandleIP_Socket_Close (U16 *p, U16 Length);

void SOCKET6_free_entries(void);
void SOCKET6_delete_route(PSock6Entry pSocket);

int SOCKET6_HandleIP_Socket_Open (U16 *p, U16 Length);
int SOCKET6_HandleIP_Socket_Update (U16 *p, U16 Length);
int SOCKET6_HandleIP_Socket_Close (U16 *p, U16 Length);




