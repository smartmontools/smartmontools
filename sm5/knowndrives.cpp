/*
 * knowndrives.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 * Address of support mailing list: smartmontools-support@lists.sourceforge.net
 *
 * Copyright (C) 2003 Bruce Allen, Philip Williams 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include "knowndrives.h"

const char *knowndrives_c_cvsid="$Id: knowndrives.cpp,v 1.1 2003/04/08 21:40:01 pjwilliams Exp $" KNOWNDRIVES_H_CVSID;

/* Table of settings for known drives terminated by an element containing all
 * zeros.  The drivesettings structure is described in knowndrives.h */
const drivesettings knowndrives[] = {
  /*------------------------------------------------------------
   *  IBM Deskstar 60GXP series   
   *------------------------------------------------------------ */
  {
    "IC35L0[12346]0AVER07",
    NULL,
    "IBM Deskstar 60GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/index.html#rel",
    NULL,
    NULL
  },
  /*------------------------------------------------------------
   *  IBM Deskstar 40GV & 75GXP series
   *------------------------------------------------------------ */
  {
    "(IBM-)?DTLA-30[57]0[123467][05]",
    NULL,
    "IBM Deskstar 40GV and 75GXP drives may need upgraded SMART firmware.\n"
      "Please see http://www.geocities.com/dtla_update/",
    NULL,
    NULL
  },
  /*------------------------------------------------------------
   *  Phil's IBM Deskstar 120GXP (FOR TESTING)
   *------------------------------------------------------------ */
/*
  {
     "^[[:space:]]*IC35L060AVVA07-0[[:space:]]*$",
     NULL,
     "Hey, you've got an IBM Deskstar 120GXP!\n",
     NULL,
     NULL,
  },
*/
  /*------------------------------------------------------------
   *  End of table.  Please add entries above this marker.
   *------------------------------------------------------------ */
  {0, 0, 0, 0, 0}
};
