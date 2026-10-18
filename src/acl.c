#include "kave/acl.h"
#include "kave/allocator.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct acl_user {
    char *name;
    uint64_t password_hash;
    int has_password;
    int enabled;
    int all_commands;
    char **commands;
    size_t num_commands;
    struct acl_user *next;
};

struct acl {
    struct acl_user *users;
    size_t num_users;
};

static uint64_t acl_hash_password(const char *password)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const char *p = password; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 0x100000001b3ULL;
    }
    return h;
}

static struct acl_user *acl_find_user(const acl *a, const char *name)
{
    struct acl_user *u = a->users;
    while (u) {
        if (strcmp(u->name, name) == 0) return u;
        u = u->next;
    }
    return NULL;
}

static void acl_user_free(struct acl_user *u)
{
    if (!u) return;
    if (u->name) kave_free(u->name);
    for (size_t i = 0; i < u->num_commands; i++) {
        if (u->commands[i]) kave_free(u->commands[i]);
    }
    if (u->commands) kave_free(u->commands);
    kave_free(u);
}

acl *acl_new(void)
{
    acl *a = kave_malloc(sizeof(acl));
    if (!a) return NULL;
    a->users = NULL;
    a->num_users = 0;
    struct acl_user *u = kave_malloc(sizeof(struct acl_user));
    if (!u) {
        kave_free(a);
        return NULL;
    }
    size_t nlen = strlen("default");
    u->name = kave_malloc(nlen + 1);
    if (!u->name) {
        kave_free(u);
        kave_free(a);
        return NULL;
    }
    memcpy(u->name, "default", nlen + 1);
    u->password_hash = 0;
    u->has_password = 0;
    u->enabled = 1;
    u->all_commands = 1;
    u->commands = NULL;
    u->num_commands = 0;
    u->next = NULL;
    a->users = u;
    a->num_users = 1;
    return a;
}

void acl_free(acl *a)
{
    if (!a) return;
    struct acl_user *u = a->users;
    while (u) {
        struct acl_user *next = u->next;
        acl_user_free(u);
        u = next;
    }
    kave_free(a);
}

int acl_add_user(acl *a, const char *name, const char *password)
{
    if (!a || !name) return -1;
    if (acl_find_user(a, name)) return -1;
    struct acl_user *u = kave_malloc(sizeof(struct acl_user));
    if (!u) return -1;
    size_t nlen = strlen(name);
    u->name = kave_malloc(nlen + 1);
    if (!u->name) {
        kave_free(u);
        return -1;
    }
    memcpy(u->name, name, nlen + 1);
    if (password) {
        u->password_hash = acl_hash_password(password);
        u->has_password = 1;
    } else {
        u->password_hash = 0;
        u->has_password = 0;
    }
    u->enabled = 1;
    u->all_commands = 0;
    u->commands = NULL;
    u->num_commands = 0;
    u->next = a->users;
    a->users = u;
    a->num_users++;
    return 0;
}

int acl_delete_user(acl *a, const char *name)
{
    if (!a || !name) return -1;
    if (strcmp(name, "default") == 0) return -1;
    struct acl_user **pp = &a->users;
    while (*pp) {
        struct acl_user *u = *pp;
        if (strcmp(u->name, name) == 0) {
            *pp = u->next;
            acl_user_free(u);
            a->num_users--;
            return 0;
        }
        pp = &u->next;
    }
    return -1;
}

int acl_authenticate(const acl *a, const char *name, const char *password)
{
    if (!a || !name) return 0;
    struct acl_user *u = acl_find_user(a, name);
    if (!u) return 0;
    if (!u->enabled) return 0;
    if (!u->has_password) return 1;
    if (!password) return 0;
    return acl_hash_password(password) == u->password_hash;
}

int acl_check_command(const acl *a, const char *name, const char *command)
{
    if (!a || !name || !command) return 0;
    struct acl_user *u = acl_find_user(a, name);
    if (!u || !u->enabled) return 0;
    if (u->all_commands) return 1;
    for (size_t i = 0; i < u->num_commands; i++) {
        if (strcmp(u->commands[i], command) == 0) return 1;
    }
    return 0;
}

int acl_set_user_enabled(acl *a, const char *name, int enabled)
{
    if (!a || !name) return -1;
    struct acl_user *u = acl_find_user(a, name);
    if (!u) return -1;
    u->enabled = enabled ? 1 : 0;
    return 0;
}

int acl_allow_command(acl *a, const char *name, const char *command)
{
    if (!a || !name || !command) return -1;
    struct acl_user *u = acl_find_user(a, name);
    if (!u) return -1;
    if (strcmp(command, "*") == 0) {
        u->all_commands = 1;
        return 0;
    }
    char **new_cmds = kave_realloc(u->commands, (u->num_commands + 1) * sizeof(char *));
    if (!new_cmds) return -1;
    u->commands = new_cmds;
    size_t clen = strlen(command);
    char *copy = kave_malloc(clen + 1);
    if (!copy) return -1;
    memcpy(copy, command, clen + 1);
    u->commands[u->num_commands++] = copy;
    return 0;
}

size_t acl_user_count(const acl *a)
{
    return a ? a->num_users : 0;
}
