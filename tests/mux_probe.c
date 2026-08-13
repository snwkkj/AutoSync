#include "core.h"

#include <stdio.h>
#include <stdlib.h>

static void print_progress(double progress, const char *stage, void *user_data)
{
    (void)user_data;
    (void)printf("progress=%.0f%% stage=%s\n", progress * 100.0, stage);
}

int main(int argc, char **argv)
{
    autosync_mux_request request;
    autosync_mux_result result;
    autosync_status status;

    if (argc < 5 || argc > 7) {
        (void)fprintf(stderr,
                      "usage: %s video audio output delay_ms [audio-only] [codec]\n",
                      argv[0]);
        return 2;
    }
    request.video_path = argv[1];
    request.audio_path = argv[2];
    request.output_path = argv[3];
    request.delay_ms = strtod(argv[4], NULL);
    request.audio_language = "por";
    request.default_track = 1;
    request.preserve_chapters = 1;
    request.remove_tags = 1;
    request.audio_only = argc == 6 ? 1 : 0;
    request.reencode_audio = argc == 7 ? 1 : 0;
    request.audio_codec = argc == 7 ? argv[6] : "eac3";
    request.audio_bitrate_kbps = 640;
    request.progress_callback = print_progress;
    request.progress_user_data = NULL;
    status = autosync_mux(&request, &result);
    (void)printf("status=%d exit=%d message=%s\n", status,
                 result.process_exit_code, result.message);
    return status == AUTOSYNC_STATUS_OK ? 0 : 1;
}
