#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "core.h"

#include "internal.h"

#include <math.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

static void prepare_external_process(void)
{
    (void)prctl(PR_SET_PDEATHSIG, SIGKILL);
    (void)setpgid(0, 0);
}

static void stop_external_process(pid_t child)
{
    int status;
    int attempt;
    (void)kill(-child, SIGTERM);
    (void)kill(child, SIGTERM);
    for (attempt = 0; attempt < 10; ++attempt) {
        if (waitpid(child, &status, WNOHANG) != 0) {
            return;
        }
        (void)usleep(20000);
    }
    (void)kill(-child, SIGKILL);
    (void)kill(child, SIGKILL);
    (void)waitpid(child, &status, 0);
}

static int supported_audio_codec(const char *codec)
{
    return codec != NULL &&
           (strcmp(codec, "eac3") == 0 || strcmp(codec, "ac3") == 0 ||
            strcmp(codec, "aac") == 0 || strcmp(codec, "libopus") == 0 ||
            strcmp(codec, "flac") == 0);
}

static int encode_audio_file(const autosync_mux_request *request,
                             char *temporary_path, size_t path_capacity)
{
    char template_path[] = "/tmp/autosync-audio-XXXXXX.mka";
    char bitrate[32];
    int file_descriptor;
    pid_t child;
    int child_status = 0;

    if (!supported_audio_codec(request->audio_codec)) {
        return 0;
    }
    file_descriptor = mkstemps(template_path, 4);
    if (file_descriptor < 0) {
        return 0;
    }
    (void)close(file_descriptor);
    (void)snprintf(temporary_path, path_capacity, "%s", template_path);
    (void)snprintf(bitrate, sizeof(bitrate), "%dk",
                   request->audio_bitrate_kbps > 0
                       ? request->audio_bitrate_kbps
                       : 640);

    report_progress(request->progress_callback, request->progress_user_data,
                    0.03, "Preparing audio encoder");
    child = fork();
    if (child == 0) {
        prepare_external_process();
        if (strcmp(request->audio_codec, "flac") == 0) {
            (void)execlp("ffmpeg", "ffmpeg", "-v", "error", "-nostdin", "-y",
                         "-i", request->audio_path, "-map", "0:a:0", "-vn", "-sn",
                         "-dn", "-c:a", "flac", template_path, (char *)NULL);
        } else {
            (void)execlp("ffmpeg", "ffmpeg", "-v", "error", "-nostdin", "-y",
                         "-i", request->audio_path, "-map", "0:a:0", "-vn", "-sn",
                         "-dn", "-c:a", request->audio_codec, "-b:a", bitrate,
                         template_path, (char *)NULL);
        }
        _exit(127);
    }
    if (child < 0) {
        (void)unlink(template_path);
        temporary_path[0] = '\0';
        return 0;
    }
    (void)setpgid(child, child);
    {
        pid_t wait_result;
        while ((wait_result = waitpid(child, &child_status, WNOHANG)) == 0) {
            if (autosync_cancel_requested()) {
                stop_external_process(child);
                (void)unlink(template_path);
                temporary_path[0] = '\0';
                return -1;
            }
            (void)usleep(50000);
        }
        if (wait_result < 0) {
            (void)unlink(template_path);
            temporary_path[0] = '\0';
            return 0;
        }
    }
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0) {
        (void)unlink(template_path);
        temporary_path[0] = '\0';
        return 0;
    }
    report_progress(request->progress_callback, request->progress_user_data,
                    0.40, "Audio encoding complete");
    return 1;
}

static void execute_mkvmerge(const autosync_mux_request *request,
                             const char *audio_path, char *sync_option)
{
    char *arguments[24];
    char language_option[32];
    size_t count = 0;

    arguments[count++] = (char *)"mkvmerge";
    arguments[count++] = (char *)"-o";
    arguments[count++] = (char *)request->output_path;
    if (!request->audio_only) {
        if (!request->preserve_chapters) {
            arguments[count++] = (char *)"--no-chapters";
        }
        if (request->remove_tags) {
            arguments[count++] = (char *)"--no-global-tags";
            arguments[count++] = (char *)"--no-track-tags";
        }
        arguments[count++] = (char *)request->video_path;
    }
    arguments[count++] = (char *)"--sync";
    arguments[count++] = sync_option;
    if (request->audio_language != NULL && request->audio_language[0] != '\0') {
        (void)snprintf(language_option, sizeof(language_option), "0:%s",
                       request->audio_language);
        arguments[count++] = (char *)"--language";
        arguments[count++] = language_option;
    }
    if (request->default_track) {
        arguments[count++] = (char *)"--default-track-flag";
        arguments[count++] = (char *)"0:yes";
    }
    if (request->remove_tags) {
        arguments[count++] = (char *)"--no-global-tags";
        arguments[count++] = (char *)"--no-track-tags";
    }
    arguments[count++] = (char *)audio_path;
    arguments[count] = NULL;
    (void)execvp("mkvmerge", arguments);
}

static void remove_temporary_audio(const char *path)
{
    if (path[0] != '\0') {
        (void)unlink(path);
    }
}

autosync_status autosync_mux(const autosync_mux_request *request,
                             autosync_mux_result *result)
{
    char sync_option[64];
    int output_pipe[2];
    pid_t child;
    int child_status = 0;
    char output_buffer[256];
    char temporary_audio_path[256] = {0};
    const char *mux_audio_path;
    size_t buffered = 0;

    if (result == NULL) {
        return AUTOSYNC_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(result, 0, sizeof(*result));
    if (request == NULL || !path_is_present(request->video_path) ||
        !path_is_present(request->audio_path) || request->output_path == NULL ||
        request->output_path[0] == '\0') {
        result->status = AUTOSYNC_STATUS_INVALID_ARGUMENT;
        copy_message(result->message, sizeof(result->message),
                     "Invalid paths for multiplexing.");
        return result->status;
    }

    mux_audio_path = request->audio_path;
    if (request->reencode_audio) {
        if (!supported_audio_codec(request->audio_codec)) {
            result->status = AUTOSYNC_STATUS_INVALID_ARGUMENT;
            copy_message(result->message, sizeof(result->message),
                         "Unsupported audio codec.");
            return result->status;
        }
        const int encode_status = encode_audio_file(
            request, temporary_audio_path, sizeof(temporary_audio_path));
        if (encode_status <= 0) {
            result->status = encode_status < 0
                                 ? AUTOSYNC_STATUS_CANCELLED
                                 : AUTOSYNC_STATUS_ENCODING_FAILED;
            copy_message(result->message, sizeof(result->message),
                         encode_status < 0
                             ? "Audio encoding cancelled."
                             : "FFmpeg could not encode the selected audio track.");
            return result->status;
        }
        mux_audio_path = temporary_audio_path;
    }

    (void)snprintf(sync_option, sizeof(sync_option), "0:%lld",
                   (long long)llround(request->delay_ms));
    if (pipe(output_pipe) != 0) {
        remove_temporary_audio(temporary_audio_path);
        result->status = AUTOSYNC_STATUS_MULTIPLEXING_FAILED;
        copy_message(result->message, sizeof(result->message),
                     "Could not monitor mkvmerge.");
        return result->status;
    }
    child = fork();
    if (child == 0) {
        prepare_external_process();
        (void)dup2(output_pipe[1], STDOUT_FILENO);
        (void)dup2(output_pipe[1], STDERR_FILENO);
        (void)close(output_pipe[0]);
        (void)close(output_pipe[1]);
        execute_mkvmerge(request, mux_audio_path, sync_option);
        _exit(127);
    }
    (void)close(output_pipe[1]);
    if (child < 0) {
        (void)close(output_pipe[0]);
        remove_temporary_audio(temporary_audio_path);
        result->status = AUTOSYNC_STATUS_MULTIPLEXING_FAILED;
        copy_message(result->message, sizeof(result->message),
                     "Could not start mkvmerge.");
        return result->status;
    }
    (void)setpgid(child, child);
    (void)fcntl(output_pipe[0], F_SETFL,
                fcntl(output_pipe[0], F_GETFL, 0) | O_NONBLOCK);

    report_progress(request->progress_callback, request->progress_user_data,
                    request->reencode_audio ? 0.40 : 0.0,
                    request->audio_only ? "Preparing MKA file" : "Preparing MKV file");
    {
        int cancelled = 0;
        int stream_closed = 0;
        while (!stream_closed) {
            struct pollfd descriptor = {output_pipe[0], POLLIN | POLLHUP, 0};
            if (autosync_cancel_requested()) {
                cancelled = 1;
                stop_external_process(child);
                break;
            }
            if (poll(&descriptor, 1, 100) <= 0) {
                continue;
            }
            while (1) {
                char character;
                const ssize_t read_count = read(output_pipe[0], &character, 1);
                if (read_count <= 0) {
                    break;
                }
                if (character == '\r' || character == '\n') {
                    int percent;
                    output_buffer[buffered] = '\0';
                    if (sscanf(output_buffer, "Progress: %d%%", &percent) == 1 ||
                        sscanf(output_buffer, "Progresso: %d%%", &percent) == 1) {
                        const double base = request->reencode_audio ? 0.40 : 0.0;
                        const double range = request->reencode_audio ? 0.60 : 1.0;
                        report_progress(request->progress_callback,
                                        request->progress_user_data,
                                        base + range * ((double)percent / 100.0),
                                        request->reencode_audio
                                            ? "Multiplexing encoded audio"
                                            : "Multiplexing without re-encoding");
                    }
                    buffered = 0;
                } else if (buffered + 1 < sizeof(output_buffer)) {
                    output_buffer[buffered++] = character;
                }
            }
            stream_closed = (descriptor.revents & POLLHUP) != 0;
        }
        (void)close(output_pipe[0]);
        if (cancelled) {
            remove_temporary_audio(temporary_audio_path);
            (void)unlink(request->output_path);
            result->status = AUTOSYNC_STATUS_CANCELLED;
            copy_message(result->message, sizeof(result->message),
                         "Multiplexing cancelled; incomplete output removed.");
            return result->status;
        }
    }
    if (waitpid(child, &child_status, 0) < 0) {
        remove_temporary_audio(temporary_audio_path);
        result->status = AUTOSYNC_STATUS_MULTIPLEXING_FAILED;
        copy_message(result->message, sizeof(result->message),
                     "Could not wait for mkvmerge.");
        return result->status;
    }

    result->process_exit_code = WIFEXITED(child_status) ? WEXITSTATUS(child_status) : -1;
    remove_temporary_audio(temporary_audio_path);
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) > 1) {
        result->status = WIFEXITED(child_status) && WEXITSTATUS(child_status) == 127
                             ? AUTOSYNC_STATUS_DEPENDENCY_MISSING
                             : AUTOSYNC_STATUS_MULTIPLEXING_FAILED;
        (void)snprintf(result->message, sizeof(result->message),
                       "mkvmerge exited with code %d.", result->process_exit_code);
        return result->status;
    }

    result->status = AUTOSYNC_STATUS_OK;
    report_progress(request->progress_callback, request->progress_user_data,
                    1.0, "Multiplexing complete");
    copy_message(result->message, sizeof(result->message),
                 request->audio_only
                     ? (request->reencode_audio
                            ? "MKA created with the selected audio encoding settings."
                            : "MKA created without re-encoding the audio track.")
                     : (request->reencode_audio
                            ? "MKV created with the selected audio encoding settings."
                            : "MKV created without re-encoding the audio track."));
    return result->status;
}

autosync_status autosync_mux_lossless(const autosync_mux_request *request,
                                      autosync_mux_result *result)
{
    return autosync_mux(request, result);
}
