#ifndef AUTOSYNC_CORE_H
#define AUTOSYNC_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUTOSYNC_CORE_VERSION_MAJOR 0
#define AUTOSYNC_CORE_VERSION_MINOR 1
#define AUTOSYNC_CORE_VERSION_PATCH 0

typedef void (*autosync_progress_callback)(double progress,
                                           const char *stage,
                                           void *user_data);

typedef enum autosync_status {
    AUTOSYNC_STATUS_OK = 0,
    AUTOSYNC_STATUS_INVALID_ARGUMENT,
    AUTOSYNC_STATUS_FILE_NOT_FOUND,
    AUTOSYNC_STATUS_DEPENDENCY_MISSING,
    AUTOSYNC_STATUS_ANALYSIS_FAILED,
    AUTOSYNC_STATUS_ENCODING_FAILED,
    AUTOSYNC_STATUS_MULTIPLEXING_FAILED,
    AUTOSYNC_STATUS_CANCELLED,
    AUTOSYNC_STATUS_NOT_IMPLEMENTED
} autosync_status;

typedef struct autosync_analysis_request {
    const char *hq_media_path;
    const char *source_media_path;
    const char *subtitle_path;
    autosync_progress_callback progress_callback;
    void *progress_user_data;
} autosync_analysis_request;

typedef struct autosync_analysis_summary {
    autosync_status status;
    double measured_offset_ms;
    double scale;
    unsigned int anchor_count;
    double confidence;
    char recommended_method[64];
    char message[256];
} autosync_analysis_summary;

typedef struct autosync_mux_request {
    const char *video_path;
    const char *audio_path;
    const char *output_path;
    double delay_ms;
    const char *audio_language;
    int default_track;
    int preserve_chapters;
    int remove_tags;
    int audio_only;
    int reencode_audio;
    const char *audio_codec;
    int audio_bitrate_kbps;
    autosync_progress_callback progress_callback;
    void *progress_user_data;
} autosync_mux_request;

typedef struct autosync_mux_result {
    autosync_status status;
    int process_exit_code;
    char message[256];
} autosync_mux_result;

const char *autosync_core_version(void);
const char *autosync_status_string(autosync_status status);
void autosync_reset_cancel(void);
void autosync_request_cancel(void);
int autosync_cancel_requested(void);
autosync_status autosync_validate_request(const autosync_analysis_request *request,
                                      char *message,
                                      size_t message_capacity);
autosync_status autosync_analyze(const autosync_analysis_request *request,
                             autosync_analysis_summary *summary);
autosync_status autosync_mux(const autosync_mux_request *request,
                             autosync_mux_result *result);
/* Compatibility alias retained for early users of the 0.1 API. */
autosync_status autosync_mux_lossless(const autosync_mux_request *request,
                                      autosync_mux_result *result);

#ifdef __cplusplus
}
#endif

#endif
