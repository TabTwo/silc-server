/*

  serverconfig.h

  Author: Giovanni Giacobbi <giovanni@giacobbi.net>

  Copyright (C) 1997 - 2007 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

typedef struct SilcServerConfigCipherStruct {
  char *name;
  char *module;
  SilcUInt32 key_length;
  SilcUInt32 block_length;
  struct SilcServerConfigCipherStruct *next;
} SilcServerConfigCipher;

typedef struct SilcServerConfigHashStruct {
  char *name;
  char *module;
  SilcUInt32 block_length;
  SilcUInt32 digest_length;
  struct SilcServerConfigHashStruct *next;
} SilcServerConfigHash;

typedef struct SilcServerConfigHmacStruct {
  char *name;
  char *hash;
  SilcUInt32 mac_length;
  struct SilcServerConfigHmacStruct *next;
} SilcServerConfigHmac;

typedef struct SilcServerConfigPkcsStruct {
  char *name;
  struct SilcServerConfigPkcsStruct *next;
} SilcServerConfigPkcs;

typedef struct SilcServerConfigServerInfoInterfaceStruct {
  char *server_ip;
  char *public_ip;
  SilcUInt16 port;
  struct SilcServerConfigServerInfoInterfaceStruct *next;
} SilcServerConfigServerInfoInterface;

typedef struct SilcServerConfigServerInfoStruct {
  char *server_name;
  SilcServerConfigServerInfoInterface *primary;
  SilcServerConfigServerInfoInterface *secondary;
  char *server_type;	/* E.g. "Test Server" */
  char *location;	/* geographic location */
  char *admin;		/* admin full name */
  char *email;		/* admin's email address */
  char *user;		/* userid the server should be runned at */
  char *group;		/* ditto, but about groupid */
  SilcPublicKey public_key;
  SilcPrivateKey private_key;
  char *motd_file;	/* path to text motd file (reading only) */
  char *pid_file;	/* path to the pid file (for reading and writing) */
} SilcServerConfigServerInfo;

typedef struct SilcServerConfigLoggingStruct {
  char *file;
  SilcUInt32 maxsize;
} SilcServerConfigLogging;

/* Connection parameters */
typedef struct SilcServerConfigConnParams {
  struct SilcServerConfigConnParams *next;
  char *name;
  char *version_protocol;
  char *version_software;
  char *version_software_vendor;
  SilcUInt32 connections_max;
  SilcUInt32 connections_max_per_host;
  SilcUInt32 keepalive_secs;
  SilcUInt32 reconnect_count;
  SilcUInt32 reconnect_interval;
  SilcUInt32 reconnect_interval_max;
  SilcUInt32 key_exchange_rekey;
  SilcUInt32 qos_rate_limit;
  SilcUInt32 qos_bytes_limit;
  SilcUInt32 qos_limit_sec;
  SilcUInt32 qos_limit_usec;
  SilcUInt32 chlimit;
  unsigned int key_exchange_pfs      : 1;
  unsigned int reconnect_keep_trying : 1;
  unsigned int anonymous             : 1;
  unsigned int qos                   : 1;
} SilcServerConfigConnParams;

/* Holds all client authentication data from config file */
typedef struct SilcServerConfigClientStruct {
  char *host;
  unsigned char *passphrase;
  SilcUInt32 passphrase_len;
  SilcBool publickeys;
  SilcServerConfigConnParams *param;
  struct SilcServerConfigClientStruct *next;
} SilcServerConfigClient;

/* Holds all server's administrators authentication data from config file */
typedef struct SilcServerConfigAdminStruct {
  char *host;
  char *user;
  char *nick;
  unsigned char *passphrase;
  SilcUInt32 passphrase_len;
  SilcBool publickeys;
  struct SilcServerConfigAdminStruct *next;
} SilcServerConfigAdmin;

/* Holds all configured denied connections from config file */
typedef struct SilcServerConfigDenyStruct {
  char *host;
  char *reason;
  struct SilcServerConfigDenyStruct *next;
} SilcServerConfigDeny;

/* Holds all configured server connections from config file */
typedef struct SilcServerConfigServerStruct {
  char *host;
  unsigned char *passphrase;
  SilcUInt32 passphrase_len;
  SilcBool publickeys;
  SilcServerConfigConnParams *param;
  SilcBool backup_router;
  struct SilcServerConfigServerStruct *next;
} SilcServerConfigServer;

/* Holds all configured router connections from config file */
typedef struct SilcServerConfigRouterStruct {
  char *host;
  unsigned char *passphrase;
  SilcUInt32 passphrase_len;
  SilcBool publickeys;
  SilcUInt16 port;
  SilcServerConfigConnParams *param;
  SilcBool initiator;
  SilcBool backup_router;
  SilcBool dynamic_connection;
  char *backup_replace_ip;
  SilcUInt16 backup_replace_port;
  SilcBool backup_local;
  struct SilcServerConfigRouterStruct *next;
} SilcServerConfigRouter;

/* define the SilcServerConfig object */
typedef struct {
  SilcServer server;
  void *tmp;

  /* Reference count (when this reaches zero, config object is destroyed) */
  SilcInt32 refcount;

  /* The General section */
  char *module_path;
  SilcBool prefer_passphrase_auth;
  SilcBool require_reverse_lookup;
  SilcUInt32 channel_rekey_secs;
  SilcUInt32 key_exchange_timeout;
  SilcUInt32 conn_auth_timeout;
  SilcServerConfigConnParams param;
  SilcBool detach_disabled;
  SilcUInt32 detach_timeout;
  SilcBool logging_timestamp;
  SilcBool logging_quick;
  long logging_flushdelay;
  char *debug_string;
  SilcBool httpd;
  char *httpd_ip;
  SilcUInt16 httpd_port;
  SilcBool dynamic_server;
  SilcBool local_channels;

  /* Other configuration sections */
  SilcServerConfigCipher *cipher;
  SilcServerConfigHash *hash;
  SilcServerConfigHmac *hmac;
  SilcServerConfigPkcs *pkcs;
  SilcServerConfigLogging *logging_info;
  SilcServerConfigLogging *logging_warnings;
  SilcServerConfigLogging *logging_errors;
  SilcServerConfigLogging *logging_fatals;
  SilcServerConfigServerInfo *server_info;
  SilcServerConfigConnParams *conn_params;
  SilcServerConfigClient *clients;
  SilcServerConfigAdmin *admins;
  SilcServerConfigDeny *denied;
  SilcServerConfigServer *servers;
  SilcServerConfigRouter *routers;
} *SilcServerConfig;

typedef struct {
  SilcServerConfig config;
  void *ref_ptr;
} SilcServerConfigRef;

/* Prototypes */

/* Basic config operations */
SilcServerConfig silc_server_config_alloc(const char *filename,
					  SilcServer server);
void silc_server_config_destroy(SilcServerConfig config);
void silc_server_config_ref(SilcServerConfigRef *ref, SilcServerConfig config,
			    void *ref_ptr);
void silc_server_config_unref(SilcServerConfigRef *ref);

/* Algorithm registering and reset functions */
SilcBool silc_server_config_register_ciphers(SilcServer server);
SilcBool silc_server_config_register_hashfuncs(SilcServer server);
SilcBool silc_server_config_register_hmacs(SilcServer server);
SilcBool silc_server_config_register_pkcs(SilcServer server);
void silc_server_config_setlogfiles(SilcServer server);

/* Run-time config access functions */
SilcServerConfigClient *
silc_server_config_find_client(SilcServer server, char *host);
SilcServerConfigAdmin *
silc_server_config_find_admin(SilcServer server, char *host, char *user,
			      char *nick);
SilcServerConfigDeny *
silc_server_config_find_denied(SilcServer server, char *host);
SilcServerConfigServer *
silc_server_config_find_server_conn(SilcServer server, char *host);
SilcServerConfigRouter *
silc_server_config_find_router_conn(SilcServer server, char *host, int port);
SilcServerConfigRouter *
silc_server_config_find_backup_conn(SilcServer server, char *host);
SilcBool silc_server_config_is_primary_route(SilcServer server);
SilcServerConfigRouter *
silc_server_config_get_primary_router(SilcServer server);
SilcServerConfigRouter *
silc_server_config_get_backup_router(SilcServer server);

#endif	/* !SERVERCONFIG_H */
