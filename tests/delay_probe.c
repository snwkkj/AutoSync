#include "core.h"

#include <stdio.h>

static void print_progress(double progress, const char *stage, void *user_data)
{
    (void)user_data;
    (void)printf("progress=%.0f%% stage=%s\n", progress * 100.0, stage);
}

int main(int argc, char **argv)
{
    autosync_analysis_request request;
    autosync_analysis_summary summary;
    autosync_status status;

    if (argc != 3) {
        (void)fprintf(stderr, "usage: %s reference source\n", argv[0]);
        return 2;
    }
    request.hq_media_path = argv[1];
    request.source_media_path = argv[2];
    request.subtitle_path = NULL;
    request.progress_callback = print_progress;
    request.progress_user_data = NULL;
    status = autosync_analyze(&request, &summary);
    (void)printf("status=%d delay_ms=%.0f confidence=%.3f message=%s\n",
                 status, summary.measured_offset_ms, summary.confidence,
                 summary.message);
    return status == AUTOSYNC_STATUS_OK ? 0 : 1;
}
