#ifndef PLUGINS_H_
#define PLUGINS_H_

#include <stdbool.h>

bool copy_plugins_for_android(void);
bool generate_proguard_rules(const char *package_name);

#endif // PLUGINS_H_