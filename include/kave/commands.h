#ifndef KAVE_COMMANDS_H
#define KAVE_COMMANDS_H

#include "kave/resp_parser.h"
#include "kave/hash_table.h"
#include <stddef.h>

typedef struct command_result {
    int success;
    resp_value *response;
    char *error_msg;
} command_result;

typedef command_result *(*command_fn)(ht *storage, const resp_value *args, void *ctx);

command_result *cmd_get(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_set(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_del(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_mget(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_expire(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_incr(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_zadd(ht *storage, const resp_value *args, void *ctx);
command_result *cmd_zrange(ht *storage, const resp_value *args, void *ctx);

command_result *command_dispatch(ht *storage, const char *cmd_name, const resp_value *args, void *ctx);
void command_result_free(command_result *res);

#endif
