#ifndef ANDROID_BUILD_H_
#define ANDROID_BUILD_H_

#include <stdbool.h>

bool collect_c_files(const char *dir, Nob_File_Paths *files, const char *base);
bool generate_cmake_lists(const char *path);
bool android_build_crossweb(void);
bool android_build_dist(void);

#endif // ANDROID_BUILD_H_