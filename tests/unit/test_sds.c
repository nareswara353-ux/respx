#include "kave/sds.h"
#include "kave/allocator.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        failures++; \
    } \
} while (0)

int main(void)
{
    sds s = sds_new("hello");
    ASSERT(s != NULL, "sds_new returns non-null");
    ASSERT(sds_len(s) == 5, "initial length is 5");
    ASSERT(strcmp(sds_data(s), "hello") == 0, "initial content correct");
    sds_free(s);

    s = sds_new("");
    ASSERT(s != NULL, "empty sds created");
    ASSERT(sds_len(s) == 0, "empty sds length zero");
    s = sds_append(s, "abc");
    ASSERT(sds_len(s) == 3, "after append length 3");
    s = sds_append(s, "def");
    ASSERT(sds_len(s) == 6, "after second append length 6");
    ASSERT(strcmp(sds_data(s), "abcdef") == 0, "concatenation correct");
    sds_free(s);

    s = sds_new("  hello  ");
    s = sds_trim(s, " ");
    ASSERT(strcmp(sds_data(s), "hello") == 0, "trim removes spaces");
    ASSERT(sds_len(s) == 5, "trim updates length");
    sds_free(s);

    s = sds_new("original");
    sds copy = sds_dup(s);
    ASSERT(copy != NULL, "sds_dup returns non-null");
    ASSERT(sds_cmp(s, copy) == 0, "duplicate equals original");
    ASSERT(sds_data(s) != sds_data(copy), "duplicate has separate buffer");
    sds_free(s);
    sds_free(copy);

    sds a = sds_new("abc");
    sds b = sds_new("abd");
    ASSERT(sds_cmp(a, b) < 0, "abc < abd");
    sds_free(a);
    sds_free(b);

    s = sds_new("reset me");
    sds_clear(s);
    ASSERT(sds_len(s) == 0, "after clear length 0");
    ASSERT(sds_data(s)[0] == '\0', "after clear buffer empty");
    sds_free(s);

    s = sds_new("format");
    s = sds_cat_printf(s, " %d-%s", 42, "test");
    ASSERT(strcmp(sds_data(s), "format 42-test") == 0, "cat_printf formats");
    sds_free(s);

    kave_alloc_stats stats = kave_get_global_stats();
    ASSERT(stats.active_bytes == 0, "no leaked bytes");
    ASSERT(stats.active_blocks == 0, "no leaked blocks");

    if (failures == 0) {
        printf("test_sds: OK\n");
        return 0;
    }
    fprintf(stderr, "test_sds: %d failures\n", failures);
    return 1;
}
