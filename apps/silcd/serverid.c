/*

  serverid.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1997 - 2007 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/
/* $Id$ */

#include "serverincludes.h"
#include "server_internal.h"

/* Creates a Server ID. Newly created Server ID is returned to the
   new_id argument. */

void silc_id_create_server_id(const char *ip, SilcUInt16 port, SilcRng rng,
			      SilcServerID **new_id)
{
  SILC_LOG_DEBUG(("Creating new Server ID"));

  *new_id = silc_calloc(1, sizeof(**new_id));

  /* Create the ID */

  if (!silc_net_addr2bin(ip, (*new_id)->ip.data,
			 sizeof((*new_id)->ip.data))) {
    silc_free(*new_id);
    *new_id = NULL;
    return;
  }

  (*new_id)->ip.data_len = silc_net_is_ip4(ip) ? 4 : 16;
  (*new_id)->port = SILC_SWAB_16(port);
  (*new_id)->rnd = 0xff;

  SILC_LOG_DEBUG(("New ID (%s)", silc_id_render(*new_id, SILC_ID_SERVER)));
}

/* Creates Client ID. This assures that there are no collisions in the
   created Client IDs.  If the collision would occur (meaning that there
   are 2^8 occurences of the `nickname' this will return FALSE, and the
   caller must recall the function with different nickname. If this returns
   TRUE the new ID was created successfully. */

SilcBool silc_id_create_client_id(SilcServer server,
				  SilcServerID *server_id, SilcRng rng,
				  SilcHash md5hash,
				  unsigned char *nickname, SilcUInt32 nick_len,
				  SilcClientID **new_id)
{
  unsigned char hash[16];
  SilcBool finding = FALSE;

  SILC_LOG_DEBUG(("Creating new Client ID"));

  *new_id = silc_calloc(1, sizeof(**new_id));

  /* Create hash of the nickname (it's already checked as valid identifier
     string). */
  silc_hash_make(md5hash, nickname, nick_len, hash);

  /* Create the ID */
  memcpy((*new_id)->ip.data, server_id->ip.data, server_id->ip.data_len);
  (*new_id)->ip.data_len = server_id->ip.data_len;
  (*new_id)->rnd = silc_rng_get_byte(rng);
  memcpy((*new_id)->hash, hash, CLIENTID_HASH_LEN);

  /* Assure that the ID does not exist already */
  while (1) {
    if (!silc_idlist_find_client_by_id(server->local_list,
				       *new_id, FALSE, NULL))
      if (!silc_idlist_find_client_by_id(server->global_list,
					 *new_id, FALSE, NULL))
	break;

    /* The ID exists, start increasing the rnd from 0 until we find a
       ID that does not exist. If we wrap and it still exists then we
       will return FALSE and the caller must send some other nickname
       since this cannot be used anymore. */
    (*new_id)->rnd++;

    if (finding && (*new_id)->rnd == 0)
      return FALSE;

    if (!finding) {
      (*new_id)->rnd = 0;
      finding = TRUE;
    }
  }

  SILC_LOG_DEBUG(("New ID (%s)", silc_id_render(*new_id, SILC_ID_CLIENT)));

  return TRUE;
}

/* Creates Channel ID */

SilcBool silc_id_create_channel_id(SilcServer server,
				   SilcServerID *router_id, SilcRng rng,
				   SilcChannelID **new_id)
{
  SilcBool finding = TRUE;

  SILC_LOG_DEBUG(("Creating new Channel ID"));

  *new_id = silc_calloc(1, sizeof(**new_id));

  /* Create the ID */
  memcpy((*new_id)->ip.data, router_id->ip.data, router_id->ip.data_len);
  (*new_id)->ip.data_len = router_id->ip.data_len;
  (*new_id)->port = router_id->port;
  (*new_id)->rnd = silc_rng_get_rn16(rng);

  /* Assure that the ID does not exist already */
  while (1) {
    if (!silc_idlist_find_channel_by_id(server->local_list,
					*new_id, NULL))
      break;

    (*new_id)->rnd++;

    if (finding && (*new_id)->rnd == 0)
      return FALSE;

    if (!finding) {
      (*new_id)->rnd = 0;
      finding = TRUE;
    }
  }

  SILC_LOG_DEBUG(("New ID (%s)", silc_id_render(*new_id, SILC_ID_CHANNEL)));

  return TRUE;
}

/* Checks whether the `server_id' is valid.  It must be based to the
   IP address provided in the `remote' socket connection. */

SilcBool silc_id_is_valid_server_id(SilcServer server,
				    SilcServerID *server_id,
				    SilcPacketStream remote)
{
  unsigned char ip[16];
  const char *remote_ip;
  SilcStream stream = silc_packet_stream_get_stream(remote);

  silc_socket_stream_get_info(stream, NULL, NULL, &remote_ip, NULL);
  if (!silc_net_addr2bin(remote_ip, ip, sizeof(ip)))
    return FALSE;

  if (silc_net_is_ip4(remote_ip)) {
    if (!memcmp(server_id->ip.data, ip, 4))
      return TRUE;
  } else {
    if (!memcmp(server_id->ip.data, ip, 16))
      return TRUE;
  }

  return FALSE;
}
