#define CONFIG_PATH "./build/config.h"

#include <ctype.h>

typedef struct {
    const char *name;
    const char *macro;
    const char *description;
    bool enabled_by_default;
} Feature_Flag;

static Feature_Flag feature_flags[] = {
    {
        .macro = "CROSSWEB_HOTRELOAD",
        .name = "hotreload",
        .description = "Moves the plugin runtime to a shared library so it can be reloaded. Opt-in.",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_UNBUNDLE",
        .name = "unbundle",
        .description = "Don't bundle resources/ folder with the executable and load the resources directly from the folder.",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_WEBVIEW_DEBUG",
        .name = "webview-debug",
        .description = "Enable WebView remote debugging. Off unless explicitly configured.",
        .enabled_by_default = false,
    },
};

typedef struct {
    const char *name;
    const char *macro;
    bool enabled_by_default;
} Target_Flag;

static Target_Flag target_flags[] = {
    {
        .macro = "CROSSWEB_TARGET_WIN64_GCC",
        .name = "win64-gcc",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_TARGET_LINUX",
        .name = "linux",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_TARGET_MACOS",
        .name = "macos",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_TARGET_OPENBSD",
        .name = "openbsd",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_TARGET_ANDROID",
        .name = "android",
        .enabled_by_default = false,
    },
    {
        .macro = "CROSSWEB_TARGET_IOS",
        .name = "ios",
        .enabled_by_default = false,
    },
};

static bool target_overridden = false;

static void select_host_target_if_needed(void)
{
    if (target_overridden) return;
#if defined(_WIN32)
    const char *host = "win64-gcc";
#elif defined(__linux__)
    const char *host = "linux";
#else
    const char *host = NULL;
#endif
    for (size_t i = 0; i < NOB_ARRAY_LEN(target_flags); ++i) {
        target_flags[i].enabled_by_default = host != NULL && strcmp(target_flags[i].name, host) == 0;
    }
    if (host == NULL) {
        nob_log(NOB_WARNING, "No host target detected; config will not enable a target");
    }
}

bool config_set_target(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        nob_log(NOB_ERROR, "Expected a target name");
        return false;
    }
    if (strcmp(name, "macos") == 0 || strcmp(name, "ios") == 0 || strcmp(name, "openbsd") == 0) {
        nob_log(NOB_ERROR, "Target `%s` is deferred. Use linux, win64-gcc, or android.", name);
        return false;
    }
    bool found = false;
    for (size_t i = 0; i < NOB_ARRAY_LEN(target_flags); ++i) {
        bool match = strcmp(target_flags[i].name, name) == 0;
        if (match) found = true;
        target_flags[i].enabled_by_default = match;
    }
    if (!found) {
        nob_log(NOB_ERROR, "Unknown target `%s`", name);
        nob_log(NOB_ERROR, "Known targets: win64-gcc, linux, macos, openbsd, android, ios");
        return false;
    }
    target_overridden = true;
    return true;
}

#define genf(out, ...) \
    do { \
        fprintf((out), __VA_ARGS__); \
        fprintf((out), " // %s:%d\n", __FILE__, __LINE__); \
    } while(0)

bool generate_default_config(const char *file_path)
{
    nob_log(NOB_INFO, "Generating %s", file_path);
    FILE *f = fopen(file_path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not generate %s: %s", file_path, strerror(errno));
        return false;
    }

    select_host_target_if_needed();

    fprintf(f, "#ifndef CROSSWEB_CONFIG_H_\n");
    fprintf(f, "#define CROSSWEB_CONFIG_H_\n\n");
    fprintf(f, "//// Feature flags\n");
    for (size_t i = 0; i < NOB_ARRAY_LEN(feature_flags); ++i) {
        Feature_Flag flag = feature_flags[i];
        genf(f, "#define %s %d\n", flag.macro, flag.enabled_by_default ? 1 : 0);
    }

    fprintf(f, "\n//// Target\n");
    for (size_t i = 0; i < NOB_ARRAY_LEN(target_flags); ++i) {
        Target_Flag flag = target_flags[i];
        if (flag.enabled_by_default) {
            genf(f, "#define %s 1\n", flag.macro);
        } else {
            genf(f, "// #define %s 1\n", flag.macro);
        }
    }

    fprintf(f, "\n//// Plugins\n");
    Nob_File_Paths plugins = {0};
    if (nob_read_entire_dir("src/plugins", &plugins)) {
        for (size_t i = 0; i < plugins.count; ++i) {
            const char *plugin_name = plugins.items[i];
            if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0) continue;
            char macro[256];
            sprintf(macro, "CROSSWEB_PLUGIN_%s", plugin_name);
            for (size_t j = strlen("CROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
                macro[j] = toupper(macro[j]);
            }
            genf(f, "#define %s 1\n", macro);
        }
        da_free(plugins);
    }

    fprintf(f, "\n#endif // CROSSWEB_CONFIG_H_\n");
    fclose(f);
    return true;
}

bool generate_config_logger(const char *file_path)
{
    nob_log(NOB_INFO, "Generating %s", file_path);
    FILE *f = fopen(file_path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not generate %s: %s", file_path, strerror(errno));
        return false;
    }

    genf(f, "#include <stdio.h>\n");
    genf(f, "#include \"../build/config.h\"\n");
    genf(f, "\n");
    genf(f, "int main(int argc, char **argv) {\n");
    genf(f, "    (void)argc; (void)argv;\n");
    genf(f, "    printf(\"Crossweb Build Config:\\n\");\n");
    for (size_t i = 0; i < NOB_ARRAY_LEN(feature_flags); ++i) {
        Feature_Flag flag = feature_flags[i];
        genf(f, "    printf(\"  %s: %%s\\n\", %s ? \"enabled\" : \"disabled\");\n", flag.name, flag.macro);
    }
    for (size_t i = 0; i < NOB_ARRAY_LEN(target_flags); ++i) {
        Target_Flag flag = target_flags[i];
        genf(f, "#ifdef %s\n", flag.macro);
        genf(f, "    printf(\"  %s: enabled\\n\");\n", flag.name);
        genf(f, "#else\n");
        genf(f, "    printf(\"  %s: disabled\\n\");\n", flag.name);
        genf(f, "#endif\n");
    }

    genf(f, "\n    // Plugins\n");
    Nob_File_Paths plugins = {0};
    if (nob_read_entire_dir("src/plugins", &plugins)) {
        for (size_t i = 0; i < plugins.count; ++i) {
            const char *plugin_name = plugins.items[i];
            if (strcmp(plugin_name, ".") == 0 || strcmp(plugin_name, "..") == 0) continue;
            char macro[256];
            sprintf(macro, "CROSSWEB_PLUGIN_%s", plugin_name);
            for (size_t j = strlen("CROSSWEB_PLUGIN_"); j < strlen(macro); ++j) {
                macro[j] = toupper(macro[j]);
            }
            genf(f, "#ifdef %s\n", macro);
            genf(f, "    printf(\"  plugin-%s: enabled\\n\");\n", plugin_name);
            genf(f, "#else\n");
            genf(f, "    printf(\"  plugin-%s: disabled\\n\");\n", plugin_name);
            genf(f, "#endif\n");
        }
        da_free(plugins);
    }

    genf(f, "    return 0;\n");
    genf(f, "}\n");

    fclose(f);
    return true;
}
