#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
// #define NOB_EXPERIMENTAL_DELETE_OLD // Doesn't work on Windows
// #define NOB_WARN_DEPRECATED
#include "./thirdparty/nob.h"
#include "./src_build/configurer.c"
#include "./src_build/nob_cli.c"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./thirdparty/nob.h", "./src_build/configurer.c");
    const char *target_string = NULL;
    int config_exists = file_exists(CONFIG_PATH);
    if (config_exists < 0) return 1;
    if (config_exists == 0) {
        if (!generate_default_config(CONFIG_PATH)) return 1;
    } else {
        nob_log(INFO, "file `%s` already exists", CONFIG_PATH);
    }

    if (!generate_config_logger("build/config_logger.c")) return 1;

    int cli_result = nob_cli_main(argc, argv);
    if (cli_result == 1) return 1; // CLI signaled error
    if (cli_result == 0) return 0; // CLI already handled a command

    if (cli_result != 2) {
        // cli_result returned a build target as a string (i.e. "CROSSWEB_TARGET_WIN64_GCC")
        // Append "-D " to the beginning of the  cli_result string
        target_string = nob_temp_sprintf("./src_build/nob_stage2.c -D %s", cli_result);
    } else {
        // Default target if none specified
        target_string = "./src_build/nob_stage2.c";
    }


    Cmd cmd = {0};
    const char *stage2_binary = "build/nob_stage2";
    cmd_append(&cmd, NOB_REBUILD_URSELF(stage2_binary, target_string));
    cmd_append(&cmd, stage2_binary);
    da_append_many(&cmd, argv, argc);
    if (!cmd_run(&cmd)) return 1;

    return 0;
}
