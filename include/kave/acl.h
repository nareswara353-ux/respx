#ifndef KAVE_ACL_H
#define KAVE_ACL_H

#include <stddef.h>

typedef struct acl acl;
typedef struct acl_user acl_user;

acl *acl_new(void);
void acl_free(acl *a);
int acl_add_user(acl *a, const char *name, const char *password);
int acl_delete_user(acl *a, const char *name);
int acl_authenticate(const acl *a, const char *name, const char *password);
int acl_check_command(const acl *a, const char *name, const char *command);
int acl_set_user_enabled(acl *a, const char *name, int enabled);
int acl_allow_command(acl *a, const char *name, const char *command);
size_t acl_user_count(const acl *a);

#endif
