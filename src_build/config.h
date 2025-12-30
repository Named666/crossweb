#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

const char *get_package_name(void);
bool is_plugin_enabled(const char *plugin_name);
bool validate_android_init(void);

#endif // CONFIG_H_