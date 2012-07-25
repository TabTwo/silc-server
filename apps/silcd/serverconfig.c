/*

  serverconfig.c

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
/* $Id$ */

#include "serverincludes.h"
#include "server_internal.h"
#include <dirent.h>

#if 0
#define SERVER_CONFIG_DEBUG(fmt) SILC_LOG_DEBUG(fmt)
#else
#define SERVER_CONFIG_DEBUG(fmt)
#endif

/* auto-declare needed variables for the common list parsing */
#define SILC_SERVER_CONFIG_SECTION_INIT(__type__)			\
  SilcServerConfig config = (SilcServerConfig) context;			\
  __type__ *findtmp, *tmp = (__type__ *) config->tmp;			\
  int got_errno = 0

/* allocate the tmp field for fetching data */
#define SILC_SERVER_CONFIG_ALLOCTMP(__type__)				\
  if (!tmp) {								\
    config->tmp = silc_calloc(1, sizeof(*findtmp));			\
    tmp = (__type__ *) config->tmp;					\
  }

/* append the tmp field to the specified list */
#define SILC_SERVER_CONFIG_LIST_APPENDTMP(__list__)			\
  if (!__list__) {							\
    __list__ = tmp;							\
  } else {								\
    for (findtmp = __list__; findtmp->next; findtmp = findtmp->next);	\
    findtmp->next = tmp;						\
  }

/* loops all elements in a list and provides a di struct pointer of the
 * specified type containing the current element */
#define SILC_SERVER_CONFIG_LIST_DESTROY(__type__, __list__)		\
  for (tmp = (void *) __list__; tmp;) {					\
    __type__ *di = (__type__ *) tmp;					\
    tmp = (void *) di->next;

/* Set EDOUBLE error value and bail out if necessary */
#define CONFIG_IS_DOUBLE(__x__)						\
  if ((__x__)) {							\
    got_errno = SILC_CONFIG_EDOUBLE; 					\
    goto got_err;							\
  }

/* Free the authentication fields in the specified struct
 * Expands to two instructions */
#define CONFIG_FREE_AUTH(__section__)			\
  silc_free(__section__->passphrase);

/* Set default values to those parameters that have not been defined */
static void
my_set_param_defaults(SilcServerConfigConnParams *params,
		      SilcServerConfigConnParams *defaults)
{
#define SET_PARAM_DEFAULT(p, d)	params->p =				\
  (params->p ? params->p : (defaults && defaults->p ? defaults->p : d))

  SET_PARAM_DEFAULT(connections_max, SILC_SERVER_MAX_CONNECTIONS);
  SET_PARAM_DEFAULT(connections_max_per_host,
		    SILC_SERVER_MAX_CONNECTIONS_SINGLE);
  SET_PARAM_DEFAULT(keepalive_secs, SILC_SERVER_KEEPALIVE);
  SET_PARAM_DEFAULT(reconnect_count, SILC_SERVER_RETRY_COUNT);
  SET_PARAM_DEFAULT(reconnect_interval, SILC_SERVER_RETRY_INTERVAL_MIN);
  SET_PARAM_DEFAULT(reconnect_interval_max, SILC_SERVER_RETRY_INTERVAL_MAX);
  SET_PARAM_DEFAULT(key_exchange_rekey, SILC_SERVER_REKEY);
  SET_PARAM_DEFAULT(qos_rate_limit, SILC_SERVER_QOS_RATE_LIMIT);
  SET_PARAM_DEFAULT(qos_bytes_limit, SILC_SERVER_QOS_BYTES_LIMIT);
  SET_PARAM_DEFAULT(qos_limit_sec, SILC_SERVER_QOS_LIMIT_SEC);
  SET_PARAM_DEFAULT(qos_limit_usec, SILC_SERVER_QOS_LIMIT_USEC);
  SET_PARAM_DEFAULT(chlimit, SILC_SERVER_CH_JOIN_LIMIT);

#undef SET_PARAM_DEFAULT
}

/* Find connection parameters by the parameter block name. */
static SilcServerConfigConnParams *
my_find_param(SilcServerConfig config, const char *name)
{
  SilcServerConfigConnParams *param;

  for (param = config->conn_params; param; param = param->next) {
    if (!strcasecmp(param->name, name))
      return param;
  }

  SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			 "Cannot find Params \"%s\".", name));

  return NULL;
}

/* SKR find callbcak */

static void my_find_callback(SilcSKR skr, SilcSKRFind find,
			     SilcSKRStatus status, SilcDList keys,
			     void *context)
{
  SilcSKRStatus *s = context;

  *s = status;
  if (keys)
    silc_dlist_uninit(keys);

  silc_skr_find_free(find);
}

/* parse an authdata according to its auth method */
static SilcBool my_parse_authdata(SilcAuthMethod auth_meth, const char *p,
				  void **auth_data, SilcUInt32 *auth_data_len,
				  SilcSKRKeyUsage usage, void *key_context)
{
  if (auth_meth == SILC_AUTH_PASSWORD) {
    /* p is a plain text password */
    if (auth_data && auth_data_len) {
      if (!silc_utf8_valid(p, strlen(p))) {
	*auth_data_len = silc_utf8_encoded_len(p, strlen(p),
					       SILC_STRING_LOCALE);
	*auth_data = silc_calloc(*auth_data_len, sizeof(unsigned char));
	silc_utf8_encode(p, strlen(p), SILC_STRING_LOCALE, *auth_data,
			 *auth_data_len);
      } else {
	*auth_data = (void *) strdup(p);
	*auth_data_len = (SilcUInt32) strlen(p);
      }
    }
  } else if (auth_meth == SILC_AUTH_PUBLIC_KEY) {
    /* p is a public key file name */
    SilcPublicKey public_key;
    SilcSKR skr = *auth_data;
    SilcSKRFind find;
    SilcSKRStatus status = SILC_SKR_NOT_FOUND;

    if (!silc_pkcs_load_public_key(p, &public_key)) {
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			     "Could not load public key file!"));
      return FALSE;
    }

    find = silc_skr_find_alloc();
    silc_skr_find_set_public_key(find, public_key);
    silc_skr_find_set_usage(find, usage);
    if (!key_context)
      silc_skr_find_set_context(find, SILC_32_TO_PTR(usage));
    silc_skr_find(skr, NULL, find, my_find_callback, &status);
    if (status == SILC_SKR_OK) {
      /* Already added, ignore error */
      silc_pkcs_public_key_free(public_key);
      return TRUE;
    }

    /* Add the public key to repository */
    status = silc_skr_add_public_key(skr, public_key, usage,
				     key_context ? key_context :
				     (void *)usage, NULL);
    if (status != SILC_SKR_OK) {
      SILC_SERVER_LOG_ERROR(("Error while adding public key \"%s\"", p));
      return FALSE;
    }
  }

  return TRUE;
}

static int my_parse_publickeydir(const char *dirname, void **auth_data,
				 SilcSKRKeyUsage usage)
{
  int total = 0;
  struct dirent *get_file;
  DIR *dp;

  if (!(dp = opendir(dirname))) {
    SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			   "Could not open directory \"%s\"", dirname));
    return -1;
  }

  /* errors are not considered fatal */
  while ((get_file = readdir(dp))) {
    const char *filename = get_file->d_name;
    char buf[1024];
    int dirname_len = strlen(dirname), filename_len = strlen(filename);
    struct stat check_file;

    /* Ignore "." and "..", and take files only with ".pub" suffix. */
    if (!strcmp(filename, ".") || !strcmp(filename, "..") ||
	(filename_len < 5) || strcmp(filename + filename_len - 4, ".pub"))
      continue;

    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf) - 1, "%s%s%s", dirname,
	     (dirname[dirname_len - 1] == '/' ? "" : "/"), filename);

    if (stat(buf, &check_file) < 0) {
      SILC_SERVER_LOG_ERROR(("Error stating file %s: %s", buf,
			     strerror(errno)));
    } else if (S_ISREG(check_file.st_mode)) {
      if (my_parse_authdata(SILC_AUTH_PUBLIC_KEY, buf, auth_data, NULL,
			    usage, NULL))
	total++;
    }
  }

  SILC_LOG_DEBUG(("Tried to load %d public keys in \"%s\"", total, dirname));
  return total;
}

/* Callbacks */

SILC_CONFIG_CALLBACK(fetch_generic)
{
  SilcServerConfig config = (SilcServerConfig) context;
  int got_errno = 0;

  if (!strcmp(name, "prefer_passphrase_auth")) {
    config->prefer_passphrase_auth = *(SilcBool *)val;
  }
  else if (!strcmp(name, "require_reverse_lookup")) {
    config->require_reverse_lookup = *(SilcBool *)val;
  }
  else if (!strcmp(name, "connections_max")) {
    config->param.connections_max = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "connections_max_per_host")) {
    config->param.connections_max_per_host = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "keepalive_secs")) {
    config->param.keepalive_secs = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "reconnect_count")) {
    config->param.reconnect_count = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "reconnect_interval")) {
    config->param.reconnect_interval = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "reconnect_interval_max")) {
    config->param.reconnect_interval_max = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "reconnect_keep_trying")) {
    config->param.reconnect_keep_trying = *(SilcBool *)val;
  }
  else if (!strcmp(name, "key_exchange_rekey")) {
    config->param.key_exchange_rekey = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "key_exchange_pfs")) {
    config->param.key_exchange_pfs = *(SilcBool *)val;
  }
  else if (!strcmp(name, "channel_rekey_secs")) {
    config->channel_rekey_secs = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "key_exchange_timeout")) {
    config->key_exchange_timeout = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "conn_auth_timeout")) {
    config->conn_auth_timeout = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "version_protocol")) {
    CONFIG_IS_DOUBLE(config->param.version_protocol);
    config->param.version_protocol =
      (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "version_software")) {
    CONFIG_IS_DOUBLE(config->param.version_software);
    config->param.version_software =
      (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "version_software_vendor")) {
    CONFIG_IS_DOUBLE(config->param.version_software_vendor);;
    config->param.version_software_vendor =
      (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "detach_disabled")) {
    config->detach_disabled = *(SilcBool *)val;
  }
  else if (!strcmp(name, "detach_timeout")) {
    config->detach_timeout = (SilcUInt32) *(int *)val;
  }
  else if (!strcmp(name, "qos")) {
    config->param.qos = *(SilcBool *)val;
  }
  else if (!strcmp(name, "qos_rate_limit")) {
    config->param.qos_rate_limit = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_bytes_limit")) {
    config->param.qos_bytes_limit = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_limit_sec")) {
    config->param.qos_limit_sec = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_limit_usec")) {
    config->param.qos_limit_usec = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "channel_join_limit")) {
    config->param.chlimit = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "debug_string")) {
    CONFIG_IS_DOUBLE(config->debug_string);
    config->debug_string = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "http_server")) {
    config->httpd = *(SilcBool *)val;
  }
  else if (!strcmp(name, "http_server_ip")) {
    CONFIG_IS_DOUBLE(config->httpd_ip);
    config->httpd_ip = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "http_server_port")) {
    int port = *(int *)val;
    if ((port <= 0) || (port > 65535)) {
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			     "Invalid port number!"));
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    config->httpd_port = (SilcUInt16)port;
  }
  else if (!strcmp(name, "dynamic_server")) {
    config->dynamic_server = *(SilcBool *)val;
  }
  else if (!strcmp(name, "local_channels")) {
    config->local_channels = *(SilcBool *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;

  return SILC_CONFIG_OK;

 got_err:
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_cipher)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigCipher);

  SERVER_CONFIG_DEBUG(("Received CIPHER type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->name) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->cipher);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigCipher);

  /* Identify and save this value */
  if (!strcmp(name, "name")) {
    CONFIG_IS_DOUBLE(tmp->name);
    tmp->name = strdup((char *) val);
  }
  else if (!strcmp(name, "keylength")) {
    tmp->key_length = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "blocklength")) {
    tmp->block_length = *(SilcUInt32 *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->name);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_hash)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigHash);

  SERVER_CONFIG_DEBUG(("Received HASH type=%d name=%s (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->name || (tmp->block_length == 0) || (tmp->digest_length == 0)) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->hash);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigHash);

  /* Identify and save this value */
  if (!strcmp(name, "name")) {
    CONFIG_IS_DOUBLE(tmp->name);
    tmp->name = strdup((char *) val);
  }
  else if (!strcmp(name, "blocklength")) {
    tmp->block_length = *(int *)val;
  }
  else if (!strcmp(name, "digestlength")) {
    tmp->digest_length = *(int *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->name);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_hmac)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigHmac);

  SERVER_CONFIG_DEBUG(("Received HMAC type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->name || !tmp->hash || (tmp->mac_length == 0)) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->hmac);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigHmac);

  /* Identify and save this value */
  if (!strcmp(name, "name")) {
    CONFIG_IS_DOUBLE(tmp->name);
    tmp->name = strdup((char *) val);
  }
  else if (!strcmp(name, "hash")) {
    CONFIG_IS_DOUBLE(tmp->hash);
    tmp->hash = strdup((char *) val);
  }
  else if (!strcmp(name, "maclength")) {
    tmp->mac_length = *(int *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->name);
  silc_free(tmp->hash);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_pkcs)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigPkcs);

  SERVER_CONFIG_DEBUG(("Received PKCS type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* Check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->name) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->pkcs);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigPkcs);

  /* Identify and save this value */
  if (!strcmp(name, "name")) {
    CONFIG_IS_DOUBLE(tmp->name);
    tmp->name = strdup((char *) val);
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->name);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_serverinfo)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigServerInfoInterface);
  SilcServerConfigServerInfo *server_info = config->server_info;

  SERVER_CONFIG_DEBUG(("Received SERVERINFO type=%d name=\"%s\" (val=%x)",
		       type, name, context));

  /* If there isn't the main struct alloc it */
  if (!server_info)
    config->server_info = server_info = (SilcServerConfigServerInfo *)
		silc_calloc(1, sizeof(*server_info));

  if (type == SILC_CONFIG_ARG_BLOCK) {
    if (!strcmp(name, "primary")) {
      if (server_info->primary) {
	SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			       "Double primary specification."));
	got_errno = SILC_CONFIG_EPRINTLINE;
	goto got_err;
      }
      CONFIG_IS_DOUBLE(server_info->primary);

      /* now check the temporary struct, don't accept empty block and
         make sure all fields are there */
      if (!tmp || !tmp->server_ip || !tmp->port) {
	got_errno = SILC_CONFIG_EMISSFIELDS;
	goto got_err;
      }
      server_info->primary = tmp;
      config->tmp = NULL;
      return SILC_CONFIG_OK;
    } else if (!strcmp(name, "secondary")) {
      if (!tmp)
	return SILC_CONFIG_OK;
      if (!tmp || !tmp->server_ip || !tmp->port) {
	got_errno = SILC_CONFIG_EMISSFIELDS;
	goto got_err;
      }
      SILC_SERVER_CONFIG_LIST_APPENDTMP(server_info->secondary);
      config->tmp = NULL;
      return SILC_CONFIG_OK;
    } else if (!server_info->public_key || !server_info->private_key) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }
    return SILC_CONFIG_OK;
  }
  if (!strcmp(name, "hostname")) {
    CONFIG_IS_DOUBLE(server_info->server_name);
    server_info->server_name = strdup((char *) val);
  }
  else if (!strcmp(name, "ip")) {
    SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigServerInfoInterface);
    CONFIG_IS_DOUBLE(tmp->server_ip);
    tmp->server_ip = strdup((char *) val);
  }
  else if (!strcmp(name, "public_ip")) {
    SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigServerInfoInterface);
    CONFIG_IS_DOUBLE(tmp->public_ip);
    tmp->public_ip = strdup((char *) val);
  }
  else if (!strcmp(name, "port")) {
    int port = *(int *)val;
    SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigServerInfoInterface);
    if ((port <= 0) || (port > 65535)) {
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			     "Invalid port number!"));
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->port = (SilcUInt16) port;
  }
  else if (!strcmp(name, "servertype")) {
    CONFIG_IS_DOUBLE(server_info->server_type);
    server_info->server_type = strdup((char *) val);
  }
  else if (!strcmp(name, "admin")) {
    CONFIG_IS_DOUBLE(server_info->admin);
    server_info->admin = strdup((char *) val);
  }
  else if (!strcmp(name, "adminemail")) {
    CONFIG_IS_DOUBLE(server_info->email);
    server_info->email = strdup((char *) val);
  }
  else if (!strcmp(name, "location")) {
    CONFIG_IS_DOUBLE(server_info->location);
    server_info->location = strdup((char *) val);
  }
  else if (!strcmp(name, "user")) {
    CONFIG_IS_DOUBLE(server_info->user);
    server_info->user = strdup((char *) val);
  }
  else if (!strcmp(name, "group")) {
    CONFIG_IS_DOUBLE(server_info->group);
    server_info->group = strdup((char *) val);
  }
  else if (!strcmp(name, "motdfile")) {
    CONFIG_IS_DOUBLE(server_info->motd_file);
    server_info->motd_file = strdup((char *) val);
  }
  else if (!strcmp(name, "pidfile")) {
    CONFIG_IS_DOUBLE(server_info->pid_file);
    server_info->pid_file = strdup((char *) val);
  }
  else if (!strcmp(name, "publickey")) {
    char *file_tmp = (char *) val;
    CONFIG_IS_DOUBLE(server_info->public_key);

    /* Try to load specified file, if fail stop config parsing */
    if (!silc_pkcs_load_public_key(file_tmp, &server_info->public_key)) {
      SILC_SERVER_LOG_ERROR(("Error: Could not load public key file."));
      return SILC_CONFIG_EPRINTLINE;
    }
  }
  else if (!strcmp(name, "privatekey")) {
    struct stat st;
    char *file_tmp = (char *) val;
    CONFIG_IS_DOUBLE(server_info->private_key);

    /* Check the private key file permissions. */
    if ((stat(file_tmp, &st)) != -1) {
      if (((st.st_mode & 0777) != 0600) &&
	  ((st.st_mode & 0777) != 0640)) {
	SILC_SERVER_LOG_ERROR(("Wrong permissions in private key "
			      "file \"%s\".  The permissions must be "
			      "0600 or 0640.", file_tmp));
        return SILC_CONFIG_ESILENT;
      }
    }

    /* Try to load specified file, if fail stop config parsing */
    if (!silc_pkcs_load_private_key(file_tmp, "", 0,
				    &server_info->private_key)) {
      SILC_SERVER_LOG_ERROR(("Error: Could not load private key file."));
      return SILC_CONFIG_EPRINTLINE;
    }
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  /* Here we need to check if tmp exists because this function handles
   * misc data (multiple fields and single-only fields) */
  if (tmp) {
    silc_free(tmp->server_ip);
    silc_free(tmp);
    config->tmp = NULL;
  }
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_logging)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigLogging);

  if (!strcmp(name, "timestamp")) {
    config->logging_timestamp = *(SilcBool *)val;
  }
  else if (!strcmp(name, "quicklogs")) {
    config->logging_quick = *(SilcBool *)val;
  }
  else if (!strcmp(name, "flushdelay")) {
    int flushdelay = *(int *)val;
    if (flushdelay < 2) { /* this value was taken from silclog.h (min delay) */
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			    "Invalid flushdelay value, use quicklogs if you "
			    "want real-time logging."));
      return SILC_CONFIG_EPRINTLINE;
    }
    config->logging_flushdelay = (long) flushdelay;
  }

  /* The following istances happens only in Logging's sub-blocks, a match
     for the sub-block name means that you should store the filename/maxsize
     temporary struct to the proper logging channel.
     If we get a match for "file" or "maxsize" this means that we are inside
     a sub-sub-block and it is safe to alloc a new tmp. */
#define FETCH_LOGGING_CHAN(__chan__, __member__)		\
  else if (!strcmp(name, __chan__)) {				\
    if (!tmp) return SILC_CONFIG_OK;				\
    if (!tmp->file) {						\
      got_errno = SILC_CONFIG_EMISSFIELDS; goto got_err;	\
    }								\
    config->__member__ = tmp;					\
    config->tmp = NULL;						\
  }
  FETCH_LOGGING_CHAN("info", logging_info)
  FETCH_LOGGING_CHAN("warnings", logging_warnings)
  FETCH_LOGGING_CHAN("errors", logging_errors)
  FETCH_LOGGING_CHAN("fatals", logging_fatals)
#undef FETCH_LOGGING_CHAN
  else if (!strcmp(name, "file")) {
    SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigLogging);
    CONFIG_IS_DOUBLE(tmp->file);
    tmp->file = strdup((char *) val);
  }
  else if (!strcmp(name, "size")) {
    if (!tmp) {
      config->tmp = silc_calloc(1, sizeof(*tmp));
      tmp = (SilcServerConfigLogging *) config->tmp;
    }
    tmp->maxsize = *(SilcUInt32 *) val;
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->file);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_connparam)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigConnParams);

  SERVER_CONFIG_DEBUG(("Received CONNPARAM type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->name) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }
    /* Set defaults */
    my_set_param_defaults(tmp, &config->param);

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->conn_params);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  if (!tmp) {
    SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigConnParams);
    tmp->reconnect_keep_trying = TRUE;
  }

  if (!strcmp(name, "name")) {
    CONFIG_IS_DOUBLE(tmp->name);
    tmp->name = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "connections_max")) {
    tmp->connections_max = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "connections_max_per_host")) {
    tmp->connections_max_per_host = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "keepalive_secs")) {
    tmp->keepalive_secs = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "reconnect_count")) {
    tmp->reconnect_count = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "reconnect_interval")) {
    tmp->reconnect_interval = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "reconnect_interval_max")) {
    tmp->reconnect_interval_max = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "reconnect_keep_trying")) {
    tmp->reconnect_keep_trying = *(SilcBool *)val;
  }
  else if (!strcmp(name, "key_exchange_rekey")) {
    tmp->key_exchange_rekey = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "key_exchange_pfs")) {
    tmp->key_exchange_pfs = *(SilcBool *)val;
  }
  else if (!strcmp(name, "version_protocol")) {
    CONFIG_IS_DOUBLE(tmp->version_protocol);
    tmp->version_protocol = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "version_software")) {
    CONFIG_IS_DOUBLE(tmp->version_software);
    tmp->version_software = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "version_software_vendor")) {
    CONFIG_IS_DOUBLE(tmp->version_software_vendor);;
    tmp->version_software_vendor =
      (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "anonymous")) {
    tmp->anonymous = *(SilcBool *)val;
  }
  else if (!strcmp(name, "qos")) {
    tmp->qos = *(SilcBool *)val;
  }
  else if (!strcmp(name, "qos_rate_limit")) {
    tmp->qos_rate_limit = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_bytes_limit")) {
    tmp->qos_bytes_limit = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_limit_sec")) {
    tmp->qos_limit_sec = *(SilcUInt32 *)val;
  }
  else if (!strcmp(name, "qos_limit_usec")) {
    tmp->qos_limit_usec = *(SilcUInt32 *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;

  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->name);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_client)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigClient);

  SERVER_CONFIG_DEBUG(("Received CLIENT type=%d name=\"%s\" (val=%x)",
		       type, name, context));

  /* Alloc before block checking, because empty sub-blocks are welcome here */
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigClient);

  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* empty sub-blocks are welcome */
    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->clients);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }

  /* Identify and save this value */
  if (!strcmp(name, "host")) {
    CONFIG_IS_DOUBLE(tmp->host);
    tmp->host = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "passphrase")) {
    CONFIG_IS_DOUBLE(tmp->passphrase);
    if (!my_parse_authdata(SILC_AUTH_PASSWORD, (char *) val,
			   (void *)&tmp->passphrase,
			   &tmp->passphrase_len, 0, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "publickey")) {
    if (!my_parse_authdata(SILC_AUTH_PUBLIC_KEY, (char *) val,
			   (void *)&config->server->repository, NULL,
			   SILC_SKR_USAGE_AUTH |
			   SILC_SKR_USAGE_KEY_AGREEMENT, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->publickeys = TRUE;
  }
  else if (!strcmp(name, "publickeydir")) {
    if (my_parse_publickeydir((char *) val,
			      (void *)&config->server->repository,
			      SILC_SKR_USAGE_AUTH |
			      SILC_SKR_USAGE_KEY_AGREEMENT) < 0) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->publickeys = TRUE;
  }
  else if (!strcmp(name, "params")) {
    CONFIG_IS_DOUBLE(tmp->param);
    tmp->param = my_find_param(config, (char *) val);
    if (!tmp->param) { /* error message already output */
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->host);
  CONFIG_FREE_AUTH(tmp);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_admin)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigAdmin);

  SERVER_CONFIG_DEBUG(("Received CLIENT type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->admins);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigAdmin);

  /* Identify and save this value */
  if (!strcmp(name, "host")) {
    CONFIG_IS_DOUBLE(tmp->host);
    tmp->host = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "user")) {
    CONFIG_IS_DOUBLE(tmp->user);
    tmp->user = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "nick")) {
    CONFIG_IS_DOUBLE(tmp->nick);
    tmp->nick = (*(char *)val ? strdup((char *) val) : NULL);
  }
  else if (!strcmp(name, "passphrase")) {
    CONFIG_IS_DOUBLE(tmp->passphrase);
    if (!my_parse_authdata(SILC_AUTH_PASSWORD, (char *) val,
			   (void *)&tmp->passphrase,
			   &tmp->passphrase_len, 0, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "publickey")) {
    if (!my_parse_authdata(SILC_AUTH_PUBLIC_KEY, (char *) val,
			   (void *)&config->server->repository, NULL,
			   SILC_SKR_USAGE_SERVICE_AUTHORIZATION, tmp)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->publickeys = TRUE;
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->host);
  silc_free(tmp->user);
  silc_free(tmp->nick);
  CONFIG_FREE_AUTH(tmp);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_deny)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigDeny);

  SERVER_CONFIG_DEBUG(("Received DENY type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->reason) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->denied);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigDeny);

  /* Identify and save this value */
  if (!strcmp(name, "host")) {
    CONFIG_IS_DOUBLE(tmp->host);
    tmp->host = (*(char *)val ? strdup((char *) val) : strdup("*"));
  }
  else if (!strcmp(name, "reason")) {
    CONFIG_IS_DOUBLE(tmp->reason);
    tmp->reason = strdup((char *) val);
  }
  else
    return SILC_CONFIG_EINTERNAL;
  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->host);
  silc_free(tmp->reason);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_server)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigServer);

  SERVER_CONFIG_DEBUG(("Received SERVER type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    /* check the temporary struct's fields */
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->host) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    /* the temporary struct is ok, append it to the list */
    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->servers);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigServer);

  /* Identify and save this value */
  if (!strcmp(name, "host")) {
    CONFIG_IS_DOUBLE(tmp->host);
    tmp->host = (*(char *)val ? strdup((char *) val) : strdup("*"));
  }
  else if (!strcmp(name, "passphrase")) {
    CONFIG_IS_DOUBLE(tmp->passphrase);
    if (!my_parse_authdata(SILC_AUTH_PASSWORD, (char *) val,
			   (void *)&tmp->passphrase,
			   &tmp->passphrase_len, 0, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "publickey")) {
    CONFIG_IS_DOUBLE(tmp->publickeys);
    if (!my_parse_authdata(SILC_AUTH_PUBLIC_KEY, (char *) val,
			   (void *)&config->server->repository, NULL,
			   SILC_SKR_USAGE_AUTH |
			   SILC_SKR_USAGE_KEY_AGREEMENT, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->publickeys = TRUE;
  }
  else if (!strcmp(name, "params")) {
    CONFIG_IS_DOUBLE(tmp->param);
    tmp->param = my_find_param(config, (char *) val);
    if (!tmp->param) { /* error message already output */
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "backup")) {
    tmp->backup_router = *(SilcBool *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;

  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->host);
  CONFIG_FREE_AUTH(tmp);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

SILC_CONFIG_CALLBACK(fetch_router)
{
  SILC_SERVER_CONFIG_SECTION_INIT(SilcServerConfigRouter);

  SERVER_CONFIG_DEBUG(("Received ROUTER type=%d name=\"%s\" (val=%x)",
		       type, name, context));
  if (type == SILC_CONFIG_ARG_BLOCK) {
    if (!tmp) /* discard empty sub-blocks */
      return SILC_CONFIG_OK;
    if (!tmp->host) {
      got_errno = SILC_CONFIG_EMISSFIELDS;
      goto got_err;
    }

    SILC_SERVER_CONFIG_LIST_APPENDTMP(config->routers);
    config->tmp = NULL;
    return SILC_CONFIG_OK;
  }
  SILC_SERVER_CONFIG_ALLOCTMP(SilcServerConfigRouter);

  /* Identify and save this value */
  if (!strcmp(name, "host")) {
    CONFIG_IS_DOUBLE(tmp->host);
    tmp->host = strdup((char *) val);
  }
  else if (!strcmp(name, "port")) {
    int port = *(int *)val;
    if ((port <= 0) || (port > 65535)) {
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			     "Invalid port number!"));
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->port = (SilcUInt16) port;
  }
  else if (!strcmp(name, "passphrase")) {
    CONFIG_IS_DOUBLE(tmp->passphrase);
    if (!my_parse_authdata(SILC_AUTH_PASSWORD, (char *) val,
			   (void *)&tmp->passphrase,
			   &tmp->passphrase_len, 0, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "publickey")) {
    CONFIG_IS_DOUBLE(tmp->publickeys);
    if (!my_parse_authdata(SILC_AUTH_PUBLIC_KEY, (char *) val,
			   (void *)&config->server->repository, NULL,
			   SILC_SKR_USAGE_AUTH |
			   SILC_SKR_USAGE_KEY_AGREEMENT, NULL)) {
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->publickeys = TRUE;
  }
  else if (!strcmp(name, "params")) {
    CONFIG_IS_DOUBLE(tmp->param);
    tmp->param = my_find_param(config, (char *) val);
    if (!tmp->param) { /* error message already output */
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
  }
  else if (!strcmp(name, "initiator")) {
    tmp->initiator = *(SilcBool *)val;
  }
  else if (!strcmp(name, "backuphost")) {
    CONFIG_IS_DOUBLE(tmp->backup_replace_ip);
    tmp->backup_replace_ip = (*(char *)val ? strdup((char *) val) :
			      strdup("*"));
    tmp->backup_router = TRUE;
  }
  else if (!strcmp(name, "backupport")) {
    int port = *(int *)val;
    if ((port <= 0) || (port > 65535)) {
      SILC_SERVER_LOG_ERROR(("Error while parsing config file: "
			     "Invalid port number!"));
      got_errno = SILC_CONFIG_EPRINTLINE;
      goto got_err;
    }
    tmp->backup_replace_port = (SilcUInt16) port;
  }
  else if (!strcmp(name, "backuplocal")) {
    tmp->backup_local = *(SilcBool *)val;
  }
  else if (!strcmp(name, "dynamic_connection")) {
    tmp->dynamic_connection = *(SilcBool *)val;
  }
  else
    return SILC_CONFIG_EINTERNAL;

  return SILC_CONFIG_OK;

 got_err:
  silc_free(tmp->host);
  silc_free(tmp->backup_replace_ip);
  CONFIG_FREE_AUTH(tmp);
  silc_free(tmp);
  config->tmp = NULL;
  return got_errno;
}

/* known config options tables */
static const SilcConfigTable table_general[] = {
  { "prefer_passphrase_auth",	SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "require_reverse_lookup",	SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "connections_max",		SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "connections_max_per_host", SILC_CONFIG_ARG_INT,    fetch_generic,	NULL },
  { "keepalive_secs",		SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "reconnect_count",		SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "reconnect_interval",      	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "reconnect_interval_max",   SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "reconnect_keep_trying",	SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "key_exchange_rekey",	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "key_exchange_pfs",		SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "channel_rekey_secs",	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "key_exchange_timeout",   	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "conn_auth_timeout",   	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "version_protocol",	        SILC_CONFIG_ARG_STR,	fetch_generic,	NULL },
  { "version_software",		SILC_CONFIG_ARG_STR,	fetch_generic,	NULL },
  { "version_software_vendor",	SILC_CONFIG_ARG_STR,	fetch_generic,	NULL },
  { "detach_disabled",    	SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "detach_timeout",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "qos",    	                SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "qos_rate_limit",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "qos_bytes_limit",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "qos_limit_sec",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "qos_limit_usec",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "channel_join_limit",    	SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "debug_string",    		SILC_CONFIG_ARG_STR,	fetch_generic,	NULL },
  { "http_server",    		SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "http_server_ip",  		SILC_CONFIG_ARG_STRE,	fetch_generic,	NULL },
  { "http_server_port",		SILC_CONFIG_ARG_INT,	fetch_generic,	NULL },
  { "dynamic_server",  		SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { "local_channels",	        SILC_CONFIG_ARG_TOGGLE,	fetch_generic,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_cipher[] = {
  { "name",		SILC_CONFIG_ARG_STR,	fetch_cipher,	NULL },
  { "keylength",	SILC_CONFIG_ARG_INT,	fetch_cipher,	NULL },
  { "blocklength",	SILC_CONFIG_ARG_INT,	fetch_cipher,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_hash[] = {
  { "name",		SILC_CONFIG_ARG_STR,	fetch_hash,	NULL },
  { "blocklength",	SILC_CONFIG_ARG_INT,	fetch_hash,	NULL },
  { "digestlength",	SILC_CONFIG_ARG_INT,	fetch_hash,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_hmac[] = {
  { "name",		SILC_CONFIG_ARG_STR,	fetch_hmac,	NULL },
  { "hash",		SILC_CONFIG_ARG_STR,	fetch_hmac,	NULL },
  { "maclength",	SILC_CONFIG_ARG_INT,	fetch_hmac,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_pkcs[] = {
  { "name",		SILC_CONFIG_ARG_STR,	fetch_pkcs,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_serverinfo_c[] = {
  { "ip",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "public_ip",	SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "port",		SILC_CONFIG_ARG_INT,	fetch_serverinfo, NULL},
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_serverinfo[] = {
  { "hostname",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "primary",		SILC_CONFIG_ARG_BLOCK,	fetch_serverinfo, table_serverinfo_c},
  { "secondary",	SILC_CONFIG_ARG_BLOCK,	fetch_serverinfo, table_serverinfo_c},
  { "servertype",	SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "location",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "admin",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "adminemail",	SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "user",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "group",		SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "publickey",	SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "privatekey",	SILC_CONFIG_ARG_STR,	fetch_serverinfo, NULL},
  { "motdfile",		SILC_CONFIG_ARG_STRE,	fetch_serverinfo, NULL},
  { "pidfile",		SILC_CONFIG_ARG_STRE,	fetch_serverinfo, NULL},
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_logging_c[] = {
  { "file",		SILC_CONFIG_ARG_STR,	fetch_logging,	NULL },
  { "size",		SILC_CONFIG_ARG_SIZE,	fetch_logging,	NULL },
/*{ "quicklog",		SILC_CONFIG_ARG_NONE,	fetch_logging,	NULL }, */
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_logging[] = {
  { "timestamp",	SILC_CONFIG_ARG_TOGGLE,	fetch_logging,	NULL },
  { "quicklogs",	SILC_CONFIG_ARG_TOGGLE,	fetch_logging,	NULL },
  { "flushdelay",	SILC_CONFIG_ARG_INT,	fetch_logging,	NULL },
  { "info",		SILC_CONFIG_ARG_BLOCK,	fetch_logging,	table_logging_c },
  { "warnings",		SILC_CONFIG_ARG_BLOCK,	fetch_logging,	table_logging_c },
  { "errors",		SILC_CONFIG_ARG_BLOCK,	fetch_logging,	table_logging_c },
  { "fatals",		SILC_CONFIG_ARG_BLOCK,	fetch_logging,	table_logging_c },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_connparam[] = {
  { "name",		       SILC_CONFIG_ARG_STR,    fetch_connparam, NULL },
  { "require_reverse_lookup",  SILC_CONFIG_ARG_TOGGLE, fetch_connparam,	NULL },
  { "connections_max",	       SILC_CONFIG_ARG_INT,    fetch_connparam, NULL },
  { "connections_max_per_host",SILC_CONFIG_ARG_INT,    fetch_connparam, NULL },
  { "keepalive_secs",	       SILC_CONFIG_ARG_INT,    fetch_connparam, NULL },
  { "reconnect_count",	       SILC_CONFIG_ARG_INT,    fetch_connparam, NULL },
  { "reconnect_interval",      SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "reconnect_interval_max",  SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "reconnect_keep_trying",   SILC_CONFIG_ARG_TOGGLE, fetch_connparam,	NULL },
  { "key_exchange_rekey",      SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "key_exchange_pfs",	       SILC_CONFIG_ARG_TOGGLE, fetch_connparam,	NULL },
  { "version_protocol",	       SILC_CONFIG_ARG_STR,    fetch_connparam,	NULL },
  { "version_software",	       SILC_CONFIG_ARG_STR,    fetch_connparam,	NULL },
  { "version_software_vendor", SILC_CONFIG_ARG_STR,    fetch_connparam,	NULL },
  { "anonymous",               SILC_CONFIG_ARG_TOGGLE, fetch_connparam,	NULL },
  { "qos",    	               SILC_CONFIG_ARG_TOGGLE, fetch_connparam,	NULL },
  { "qos_rate_limit",          SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "qos_bytes_limit",         SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "qos_limit_sec",           SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { "qos_limit_usec",          SILC_CONFIG_ARG_INT,    fetch_connparam,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_client[] = {
  { "host",		SILC_CONFIG_ARG_STRE,	fetch_client,	NULL },
  { "passphrase",	SILC_CONFIG_ARG_STR,	fetch_client,	NULL },
  { "publickey",	SILC_CONFIG_ARG_STR,	fetch_client,	NULL },
  { "publickeydir",	SILC_CONFIG_ARG_STR,	fetch_client,	NULL },
  { "params",		SILC_CONFIG_ARG_STR,	fetch_client,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_admin[] = {
  { "host",		SILC_CONFIG_ARG_STRE,	fetch_admin,	NULL },
  { "user",		SILC_CONFIG_ARG_STRE,	fetch_admin,	NULL },
  { "nick",		SILC_CONFIG_ARG_STRE,	fetch_admin,	NULL },
  { "passphrase",	SILC_CONFIG_ARG_STR,	fetch_admin,	NULL },
  { "publickey",	SILC_CONFIG_ARG_STR,	fetch_admin,	NULL },
  { "port",		SILC_CONFIG_ARG_INT,	fetch_admin,	NULL },
  { "params",		SILC_CONFIG_ARG_STR,	fetch_admin,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_deny[] = {
  { "host",		SILC_CONFIG_ARG_STRE,	fetch_deny,	NULL },
  { "reason",		SILC_CONFIG_ARG_STR,	fetch_deny,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_serverconn[] = {
  { "host",		SILC_CONFIG_ARG_STRE,	fetch_server,	NULL },
  { "passphrase",	SILC_CONFIG_ARG_STR,	fetch_server,	NULL },
  { "publickey",	SILC_CONFIG_ARG_STR,	fetch_server,	NULL },
  { "params",		SILC_CONFIG_ARG_STR,	fetch_server,	NULL },
  { "backup",		SILC_CONFIG_ARG_TOGGLE,	fetch_server,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_routerconn[] = {
  { "host",		SILC_CONFIG_ARG_STRE,	fetch_router,	NULL },
  { "port",		SILC_CONFIG_ARG_INT,	fetch_router,	NULL },
  { "passphrase",	SILC_CONFIG_ARG_STR,	fetch_router,	NULL },
  { "publickey",	SILC_CONFIG_ARG_STR,	fetch_router,	NULL },
  { "params",		SILC_CONFIG_ARG_STR,	fetch_router,	NULL },
  { "initiator",	SILC_CONFIG_ARG_TOGGLE,	fetch_router,	NULL },
  { "backuphost",	SILC_CONFIG_ARG_STRE,	fetch_router,	NULL },
  { "backupport",	SILC_CONFIG_ARG_INT,	fetch_router,	NULL },
  { "backuplocal",	SILC_CONFIG_ARG_TOGGLE,	fetch_router,	NULL },
  { "dynamic_connection",	SILC_CONFIG_ARG_TOGGLE,	fetch_router,	NULL },
  { 0, 0, 0, 0 }
};

static const SilcConfigTable table_main[] = {
  { "cipher",		SILC_CONFIG_ARG_BLOCK,	fetch_cipher,  table_cipher },
  { "hash",		SILC_CONFIG_ARG_BLOCK,	fetch_hash,    table_hash },
  { "hmac",		SILC_CONFIG_ARG_BLOCK,	fetch_hmac,    table_hmac },
  { "pkcs",		SILC_CONFIG_ARG_BLOCK,	fetch_pkcs,    table_pkcs },
  { "general",		SILC_CONFIG_ARG_BLOCK,	NULL,	       table_general },
  { "serverinfo",	SILC_CONFIG_ARG_BLOCK,	fetch_serverinfo, table_serverinfo },
  { "logging",		SILC_CONFIG_ARG_BLOCK,	NULL,	       table_logging },
  { "connectionparams",	SILC_CONFIG_ARG_BLOCK,	fetch_connparam, table_connparam },
  { "client",		SILC_CONFIG_ARG_BLOCK,	fetch_client,  table_client },
  { "admin",		SILC_CONFIG_ARG_BLOCK,	fetch_admin,   table_admin },
  { "deny",		SILC_CONFIG_ARG_BLOCK,	fetch_deny,    table_deny },
  { "serverconnection",	SILC_CONFIG_ARG_BLOCK,	fetch_server,  table_serverconn },
  { "routerconnection",	SILC_CONFIG_ARG_BLOCK,	fetch_router,  table_routerconn },
  { 0, 0, 0, 0 }
};

/* Set default values to stuff that was not configured. */

static void silc_server_config_set_defaults(SilcServerConfig config)
{
  my_set_param_defaults(&config->param, NULL);

  config->channel_rekey_secs = (config->channel_rekey_secs ?
				config->channel_rekey_secs :
				SILC_SERVER_CHANNEL_REKEY);
  config->key_exchange_timeout = (config->key_exchange_timeout ?
				  config->key_exchange_timeout :
				  SILC_SERVER_SKE_TIMEOUT);
  config->conn_auth_timeout = (config->conn_auth_timeout ?
			       config->conn_auth_timeout :
			       SILC_SERVER_CONNAUTH_TIMEOUT);
}

/* Check for correctness of the configuration */

static SilcBool silc_server_config_check(SilcServerConfig config)
{
  SilcBool ret = TRUE;
  SilcServerConfigServer *s;
  SilcServerConfigRouter *r;
  SilcBool b = FALSE;

  /* ServerConfig is mandatory */
  if (!config->server_info) {
    SILC_SERVER_LOG_ERROR(("\nError: Missing mandatory block `ServerInfo'"));
    ret = FALSE;
  }

  if (!config->server_info->public_key ||
      !config->server_info->private_key) {
    SILC_SERVER_LOG_ERROR(("\nError: Server keypair is missing"));
    ret = FALSE;
  }

  if (!config->server_info->primary) {
    SILC_SERVER_LOG_ERROR(("\nError: Missing mandatory block `Primary' "
			   "in `ServerInfo'"));
    ret = FALSE;
  }

  if (!config->server_info->primary->server_ip) {
    SILC_SERVER_LOG_ERROR(("\nError: Missing mandatory field `Ip' "
			   "in `Primary' in `ServerInfo'"));
    ret = FALSE;
  }

  /* RouterConnection sanity checks */

  if (config->routers && config->routers->backup_router == TRUE &&
      !config->servers) {
    SILC_SERVER_LOG_ERROR((
         "\nError: First RouterConnection block must be primary router "
	 "connection. You have marked it incorrectly as backup router."));
    ret = FALSE;
  }
  if (config->routers && config->routers->backup_router == TRUE &&
      !config->servers && !config->routers->next) {
    SILC_SERVER_LOG_ERROR((
         "\nError: You have configured backup router but not primary router. "
	 "If backup router is configured also primary router must be "
	 "configured."));
    ret = FALSE;
  }

  /* Backup router sanity checks */

  for (r = config->routers; r; r = r->next) {
    if (r->backup_router && !strcmp(r->host, r->backup_replace_ip)) {
      SILC_SERVER_LOG_ERROR((
          "\nError: Backup router connection incorrectly configured to use "
	  "primary and backup router as same host `%s'. They must not be "
	  "same host.", r->host));
      ret = FALSE;
    }

    if (r->initiator == FALSE && r->port != 0) {
      SILC_SERVER_LOG_WARNING(("\nWarning: Initiator is FALSE and Port is "
                               "specified. Ignoring Port value."));
      r->port = 0;
    }
  }

  /* ServerConnection sanity checks */

  for (s = config->servers; s; s = s->next) {
    if (s->backup_router) {
      b = TRUE;
      break;
    }
  }
  if (b) {
    for (s = config->servers; s; s = s->next) {
      if (!s->backup_router) {
	SILC_SERVER_LOG_ERROR((
          "\nError: Your server is backup router but not all ServerConnection "
	  "blocks were marked as backup connections. They all must be "
	  "marked as backup connections."));
	ret = FALSE;
	break;
      }
    }
  }

  return ret;
}

/* Allocates a new configuration object, opens configuration file and
   parses it. The parsed data is returned to the newly allocated
   configuration object. The SilcServerConfig must be freed by calling
   the silc_server_config_destroy function. */

SilcServerConfig silc_server_config_alloc(const char *filename,
					  SilcServer server)
{
  SilcServerConfig config_new;
  SilcConfigEntity ent;
  SilcConfigFile *file;
  int ret;
  SILC_LOG_DEBUG(("Loading config data from `%s'", filename));

  /* alloc a config object */
  config_new = silc_calloc(1, sizeof(*config_new));
  if (!config_new)
    return NULL;

  /* general config defaults */
  config_new->refcount = 1;
  config_new->logging_timestamp = TRUE;
  config_new->param.reconnect_keep_trying = TRUE;
  config_new->server = server;

  /* obtain a config file object */
  file = silc_config_open(filename);
  if (!file) {
    SILC_SERVER_LOG_ERROR(("\nError: can't open config file `%s'",
			   filename));
    return NULL;
  }

  /* obtain a SilcConfig entity, we can use it to start the parsing */
  ent = silc_config_init(file);

  /* load the known configuration options, give our empty object as context */
  silc_config_register_table(ent, table_main, (void *) config_new);

  /* enter the main parsing loop.  When this returns, we have the parsing
   * result and the object filled (or partially, in case of errors). */
  ret = silc_config_main(ent);
  SILC_LOG_DEBUG(("Parser returned [ret=%d]: %s", ret,
		  silc_config_strerror(ret)));

  /* Check if the parser returned errors */
  if (ret) {
    /* handle this special error return which asks to quietly return */
    if (ret != SILC_CONFIG_ESILENT) {
      char *linebuf, *filename = silc_config_get_filename(file);
      SilcUInt32 line = silc_config_get_line(file);
      if (ret != SILC_CONFIG_EPRINTLINE)
        SILC_SERVER_LOG_ERROR(("Error while parsing config file: %s.",
			       silc_config_strerror(ret)));
      linebuf = silc_config_read_line(file, line);
      if (linebuf) {
	SILC_SERVER_LOG_ERROR(("  file %s line %u:  %s\n", filename,
			       line, linebuf));
	silc_free(linebuf);
      }
    }
    silc_server_config_destroy(config_new);
    silc_config_close(file);
    return NULL;
  }

  /* close (destroy) the file object */
  silc_config_close(file);

  /* Check the configuration */
  if (!silc_server_config_check(config_new)) {
    silc_server_config_destroy(config_new);
    return NULL;
  }

  /* Set default to configuration parameters */
  silc_server_config_set_defaults(config_new);

  return config_new;
}

/* Increments the reference counter of a config object */

void silc_server_config_ref(SilcServerConfigRef *ref, SilcServerConfig config,
			    void *ref_ptr)
{
  if (ref_ptr) {
    config->refcount++;
    ref->config = config;
    ref->ref_ptr = ref_ptr;
    SILC_LOG_DEBUG(("Referencing config [%p] refcnt %d->%d", config,
		    config->refcount - 1, config->refcount));
  }
}

/* Decrements the reference counter of a config object.  If the counter
   reaches 0, the config object is destroyed. */

void silc_server_config_unref(SilcServerConfigRef *ref)
{
  if (ref->ref_ptr)
    silc_server_config_destroy(ref->config);
}

/* Destroy a config object with all his children lists */

void silc_server_config_destroy(SilcServerConfig config)
{
  void *tmp;

  config->refcount--;
  SILC_LOG_DEBUG(("Unreferencing config [%p] refcnt %d->%d", config,
		  config->refcount + 1, config->refcount));
  if (config->refcount > 0)
    return;

  SILC_LOG_DEBUG(("Freeing config context"));

  /* Destroy general config stuff */
  silc_free(config->debug_string);
  silc_free(config->param.version_protocol);
  silc_free(config->param.version_software);
  silc_free(config->param.version_software_vendor);
  silc_free(config->httpd_ip);

  /* Destroy Logging channels */
  if (config->logging_info)
    silc_free(config->logging_info->file);
  if (config->logging_warnings)
    silc_free(config->logging_warnings->file);
  if (config->logging_errors)
    silc_free(config->logging_errors->file);
  if (config->logging_fatals)
    silc_free(config->logging_fatals->file);
  silc_free(config->logging_info);
  silc_free(config->logging_warnings);
  silc_free(config->logging_errors);
  silc_free(config->logging_fatals);

  /* Destroy the ServerInfo struct */
  if (config->server_info) {
    register SilcServerConfigServerInfo *si = config->server_info;
    silc_free(si->server_name);
    if (si->primary) {
      silc_free(si->primary->server_ip);
      silc_free(si->primary);
    }
    SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigServerInfoInterface,
				  si->secondary)
      silc_free(di->server_ip);
      silc_free(di);
    }
    silc_free(si->server_type);
    silc_free(si->location);
    silc_free(si->admin);
    silc_free(si->email);
    silc_free(si->user);
    silc_free(si->group);
    silc_free(si->motd_file);
    silc_free(si->pid_file);
    if (si->public_key)
      silc_pkcs_public_key_free(si->public_key);
    if (si->private_key)
      silc_pkcs_private_key_free(si->private_key);
    silc_free(si);
  }

  /* Now let's destroy the lists */

  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigCipher,
				  config->cipher)
    silc_free(di->name);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigHash, config->hash)
    silc_free(di->name);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigHmac, config->hmac)
    silc_free(di->name);
    silc_free(di->hash);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigPkcs, config->pkcs)
    silc_free(di->name);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigConnParams,
                                  config->conn_params)
    silc_free(di->name);
    silc_free(di->version_protocol);
    silc_free(di->version_software);
    silc_free(di->version_software_vendor);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigClient, config->clients)
    silc_free(di->host);
    CONFIG_FREE_AUTH(di);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigAdmin, config->admins)
    silc_free(di->host);
    silc_free(di->user);
    silc_free(di->nick);
    CONFIG_FREE_AUTH(di);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigDeny, config->denied)
    silc_free(di->host);
    silc_free(di->reason);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigServer,
				  config->servers)
    silc_free(di->host);
    CONFIG_FREE_AUTH(di);
    silc_free(di);
  }
  SILC_SERVER_CONFIG_LIST_DESTROY(SilcServerConfigRouter,
				  config->routers)
    silc_free(di->host);
    silc_free(di->backup_replace_ip);
    CONFIG_FREE_AUTH(di);
    silc_free(di);
  }

  memset(config, 'F', sizeof(*config));
  silc_free(config);
}

/* Registers configured ciphers. These can then be allocated by the
   server when needed. */

SilcBool silc_server_config_register_ciphers(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigCipher *cipher = config->cipher;
  char *module_path = config->module_path;

  SILC_LOG_DEBUG(("Registering configured ciphers"));

  if (!cipher) /* any cipher in the config file? */
    return FALSE;

  while (cipher) {
    /* if there isn't a module_path OR there isn't a module sim name try to
     * use buil-in functions */
    if (!module_path || !cipher->module) {
      int i;
      for (i = 0; silc_default_ciphers[i].name; i++)
	if (!strcmp(silc_default_ciphers[i].name, cipher->name)) {
	  silc_cipher_register((SilcCipherObject *)&silc_default_ciphers[i]);
	  break;
	}
      if (!silc_cipher_is_supported(cipher->name)) {
	SILC_LOG_ERROR(("Unknown cipher `%s'", cipher->name));
	silc_server_stop(server);
	exit(1);
      }
    }
    cipher = cipher->next;
  } /* while */

  return TRUE;
}

/* Registers configured hash functions. These can then be allocated by the
   server when needed. */

SilcBool silc_server_config_register_hashfuncs(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigHash *hash = config->hash;
  char *module_path = config->module_path;

  SILC_LOG_DEBUG(("Registering configured hash functions"));

  if (!hash) /* any hash func in the config file? */
    return FALSE;

  while (hash) {
    /* if there isn't a module_path OR there isn't a module sim name try to
     * use buil-in functions */
    if (!module_path || !hash->module) {
      int i;
      for (i = 0; silc_default_hash[i].name; i++)
	if (!strcmp(silc_default_hash[i].name, hash->name)) {
	  silc_hash_register((SilcHashObject *)&silc_default_hash[i]);
	  break;
	}
      if (!silc_hash_is_supported(hash->name)) {
	SILC_LOG_ERROR(("Unknown hash funtion `%s'", hash->name));
	silc_server_stop(server);
	exit(1);
      }
    }
    hash = hash->next;
  } /* while */

  return TRUE;
}

/* Registers configure HMACs. These can then be allocated by the server
   when needed. */

SilcBool silc_server_config_register_hmacs(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigHmac *hmac = config->hmac;

  SILC_LOG_DEBUG(("Registering configured HMACs"));

  if (!hmac)
    return FALSE;

  while (hmac) {
    SilcHmacObject hmac_obj;
    if (!silc_hash_is_supported(hmac->hash)) {
      SILC_LOG_ERROR(("Unknown hash function `%s'", hmac->hash));
      silc_server_stop(server);
      exit(1);
    }

    /* Register the HMAC */
    memset(&hmac_obj, 0, sizeof(hmac_obj));
    hmac_obj.name = hmac->name;
    hmac_obj.len = hmac->mac_length;
    silc_hmac_register(&hmac_obj);

    hmac = hmac->next;
  } /* while */

  return TRUE;
}

/* Registers configured PKCS's. */

SilcBool silc_server_config_register_pkcs(SilcServer server)
{
  return FALSE;
}

/* Sets log files where log messages are saved by the server logger. */

void silc_server_config_setlogfiles(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigLogging *this;

  SILC_LOG_DEBUG(("Setting configured log file names and options"));

  silc_log_timestamp(config->logging_timestamp);
  silc_log_quick(config->logging_quick);
  silc_log_flushdelay(config->logging_flushdelay ?
		      config->logging_flushdelay :
		      SILC_SERVER_LOG_FLUSH_DELAY);

  if ((this = config->logging_fatals))
    silc_log_set_file(SILC_LOG_FATAL, this->file, this->maxsize,
		      server->schedule);
  if ((this = config->logging_errors))
    silc_log_set_file(SILC_LOG_ERROR, this->file, this->maxsize,
		      server->schedule);
  if ((this = config->logging_warnings))
    silc_log_set_file(SILC_LOG_WARNING, this->file, this->maxsize,
		      server->schedule);
  if ((this = config->logging_info))
    silc_log_set_file(SILC_LOG_INFO, this->file, this->maxsize,
		      server->schedule);
}

/* Returns client authentication information from configuration file by host
   (name or ip) */

SilcServerConfigClient *
silc_server_config_find_client(SilcServer server, char *host)
{
  SilcServerConfig config = server->config;
  SilcServerConfigClient *client;

  if (!config || !host)
    return NULL;

  for (client = config->clients; client; client = client->next) {
    if (client->host && !silc_string_match(client->host, host))
      continue;
    break;
  }

  /* if none matched, then client is already NULL */
  return client;
}

/* Returns admin connection configuration by host, username and/or
   nickname. */

SilcServerConfigAdmin *
silc_server_config_find_admin(SilcServer server, char *host, char *user,
			      char *nick)
{
  SilcServerConfig config = server->config;
  SilcServerConfigAdmin *admin;

  /* make sure we have a value for the matching parameters */
  if (!host)
    host = "*";
  if (!user)
    user = "*";
  if (!nick)
    nick = "*";

  for (admin = config->admins; admin; admin = admin->next) {
    if (admin->host && !silc_string_match(admin->host, host))
      continue;
    if (admin->user && !silc_string_match(admin->user, user))
      continue;
    if (admin->nick && !silc_string_match(admin->nick, nick))
      continue;
    /* no checks failed -> this entry matches */
    break;
  }

  /* if none matched, then admin is already NULL */
  return admin;
}

/* Returns the denied connection configuration entry by host. */

SilcServerConfigDeny *
silc_server_config_find_denied(SilcServer server, char *host)
{
  SilcServerConfig config = server->config;
  SilcServerConfigDeny *deny;

  /* make sure we have a value for the matching parameters */
  if (!config || !host)
    return NULL;

  for (deny = config->denied; deny; deny = deny->next) {
    if (deny->host && !silc_string_match(deny->host, host))
      continue;
    break;
  }

  /* if none matched, then deny is already NULL */
  return deny;
}

/* Returns server connection info from server configuartion by host
   (name or ip). */

SilcServerConfigServer *
silc_server_config_find_server_conn(SilcServer server, char *host)
{
  SilcServerConfig config = server->config;
  SilcServerConfigServer *serv = NULL;

  if (!host)
    return NULL;

  if (!config->servers)
    return NULL;

  for (serv = config->servers; serv; serv = serv->next) {
    if (!silc_string_match(serv->host, host))
      continue;
    break;
  }

  return serv;
}

/* Returns router connection info from server configuration by
   host (name or ip). */

SilcServerConfigRouter *
silc_server_config_find_router_conn(SilcServer server, char *host, int port)
{
  SilcServerConfig config = server->config;
  SilcServerConfigRouter *serv = NULL;

  if (!host)
    return NULL;

  if (!config->routers)
    return NULL;

  for (serv = config->routers; serv; serv = serv->next) {
    if (!silc_string_match(serv->host, host))
      continue;
    if (port && serv->port && serv->port != port)
      continue;
    break;
  }

  return serv;
}

/* Find backup router connection by host (name or ip) */

SilcServerConfigRouter *
silc_server_config_find_backup_conn(SilcServer server, char *host)
{
  SilcServerConfig config = server->config;
  SilcServerConfigRouter *serv = NULL;

  if (!host)
    return NULL;

  if (!config->routers)
    return NULL;

  for (serv = config->routers; serv; serv = serv->next) {
    if (!serv->backup_router)
      continue;
    if (!silc_string_match(serv->host, host))
      continue;
    break;
  }

  return serv;
}

/* Returns TRUE if configuration for a router connection that we are
   initiating exists. */

SilcBool silc_server_config_is_primary_route(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigRouter *serv = NULL;
  int i;
  SilcBool found = FALSE;

  serv = config->routers;
  for (i = 0; serv; i++) {
    if (serv->initiator == TRUE && serv->backup_router == FALSE) {
      found = TRUE;
      break;
    }

    serv = serv->next;
  }

  return found;
}

/* Returns our primary connection configuration or NULL if we do not
   have primary router configured. */

SilcServerConfigRouter *
silc_server_config_get_primary_router(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigRouter *serv = NULL;
  int i;

  serv = config->routers;
  for (i = 0; serv; i++) {
    if (serv->initiator == TRUE && serv->backup_router == FALSE)
      return serv;
    serv = serv->next;
  }

  return NULL;
}

/* If we have backup router configured that is going to replace us this
   function returns it. */

SilcServerConfigRouter *
silc_server_config_get_backup_router(SilcServer server)
{
  SilcServerConfig config = server->config;
  SilcServerConfigRouter *serv = NULL;
  int i;

  if (server->server_type != SILC_ROUTER)
    return NULL;

  serv = config->routers;
  for (i = 0; serv; i++) {
    if (serv->initiator == FALSE && serv->backup_router == TRUE &&
	serv->backup_local == TRUE &&
	!strcmp(server->config->server_info->primary->server_ip,
		serv->backup_replace_ip) &&
	server->config->server_info->primary->port ==
	serv->backup_replace_port)
      return serv;
    serv = serv->next;
  }

  return NULL;
}
