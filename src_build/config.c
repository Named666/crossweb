#include "config.h"
#include <string.h>

const char *get_package_name(void) {
    static char package_name[256];
    String_Builder gradle = {0};
    if (!read_entire_file("android/app/build.gradle.kts", &gradle)) return NULL;
    const char *namespace_attr = "namespace = \"";
    char *pos = strstr(gradle.items, namespace_attr);
    if (!pos) { da_free(gradle); return NULL; }
    pos += strlen(namespace_attr);
    char *end = strchr(pos, '"');
    if (!end) { da_free(gradle); return NULL; }
    size_t len = end - pos;
    if (len >= sizeof(package_name)) { da_free(gradle); return NULL; }
    memcpy(package_name, pos, len);
    package_name[len] = '\0';
    da_free(gradle);
    return package_name;
}

bool is_plugin_enabled(const char *plugin_name) {
    String_Builder content = {0};
    if (!read_entire_file("build/config.h", &content)) return true;
    char macro[256];
    sprintf(macro, "#define CROSSWEB_PLUGIN_%s 1", plugin_name);
    for (size_t j = strlen("#define CROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
        macro[j] = toupper(macro[j]);
    }
    bool enabled = strstr(content.items, macro) != NULL;
    da_free(content);
    return enabled;
}

bool validate_android_init(void) {
    if (file_exists("android/app/build.gradle.kts") != 1) {
        nob_log(ERROR, "Android project not initialized. Run 'nob android init' first.");
        return false;
    }
    if (file_exists("android/app/src/main/AndroidManifest.xml") != 1) {
        nob_log(ERROR, "Android manifest missing. Re-run 'nob android init'.");
        return false;
    }
    return true;
}