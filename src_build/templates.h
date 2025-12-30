#ifndef TEMPLATES_H_
#define TEMPLATES_H_

#include <stdbool.h>

bool replace_package_placeholders_in_file(const char *file_path, const char *package_name);
bool copy_template_with_replacements(const char *template_dir, const char *target_dir, const char *package_name, const char *app_name);

#endif // TEMPLATES_H_