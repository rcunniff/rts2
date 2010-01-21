/*
 * Header for OAK communication
 * Copyright (C) 2010 Markus Wildi, version for RTS2
 * Copyright (C) 2009 Lukas Zimmermann, Basel, Switzerland
 *
 *
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Or visit http://www.gnu.org/licenses/gpl.html.

 *
 */

#ifndef __OAK_COMM_VERMES_H__
#define __OAK_COMM_VERMES_H__

#ifdef __cplusplus
extern "C"
{
#endif
void connectOakDiginDevice(int connecting);
#ifdef __cplusplus
}
#endif


// action in connectOakDiginDevice 
#define OAKDIGIN_CONNECT    0 
#define OAKDIGIN_DISCONNECT 1 

// OAK states bit patterns
//

#endif   // #ifndef __OAK_COMM_VERMES_H__

