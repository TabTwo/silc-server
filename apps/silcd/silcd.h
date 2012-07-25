/*

  silcd.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1997 - 2002 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef SILCD_H
#define SILCD_H

/* Default server configuration file. This can be overridden at the
   compilation time. Otherwise, use default. This can be overridden on
   command line as well. */
#ifndef SILC_SERVER_CONFIG_FILE
#define SILC_SERVER_CONFIG_FILE "/etc/silc/silcd.conf"
#endif

#define SILC_SERVER_PUBLIC_KEY_NAME "/silcd.pub"
#define SILC_SERVER_PRIVATE_KEY_NAME "/silcd.prv"

#endif
