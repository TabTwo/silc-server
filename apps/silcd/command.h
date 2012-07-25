
/*

  servercommand.h

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 1997 - 2005, 2007 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#ifndef COMMAND_H
#define COMMAND_H

/*
   Structure holding one command and pointer to its function.

   SilcCommandCb cb

       Callback function called when this command is executed.

   SilcCommand cmd

       The actual command. Defined in silccore/silccommand.h

   SilcCommandFlag flags

       Flags for the command. These set how command behaves on different
       situations.

*/
typedef struct {
  SilcCommandCb cb;
  SilcCommand cmd;
  SilcCommandFlag flags;
} SilcServerCommand;

/* All server commands */
extern SilcServerCommand silc_command_list[];

/* Context sent as argument to all commands */
typedef struct {
  SilcServer server;
  SilcPacketStream sock;
  SilcCommandPayload payload;
  SilcArgumentPayload args;
  SilcPacket packet;
  int pending;			/* Command is being re-processed when TRUE */
  int users;			/* Reference counter */
} *SilcServerCommandContext;

/* Structure holding pending commands. If command is pending it will be
   executed after command reply has been received and executed. */
typedef struct SilcServerCommandPendingStruct {
  SilcCommand reply_cmd;
  SilcUInt16 ident;
  unsigned int reply_check : 8;
  SilcCommandCb callback;
  void *context;
  SilcTask timeout;
  struct SilcServerCommandPendingStruct *next;
} SilcServerCommandPending;

#include "command_reply.h"

/* Macros */

/* Macro used for command declaration in command list structure */
#define SILC_SERVER_CMD(func, cmd, flags) \
{ silc_server_command_##func, SILC_COMMAND_##cmd, flags }

/* Macro used to declare command functions. The `context' will be the
   SilcServerCommandContext and the `context2' is the
   SilcServerCommandReplyContext if this function is called from the
   command reply as pending command callback. Otherwise `context2'
   is NULL. */
#define SILC_SERVER_CMD_FUNC(func) \
void silc_server_command_##func(void *context, void *context2)

/* Executed pending command. The first argument to the callback function
   is the user specified context. The second argument is always the
   SilcServerCommandReply context. */
#define SILC_SERVER_PENDING_EXEC(ctx, cmd)				\
do {									\
  int _i;								\
  for (_i = 0; _i < ctx->callbacks_count; _i++)				\
    if (ctx->callbacks[_i].callback)					\
      (*ctx->callbacks[_i].callback)(ctx->callbacks[_i].context, ctx);	\
  silc_server_command_pending_del(ctx->server, cmd, ctx->ident);	\
} while(0)

/* Prototypes */
void silc_server_command_process(SilcServer server,
				 SilcPacketStream sock,
				 SilcPacket packet);
SilcServerCommandContext silc_server_command_alloc();
void silc_server_command_free(SilcServerCommandContext ctx);
SilcServerCommandContext
silc_server_command_dup(SilcServerCommandContext ctx);
SilcBool silc_server_command_pending(SilcServer server,
				 SilcCommand reply_cmd,
				 SilcUInt16 ident,
				 SilcCommandCb callback,
				 void *context);
SilcBool silc_server_command_pending_timed(SilcServer server,
				       SilcCommand reply_cmd,
				       SilcUInt16 ident,
				       SilcCommandCb callback,
				       void *context,
				       SilcUInt16 timeout);
void silc_server_command_pending_del(SilcServer server,
				     SilcCommand reply_cmd,
				     SilcUInt16 ident);
SilcServerCommandPendingCallbacks
silc_server_command_pending_check(SilcServer server,
				  SilcCommand command,
				  SilcUInt16 ident,
				  SilcUInt32 *callbacks_count);
SILC_SERVER_CMD_FUNC(whois);
SILC_SERVER_CMD_FUNC(whowas);
SILC_SERVER_CMD_FUNC(identify);
SILC_SERVER_CMD_FUNC(newuser);
SILC_SERVER_CMD_FUNC(nick);
SILC_SERVER_CMD_FUNC(list);
SILC_SERVER_CMD_FUNC(topic);
SILC_SERVER_CMD_FUNC(invite);
SILC_SERVER_CMD_FUNC(quit);
SILC_SERVER_CMD_FUNC(kill);
SILC_SERVER_CMD_FUNC(info);
SILC_SERVER_CMD_FUNC(stats);
SILC_SERVER_CMD_FUNC(ping);
SILC_SERVER_CMD_FUNC(oper);
SILC_SERVER_CMD_FUNC(join);
SILC_SERVER_CMD_FUNC(motd);
SILC_SERVER_CMD_FUNC(umode);
SILC_SERVER_CMD_FUNC(cmode);
SILC_SERVER_CMD_FUNC(cumode);
SILC_SERVER_CMD_FUNC(kick);
SILC_SERVER_CMD_FUNC(ban);
SILC_SERVER_CMD_FUNC(detach);
SILC_SERVER_CMD_FUNC(watch);
SILC_SERVER_CMD_FUNC(silcoper);
SILC_SERVER_CMD_FUNC(leave);
SILC_SERVER_CMD_FUNC(users);
SILC_SERVER_CMD_FUNC(getkey);
SILC_SERVER_CMD_FUNC(service);

SILC_SERVER_CMD_FUNC(connect);
SILC_SERVER_CMD_FUNC(close);
SILC_SERVER_CMD_FUNC(shutdown);

#endif
