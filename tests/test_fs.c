#include "plugins/fs/path.h"
#include "safe_open.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_lexical(void) {
    char out[256];
    CHECK(safe_relative_path("note.txt", out, sizeof(out)));
    CHECK(strcmp(out, "note.txt") == 0);
    CHECK(safe_relative_path("dir/../note.txt", out, sizeof(out)));
    CHECK(strcmp(out, "note.txt") == 0);
    CHECK(safe_relative_path("dir/sub/../../note.txt", out, sizeof(out)));
    CHECK(strcmp(out, "note.txt") == 0);
    CHECK(!safe_relative_path("../note.txt", out, sizeof(out)));
    CHECK(!safe_relative_path("/etc/passwd", out, sizeof(out)));
    CHECK(!safe_relative_path("dir/../../etc/passwd", out, sizeof(out)));
    CHECK(!safe_relative_path("..", out, sizeof(out)));
    CHECK(!safe_relative_path("", out, sizeof(out)));
    CHECK(!safe_relative_path("a\\b", out, sizeof(out)));
    CHECK(!safe_relative_path("C:/windows", out, sizeof(out)));
}

static void test_jail(void) {
    char root[] = "/tmp/crossweb-fs-XXXXXX";
    char outside[] = "/tmp/crossweb-fs-out-XXXXXX";
    char path[512];
    char *content = NULL;
    size_t size = 0;
    FILE *file;
    ReadFileRequest read_req;
    WriteFileRequest write_req;

    CHECK(mkdtemp(root) != NULL);
    CHECK(mkdtemp(outside) != NULL);
    snprintf(path, sizeof(path), "%s/outside.txt", outside);
    file = fopen(path, "w");
    CHECK(file != NULL);
    fputs("outside", file);
    fclose(file);

    fs_clear_root();
    read_req.path = "note.txt";
    read_req.binary = false;
    CHECK(fs_read_file(&read_req, &content, &size) == FS_ERROR_PERMISSION_DENIED);

    CHECK(fs_set_root(root));
    write_req.path = "note.txt";
    write_req.content = "hello";
    write_req.binary = false;
    CHECK(fs_write_file(&write_req) == FS_ERROR_NONE);
    CHECK(fs_read_file(&read_req, &content, &size) == FS_ERROR_NONE);
    CHECK(content && strcmp(content, "hello") == 0);
    free(content);
    content = NULL;

    read_req.path = "/etc/passwd";
    CHECK(fs_read_file(&read_req, &content, &size) == FS_ERROR_INVALID_PATH);
    CHECK(content == NULL);
    read_req.path = "../outside.txt";
    CHECK(fs_read_file(&read_req, &content, &size) != FS_ERROR_NONE);
    CHECK(content == NULL);
    write_req.path = "../owned.txt";
    write_req.content = "nope";
    CHECK(fs_write_file(&write_req) != FS_ERROR_NONE);
    snprintf(path, sizeof(path), "%s/owned.txt", outside);
    CHECK(access(path, F_OK) != 0);

    snprintf(path, sizeof(path), "%s/escape.txt", root);
    CHECK(symlink("/etc/passwd", path) == 0);
    read_req.path = "escape.txt";
    CHECK(fs_read_file(&read_req, &content, &size) == FS_ERROR_PERMISSION_DENIED);
    CHECK(content == NULL);

    snprintf(path, sizeof(path), "%s/nested", root);
    CHECK(mkdir(path, 0700) == 0);
    write_req.path = "nested/inner.txt";
    write_req.content = "inner";
    CHECK(fs_write_file(&write_req) == FS_ERROR_NONE);
    read_req.path = "nested/inner.txt";
    CHECK(fs_read_file(&read_req, &content, &size) == FS_ERROR_NONE);
    CHECK(content && strcmp(content, "inner") == 0);
    free(content);
}

int main(void) {
    test_lexical();
    test_jail();
    if (failures != 0) {
        fprintf(stderr, "test_fs: %d checks, %d failures\n", checks, failures);
        return 1;
    }
    printf("test_fs: %d checks, 0 failures\n", checks);
    return 0;
}
