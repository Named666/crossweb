#include "origin.h"
#include "assets_map.h"
#include "host_config.h"
#include "plug.h"
#include "safe_open.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void test_origin(void) {
    char origin[256];
    CHECK(origin_extract("crossweb://app/index.html", origin, sizeof(origin)));
    CHECK(strcmp(origin, "crossweb://app") == 0);
    CHECK(origin_is_trusted("crossweb://app/app.js", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("crossweb://evil/index.html", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("https://crossweb.localhost/index.html", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(origin_is_trusted("https://crossweb.localhost/index.html", CROSSWEB_WINDOWS_APP_ORIGIN, NULL));
    CHECK(origin_is_trusted("https://appassets.androidplatform.net/index.html", CROSSWEB_ANDROID_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("https://appassets.androidplatform.net.evil/index.html", CROSSWEB_ANDROID_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("javascript:alert(1)", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("file:///etc/passwd", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("data:text/html,hi", CROSSWEB_APP_ORIGIN, NULL));
    CHECK(!origin_is_trusted("http://example.com/", CROSSWEB_APP_ORIGIN, NULL));

    CHECK(!origin_dev_url_allowed(NULL, origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("", origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("http://localhost/index.html", origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("http://example.com:5173/", origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("http://user@127.0.0.1:5173/", origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("http://127.0.0.1:5173.evil/", origin, sizeof(origin)));
    CHECK(!origin_dev_url_allowed("javascript:http://127.0.0.1:5173/", origin, sizeof(origin)));
    CHECK(origin_dev_url_allowed("http://127.0.0.1:5173/app", origin, sizeof(origin)));
    CHECK(strcmp(origin, "http://127.0.0.1:5173") == 0);
    CHECK(origin_is_trusted("http://127.0.0.1:5173/x", CROSSWEB_APP_ORIGIN, origin));
    CHECK(!origin_is_trusted("http://127.0.0.1:5174/x", CROSSWEB_APP_ORIGIN, origin));
    CHECK(!origin_is_trusted("http://127.0.0.1:51730/x", CROSSWEB_APP_ORIGIN, origin));
    CHECK(origin_dev_url_allowed("https://localhost:8443", origin, sizeof(origin)));
    CHECK(strcmp(origin, "https://localhost:8443") == 0);
}

static void test_assets(void) {
    unsigned char *data = NULL;
    size_t len = 0;
    const char *mime = NULL;
    const char *err = NULL;
    CHECK(assets_read("assets", "/", &data, &len, &mime, &err));
    CHECK(data != NULL && strstr((char *)data, "app.js") != NULL);
    CHECK(mime != NULL && strstr(mime, "text/html") != NULL);
    free(data);
    data = NULL;
    CHECK(assets_read("assets", "/app.js", &data, &len, &mime, &err));
    CHECK(data != NULL && strstr((char *)data, "app.ping") != NULL);
    free(data);
    data = NULL;
    CHECK(!assets_read("assets", "/../crossweb.permissions", &data, &len, &mime, &err));
    CHECK(data == NULL);
    CHECK(!assets_read("assets", "/%2e%2e/crossweb.permissions", &data, &len, &mime, &err));
    CHECK(!assets_read("assets", "/secret.c", &data, &len, &mime, &err));
    CHECK(!assets_read(NULL, "/", &data, &len, &mime, &err));
}

static void test_assets_jail(void) {
    char dir[] = "/tmp/crossweb-assets-XXXXXX";
    char outside_dir[] = "/tmp/crossweb-secret-XXXXXX";
    char path[512];
    FILE *file;
    unsigned char *data = NULL;
    size_t len = 0;

    CHECK(mkdtemp(dir) != NULL);
    CHECK(mkdtemp(outside_dir) != NULL);
    snprintf(path, sizeof(path), "%s/secret.txt", outside_dir);
    file = fopen(path, "w");
    CHECK(file != NULL);
    fputs("secret", file);
    fclose(file);
    snprintf(path, sizeof(path), "%s/index.html", dir);
    file = fopen(path, "w");
    CHECK(file != NULL);
    fputs("<html>ok</html>", file);
    fclose(file);
    snprintf(path, sizeof(path), "%s/escape.html", dir);
    CHECK(symlink("/etc/passwd", path) == 0);
    CHECK(assets_read(dir, "/index.html", &data, &len, NULL, NULL));
    CHECK(data && strcmp((char *)data, "<html>ok</html>") == 0);
    free(data);
    data = NULL;
    CHECK(!assets_read(dir, "/escape.html", &data, &len, NULL, NULL));
    CHECK(data == NULL);
    CHECK(!assets_read(dir, "/../secret.txt", &data, &len, NULL, NULL));
}

static void test_host_config(void) {
    char origin[256];
    const char *err = NULL;
    FILE *file;

    CHECK(host_prepare("crossweb.permissions", NULL, origin, sizeof(origin), &err) == 0);
    CHECK(origin[0] == '\0');
    CHECK(plug_is_allowed("app", "ping"));
    CHECK(!plug_is_allowed("fs", "read"));
    CHECK(!plug_is_allowed("fs", "write"));
    CHECK(!plug_is_allowed("keystore", "encrypt"));

    CHECK(host_prepare("build/missing-grants.txt", NULL, origin, sizeof(origin), &err) != 0);
    CHECK(!plug_is_allowed("app", "ping"));

    file = fopen("build/bad-grants.txt", "w");
    CHECK(file != NULL);
    fputs("app.ping\nnot a grant\n", file);
    fclose(file);
    CHECK(host_prepare("build/bad-grants.txt", NULL, origin, sizeof(origin), &err) != 0);
    CHECK(!plug_is_allowed("app", "ping"));

    CHECK(host_prepare("crossweb.permissions", "http://evil.test:5173/", origin, sizeof(origin), &err) != 0);
    CHECK(!plug_is_allowed("app", "ping"));
    CHECK(host_prepare("crossweb.permissions", "http://127.0.0.1:5173", origin, sizeof(origin), &err) == 0);
    CHECK(strcmp(origin, "http://127.0.0.1:5173") == 0);
    CHECK(origin_is_trusted("http://127.0.0.1:5173/index.html", CROSSWEB_APP_ORIGIN, origin));
    CHECK(!origin_is_trusted("http://evil.test/", CROSSWEB_APP_ORIGIN, origin));
}

int main(void) {
    test_origin();
    test_assets();
    test_assets_jail();
    test_host_config();
    if (failures != 0) {
        fprintf(stderr, "test_bounds: %d checks, %d failures\n", checks, failures);
        return 1;
    }
    printf("test_bounds: %d checks, 0 failures\n", checks);
    return 0;
}
