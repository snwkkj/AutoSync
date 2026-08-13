#include "core.h"

#include <stdio.h>
#include <string.h>

static int test_version_and_status_strings(void)
{
    int status;

    if (strcmp(autosync_core_version(), "0.1.0") != 0) {
        (void)fprintf(stderr, "unexpected engine version\n");
        return 1;
    }
    for (status = AUTOSYNC_STATUS_OK;
         status <= AUTOSYNC_STATUS_NOT_IMPLEMENTED; ++status) {
        if (strcmp(autosync_status_string((autosync_status)status),
                   "unknown error") == 0) {
            (void)fprintf(stderr, "missing string for status %d\n", status);
            return 2;
        }
    }
    if (strcmp(autosync_status_string((autosync_status)999),
               "unknown error") != 0) {
        (void)fprintf(stderr, "unknown status should use fallback string\n");
        return 3;
    }
    autosync_reset_cancel();
    if (autosync_cancel_requested()) {
        (void)fprintf(stderr, "cancel state should start cleared\n");
        return 12;
    }
    autosync_request_cancel();
    if (!autosync_cancel_requested()) {
        (void)fprintf(stderr, "cancel request was not recorded\n");
        return 13;
    }
    autosync_reset_cancel();
    return 0;
}

static int test_analysis_validation(void)
{
    autosync_analysis_request request = {0};
    autosync_analysis_summary summary;
    autosync_status status;
    char message[64];

    status = autosync_validate_request(NULL, message, sizeof(message));
    if (status != AUTOSYNC_STATUS_INVALID_ARGUMENT || message[0] == '\0') {
        (void)fprintf(stderr, "null analysis request should be rejected\n");
        return 4;
    }

    status = autosync_analyze(&request, &summary);
    if (status != AUTOSYNC_STATUS_FILE_NOT_FOUND) {
        (void)fprintf(stderr, "validation should reject empty paths\n");
        return 5;
    }
    if (summary.message[0] == '\0') {
        (void)fprintf(stderr, "validation should provide a message\n");
        return 6;
    }
    if (autosync_analyze(&request, NULL) != AUTOSYNC_STATUS_INVALID_ARGUMENT) {
        (void)fprintf(stderr, "null analysis result should be rejected\n");
        return 7;
    }
    return 0;
}

static int test_mux_validation(void)
{
    autosync_mux_request request = {0};
    autosync_mux_result result;

    if (autosync_mux(&request, NULL) != AUTOSYNC_STATUS_INVALID_ARGUMENT) {
        (void)fprintf(stderr, "null mux result should be rejected\n");
        return 8;
    }
    if (autosync_mux(&request, &result) !=
            AUTOSYNC_STATUS_INVALID_ARGUMENT ||
        result.message[0] == '\0') {
        (void)fprintf(stderr, "invalid mux paths should be rejected\n");
        return 9;
    }
    request.video_path = "/dev/null";
    request.audio_path = "/dev/null";
    request.output_path = "/tmp/autosync-invalid-codec.mkv";
    request.reencode_audio = 1;
    request.audio_codec = "invalid";
    if (autosync_mux(&request, &result) != AUTOSYNC_STATUS_INVALID_ARGUMENT) {
        (void)fprintf(stderr, "unsupported codec should be rejected\n");
        return 10;
    }
    request.video_path = NULL;
    if (autosync_mux_lossless(&request, &result) !=
        AUTOSYNC_STATUS_INVALID_ARGUMENT) {
        (void)fprintf(stderr, "compatibility mux alias should remain callable\n");
        return 11;
    }
    return 0;
}

int main(void)
{
    int result = test_version_and_status_strings();
    if (result != 0) {
        return result;
    }
    result = test_analysis_validation();
    if (result != 0) {
        return result;
    }
    result = test_mux_validation();
    if (result != 0) {
        return result;
    }

    return 0;
}
