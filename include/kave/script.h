#ifndef KAVE_SCRIPT_H
#define KAVE_SCRIPT_H

#include <stddef.h>

#define SCRIPT_SHA_LEN 40

typedef struct script_ctx script_ctx;

script_ctx *script_new(void);
void script_free(script_ctx *s);
int script_load(script_ctx *s, const char *body, size_t body_len, char *sha_out);
int script_exists(script_ctx *s, const char *sha);
int script_flush(script_ctx *s);
const char *script_get_body(script_ctx *s, const char *sha, size_t *body_len);
size_t script_count(const script_ctx *s);

#endif
