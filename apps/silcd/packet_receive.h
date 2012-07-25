/*

  packet_receive.h

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

#ifndef PACKET_RECEIVE_H
#define PACKET_RECEIVE_H

/* Prototypes */

void silc_server_notify(SilcServer server,
			SilcPacketStream sock,
			SilcPacket packet);
void silc_server_notify_list(SilcServer server,
			     SilcPacketStream sock,
			     SilcPacket packet);
void silc_server_private_message(SilcServer server,
				 SilcPacketStream sock,
				 SilcPacket packet);
void silc_server_private_message_key(SilcServer server,
				     SilcPacketStream sock,
				     SilcPacket packet);
void silc_server_command_reply(SilcServer server,
			       SilcPacketStream sock,
			       SilcPacket packet);
void silc_server_channel_message(SilcServer server,
				 SilcPacketStream sock,
				 SilcPacket packet);
void silc_server_channel_key(SilcServer server,
			     SilcPacketStream sock,
			     SilcPacket packet);
SilcClientEntry silc_server_new_client(SilcServer server,
				       SilcPacketStream sock,
				       SilcPacket packet);
SilcServerEntry silc_server_new_server(SilcServer server,
				       SilcPacketStream sock,
				       SilcPacket packet);
void silc_server_new_channel(SilcServer server,
			     SilcPacketStream sock,
			     SilcPacket packet);
void silc_server_new_channel_list(SilcServer server,
				  SilcPacketStream sock,
				  SilcPacket packet);
void silc_server_new_id(SilcServer server, SilcPacketStream sock,
			SilcPacket packet);
void silc_server_new_id_list(SilcServer server, SilcPacketStream sock,
			     SilcPacket packet);
void silc_server_remove_id(SilcServer server,
			   SilcPacketStream sock,
			   SilcPacket packet);
void silc_server_remove_id_list(SilcServer server,
				SilcPacketStream sock,
				SilcPacket packet);
void silc_server_key_agreement(SilcServer server,
			       SilcPacketStream sock,
			       SilcPacket packet);
void silc_server_connection_auth_request(SilcServer server,
					 SilcPacketStream sock,
					 SilcPacket packet);
void silc_server_ftp(SilcServer server,
		     SilcPacketStream sock,
		     SilcPacket packet);
void silc_server_resume_client(SilcServer server,
			       SilcPacketStream sock,
			       SilcPacket packet);

#endif
