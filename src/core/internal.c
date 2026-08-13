#include "internal.h"

#include <stdio.h>

void copy_message(char *destination, size_t capacity, const char *message)
{
    if (destination == NULL || capacity == 0) {
        return;
    }
    (void)snprintf(destination, capacity, "%s", message != NULL ? message : "");
}

void report_progress(autosync_progress_callback callback, void *user_data,
                     double progress, const char *stage)
{
    if (callback != NULL) {
        callback(progress, stage, user_data);
    }
}

void format_ffmpeg_decimal(char *destination, size_t capacity, double value)
{
    size_t index;
    (void)snprintf(destination, capacity, "%.6f", value);
    /* FFmpeg always expects a decimal point, independent of the process locale. */
    for (index = 0; destination[index] != '\0'; ++index) {
        if (destination[index] == ',') {
            destination[index] = '.';
        }
    }
}

int path_is_present(const char *path)
{
    FILE *file;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    (void)fclose(file);
    return 1;
}
