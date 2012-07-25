/*

  idlist.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1997 - 2005, 2007 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef IDLIST_H
#define IDLIST_H

#include "serverconfig.h"

/* Context for holding cache information to periodically purge
   the cache. */
typedef struct {
  SilcIDCache cache;
  SilcUInt32 timeout;
} *SilcIDListPurge;

/* Channel key re-key context. */
typedef struct {
  SilcChannelEntry channel;
  SilcUInt32 key_len;
  SilcTask task;
} *SilcServerChannelRekey;

/* ID List Entry status flags. */
typedef SilcUInt8 SilcIDListStatus;
#define SILC_IDLIST_STATUS_NONE         0x00  /* No status */
#define SILC_IDLIST_STATUS_REGISTERED   0x01  /* Entry is registered */
#define SILC_IDLIST_STATUS_RESOLVED     0x02  /* Entry info is resolved */
#define SILC_IDLIST_STATUS_RESOLVING    0x04  /* Entry is being resolved
						 with WHOIS or IDENTIFY */
#define SILC_IDLIST_STATUS_DISABLED     0x08  /* Entry is disabled */
#define SILC_IDLIST_STATUS_RESUMED      0x10  /* Entry is resumed */
#define SILC_IDLIST_STATUS_LOCAL        0x20  /* Entry locally connected */
#define SILC_IDLIST_STATUS_RESUME_RES   0x40  /* Entry resolved while
						 resuming */
#define SILC_IDLIST_STATUS_NOATTR       0x80  /* Entry does not support
						 attributes in WHOIS */

/*
   Generic ID list data structure.

   This structure is included in all ID list entries and it includes data
   pointers that are common to all ID entries.  This structure is always
   defined to the first field in the ID entries and is used to explicitly
   type cast to this type without first explicitly casting to correct ID
   entry type.  Hence, the ID list entry is type casted to this type to
   get this data from the ID entry (which is usually opaque pointer).

   Note that some of the fields may be NULL.

*/
struct SilcIDListDataObject {
  SilcConnectionType conn_type;      /* Connection type */
  SilcServerConnection sconn;	     /* Connection context */
  SilcSKERekeyMaterial rekey;	     /* Rekey material */
  SilcHash hash;

  /* Public key */
  SilcPublicKey public_key;
  unsigned char fingerprint[20];

  long last_receive;		/* Time last received data */
  long last_sent;		/* Time last sent data */

  unsigned long created;	/* Time when entry was created */

  SilcIDListStatus status;	/* Status mask of the entry */
};

/*
   SILC Server entry object.

   This entry holds information about servers in SILC network. However,
   contents of this entry is highly dependent of what kind of server we are
   (normal server or router server) and whether the entry is used as a local
   list or a global list. These factors dictates the contents of this entry.

   This entry is defined as follows:

   Server type   List type      Contents
   =======================================================================
   server        local list     Server itself
   server        global list    NULL
   router        local list     All servers is the cell
   router        global list    All servers in the SILC network

   Following short description of the fields:

   SilcIDListDataStruct data

       Generic data structure to hold data common to all ID entries.

   char *server_name

       Logical name of the server. There is no limit of the length of the
       server name. This is usually the same name as defined in DNS.

   SilcUInt8 server_type

       Type of the server. SILC_SERVER or SILC_ROUTER are the possible
       choices for this.

   SilcServerID *id

       ID of the server. This includes all the relevant information about
       the server SILC will ever need. These are also the informations
       that is broadcasted between servers and routers in the SILC network.

   char *server_info
   char *motd

       Server info (from INFO command) saved temporarily and motd (from
       MOTD command) saved temporarily.

   SilcServerEntry router

       This is a pointer back to the server list. This is the router server
       where this server is connected to. If this is the router itself and
       it doesn't have a route this is NULL.

   SilcCipher send_key
   SilcCipher receive_key

       Data sending and receiving keys.

   void *connection

       A pointer, usually, to the socket list for fast referencing to
       the data used in connection with this server.  This may be anything
       but as just said, this is usually pointer to the socket connection
       list.

*/
struct SilcServerEntryStruct {
  /* Generic data structure. DO NOT add anything before this! */
  SilcIDListDataStruct data;

  char *server_name;
  SilcUInt8 server_type;
  SilcServerID *id;
  char *server_info;
  char *motd;

  /* Pointer to the router */
  SilcServerEntry router;

  /* Connection data */
  void *connection;

  void *backup_proto;
  unsigned int backup  : 1;	/* Set when executing backup protocol */
};

/*
   SILC Channel Client entry structure.

   This entry used only by the SilcChannelEntry object and it holds
   information about current clients (ie. users) on channel. Following
   short description of the fields:

   SilcClientEntry client

       Pointer to the client list. This is the client currently on channel.

   SilcUInt32 mode

       Client's current mode on the channel.

   SilcChannelEntry channel

       Back pointer back to channel. As this structure is also used by
       SilcClientEntry we have this here for fast access to the channel when
       used by SilcClientEntry.

*/
typedef struct SilcChannelClientEntryStruct {
  SilcClientEntry client;
  SilcUInt32 mode;
  SilcChannelEntry channel;
} *SilcChannelClientEntry;

/*
   SILC Client entry object.

   This entry holds information about connected clients ie. users in the SILC
   network. The contents of this entrt is depended on whether we are normal
   server or router server and whether the list is a local or global list.

   This entry is defined as follows:

   Server type   List type      Contents
   =======================================================================
   server        local list     All clients in server
   server        global list    NULL
   router        local list     All clients in cell
   router        global list    All clients in SILC

   Following short description of the fields:

   SilcIDListDataStruct data

       Generic data structure to hold data common to all ID entries.

   unsigned char *nickname

       The nickname of the client.  This is nickname in original format,
       not casefolded or normalized.  However, it is checked to assure
       that prohibited characters do not exist.  The casefolded version
       is in the ID Cache.

   char *servername

       The name of the server where the client is from. MAy be NULL.

   char username

       Client's usename. This is defined in the following manner:

       Server type   List type      Contents
       ====================================================
       server        local list     User's name
       router        local list     NULL
       router        global list    NULL

       Router doesn't hold this information since it is not vital data
       for the router. If this information is needed by the client it is
       fetched when it is needed.

   char userinfo

       Information about user. This is free information and can be virtually
       anything. This is defined in following manner:

       Server type   List type      Contents
       ====================================================
       server        local list     User's information
       router        local list     NULL
       router        global list    NULL

       Router doesn't hold this information since it is not vital data
       for the router. If this information is needed by the client it is
       fetched when it is needed.

   SilcClientID *id

       ID of the client. This includes all the information SILC will ever
       need. Notice that no nickname of the user is saved anywhere. This is
       beacuse of SilcClientID includes 88 bit hash value of the user's
       nickname which can be used to track down specific user by their
       nickname. Nickname is not relevant information that would need to be
       saved as plain.

   SilcUInt32 mode

       Client's mode.  Client maybe for example server operator or
       router operator (SILC operator).

   long last_command

       Time of last time client executed command. We are strict and will
       not allow any command to be exeucted more than once in about
       2 seconds. This is result of normal time().

   SilcUInt8 fast_command

       Counter to check command bursts.  By default, up to 5 commands
       are allowed before limiting the execution.  See command flags
       for more detail.

   SilcServerEntry router

       This is a pointer to the server list. This is the router server whose
       cell this client is coming from. This is used to route messages to
       this client.

   SilcHashTable channels;

       All the channels this client has joined.  The context saved in the
       hash table shares memory with the channel entrys `user_list' hash
       table.

   void *connection

       A pointer, usually, to the socket list for fast referencing to
       the data used in connection with this client.  This may be anything
       but as just said, this is usually pointer to the socket connection
       list.

   SilcUInt16 resolve_cmd_ident

       Command identifier for the entry when the entry's data.status
       is SILC_IDLIST_STATUS_RESOLVING.  If this entry is asked to be
       resolved when the status is set then the resolver may attach to
       this command identifier and handle the process after the resolving
       is over.

   SilcClientEntry resuming_client

   unsigned locally_detached : 1

       Set to indicate that the client is locally owned but detached for
       purposes of tracking user counts.

*/
struct SilcClientEntryStruct {
  /* Generic data structure. DO NOT add anything before this! */
  SilcIDListDataStruct data;

  unsigned char *nickname;
  char *servername;
  char *username;
  char *userinfo;
  SilcClientID *id;
  SilcUInt32 mode;

  long last_command;
  SilcUInt8 fast_command;

  /* Requested Attributes */
  unsigned char *attrs;
  SilcUInt16 attrs_len;

  /* Pointer to the router */
  SilcServerEntry router;

  /* All channels this client has joined */
  SilcHashTable channels;

  /* Connection data */
  void *connection;

  /* Last time updated/accessed */
  unsigned long updated;

  /* data.status is RESOLVING and this includes the resolving command
     reply identifier. */
  SilcUInt16 resolve_cmd_ident;

  /* we need this so nobody can resume more than once at the same time -
   * server crashes, really odd behaviour, ... */
  SilcClientEntry resuming_client;

  /* Client is locally owned but detached and not counted in the user
   * count for local clients (server->stat.my_clients). */
  unsigned local_detached : 1;
};

/*
   SILC Channel entry object.

   This entry holds information about channels in SILC network. The contents
   of this entry is depended on whether we are normal server or router server
   and whether the list is a local or global list.

   This entry is defined as follows:

   Server type   List type      Contents
   =======================================================================
   server        local list     All channels in server
   server        global list    NULL
   router        local list     All channels in cell
   router        global list    All channels in SILC

   Following short description of the fields:

   char *channel_name

       Logical name of the channel.  This is the original format, not
       the casefolded or normalized.  However, this is checked to assure
       that prohibited characters do not exist.  The casefolded version
       is in the ID Cache.

   SilcUInt32 mode

       Current mode of the channel.  See lib/silccore/silcchannel.h for
       all modes.

   SilcChannelID *id

       ID of the channel. This includes all the information SILC will ever
       need.

   SilcBool global_users

       Boolean value to tell whether there are users outside this server
       on this channel. This is set to TRUE if router sends message to
       the server that there are users outside your server on your
       channel as well. This way server knows that messages needs to be
       sent to the router for further routing. If this is a normal
       server and this channel is not created on this server this field
       is always TRUE. If this server is a router this field is ignored.

   char *topic

       Current topic of the channel.

   char *cipher

       Default cipher of the channel. If this is NULL then server picks
       the cipher to be used. This can be set at SILC_COMMAND_JOIN.

   char *hmac_name

       Default hmac of the channel. If this is NULL then server picks
       the cipher to be used. This can be set at SILC_COMMAND_JOIN.

   SilcPublicKey founder_key

       If the SILC_CMODE_FOUNDER_AUTH has been set then this will include
       the founder's public key.  When the mode and this key is set the
       channel is also permanent channel and cannot be destroyed.

   SilcHashTable user_list

       All users joined on this channel.  Note that the context saved to
       this entry shares memory with the client entrys `channels' hash
       table.

   SilcServerEntry router

       This is a pointer to the server list. This is the router server
       whose cell this channel belongs to. This is used to route messages
       to this channel.

   SilcCipher send_key
   SilcCipher receive_key

       The key of the channel (the cipher actually).

   unsigned char *key
   SilcUInt32 key_len

       Raw key data of the channel key.

   unsigned char iv[SILC_CIPHER_MAX_IV_SIZE]

       Current initial vector. Initial vector is received always along
       with the channel packet. By default this is filled with NULL.

   SilcHmac hmac;

       HMAC of the channel.

   SilcServerChannelRekey rekey

       Channel key re-key context.

*/
struct SilcChannelEntryStruct {
  char *channel_name;
  SilcUInt32 mode;
  SilcChannelID *id;
  char *topic;
  char *cipher;
  char *hmac_name;
  SilcPublicKey founder_key;
  SilcHashTable channel_pubkeys;

  SilcUInt32 user_limit;
  unsigned char *passphrase;
  SilcHashTable invite_list;
  SilcHashTable ban_list;

  /* All users on this channel */
  SilcHashTable user_list;
  SilcUInt32 user_count;

  /* Pointer to the router */
  SilcServerEntry router;

  /* Channel keys */
  SilcCipher send_key;
  SilcCipher receive_key;
  unsigned char *key;
  SilcUInt32 key_len;
  SilcHmac hmac;

  SilcServerChannelRekey rekey;
  unsigned long created;
  unsigned long updated;

  /* Flags */
  unsigned int global_users : 1;
  unsigned int disabled : 1;
  unsigned int users_resolved : 1;
};

/*
   SILC ID List object.

   As for remainder these lists are defined as follows:

   Entry list (cache)  Server type   List type      Contents
   =======================================================================
   servers             server        local list     Server itself
   servers             server        global list    NULL
   servers             router        local list     All servers in cell
   servers             router        global list    All servers in SILC

   clients             server        local list     All clients in server
   clients             server        global list    NULL
   clients             router        local list     All clients in cell
   clients             router        global list    All clients in SILC

   channels            server        local list     All channels in server
   channels            server        global list    NULL
   channels            router        local list     All channels in cell
   channels            router        global list    All channels in SILC

   As seen on the list normal server never defines a global list. This is
   because of normal server don't know anything about anything global data,
   they get it from the router if and when they need it. Routers, on the
   other hand, always define local and global lists because routers really
   know all the relevant data in the SILC network.

   This object is used as local and global list by the server/router.
   Above table shows how this is defined on different conditions.

   This object holds pointers to the ID cache system. Every ID cache entry
   has a specific context pointer to allocated entry (server, client or
   channel entry).

*/
struct SilcIDListStruct {
  SilcIDCache servers;
  SilcIDCache clients;
  SilcIDCache channels;
};

/*
   ID Entry for Unknown connections.

   This is used during authentication phases where we still don't know
   what kind of connection remote connection is, hence, we will use this
   structure instead until we know what type of connection remote end is.

   This is not in any list. This is always individually allocated and
   used as such.

*/
typedef struct {
  /* Generic data structure. DO NOT add anything before this! */
  SilcIDListDataStruct data;
  SilcAsyncOperation op;
  SilcServerConfigRef cconfig;
  SilcServerConfigRef sconfig;
  SilcServerConfigRef rconfig;
  SilcServer server;
  const char *hostname;
  const char *ip;
  SilcUInt16 port;
  SilcConnectionType conn_type;
} *SilcUnknownEntry;

/* Prototypes */
void silc_idlist_add_data(void *entry, SilcIDListData idata);
void silc_idlist_del_data(void *entry);
SILC_TASK_CALLBACK(silc_idlist_purge);
SilcServerEntry
silc_idlist_add_server(SilcIDList id_list,
		       char *server_name, int server_type,
		       SilcServerID *id, SilcServerEntry router,
		       void *connection);
SilcServerEntry
silc_idlist_find_server_by_id(SilcIDList id_list, SilcServerID *id,
			      SilcBool registered, SilcIDCacheEntry *ret_entry);
SilcServerEntry
silc_idlist_find_server_by_name(SilcIDList id_list, char *name,
				SilcBool registered, SilcIDCacheEntry *ret_entry);
SilcServerEntry
silc_idlist_find_server_by_conn(SilcIDList id_list, char *hostname,
				int port, SilcBool registered,
				SilcIDCacheEntry *ret_entry);
SilcServerEntry
silc_idlist_replace_server_id(SilcIDList id_list, SilcServerID *old_id,
			      SilcServerID *new_id);
int silc_idlist_del_server(SilcIDList id_list, SilcServerEntry entry);
void silc_idlist_server_destructor(SilcIDCache cache,
				   SilcIDCacheEntry entry,
				   void *dest_context,
				   void *app_context);
SilcClientEntry
silc_idlist_add_client(SilcIDList id_list, char *nickname, char *username,
		       char *userinfo, SilcClientID *id,
		       SilcServerEntry router, void *connection);
int silc_idlist_del_client(SilcIDList id_list, SilcClientEntry entry);
int silc_idlist_get_clients_by_nickname(SilcIDList id_list, char *nickname,
					char *server,
					SilcClientEntry **clients,
					SilcUInt32 *clients_count);
int silc_idlist_get_clients_by_hash(SilcIDList id_list,
				    char *nickname, char *server,
				    SilcHash md5hash,
				    SilcClientEntry **clients,
				    SilcUInt32 *clients_count);
SilcClientEntry
silc_idlist_find_client_by_id(SilcIDList id_list, SilcClientID *id,
			      SilcBool registered, SilcIDCacheEntry *ret_entry);
SilcClientEntry
silc_idlist_replace_client_id(SilcServer server,
			      SilcIDList id_list, SilcClientID *old_id,
			      SilcClientID *new_id, const char *nickname);
void silc_idlist_client_destructor(SilcIDCache cache,
				   SilcIDCacheEntry entry,
				   void *dest_context,
				   void *app_context);
SilcChannelEntry
silc_idlist_add_channel(SilcIDList id_list, char *channel_name, int mode,
			SilcChannelID *id, SilcServerEntry router,
			SilcCipher send_key, SilcCipher receive_key,
			SilcHmac hmac);
void silc_idlist_channel_destructor(SilcIDCache cache,
				    SilcIDCacheEntry entry,
				    void *dest_context,
				    void *app_context);
int silc_idlist_del_channel(SilcIDList id_list, SilcChannelEntry entry);
SilcChannelEntry
silc_idlist_find_channel_by_name(SilcIDList id_list, char *name,
				 SilcIDCacheEntry *ret_entry);
SilcChannelEntry
silc_idlist_find_channel_by_id(SilcIDList id_list, SilcChannelID *id,
			       SilcIDCacheEntry *ret_entry);
SilcChannelEntry
silc_idlist_replace_channel_id(SilcIDList id_list, SilcChannelID *old_id,
			       SilcChannelID *new_id);
SilcChannelEntry *
silc_idlist_get_channels(SilcIDList id_list, SilcChannelID *channel_id,
			 SilcUInt32 *channels_count);

#endif
