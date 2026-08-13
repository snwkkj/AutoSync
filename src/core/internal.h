#ifndef AUTOSYNC_INTERNAL_H
#define AUTOSYNC_INTERNAL_H

#include "core.h"

#include <stddef.h>

void copy_message(char *destination, size_t capacity, const char *message);
void report_progress(autosync_progress_callback callback, void *user_data,
                     double progress, const char *stage);
void format_ffmpeg_decimal(char *destination, size_t capacity, double value);
int path_is_present(const char *path);

#endif
