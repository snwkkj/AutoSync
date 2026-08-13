#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "core.h"

#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ANALYSIS_SAMPLE_RATE 8000
#define ENVELOPE_RATE 20
#define SAMPLES_PER_ENVELOPE (ANALYSIS_SAMPLE_RATE / ENVELOPE_RATE)
#define ANALYSIS_SECONDS 1200
#define MAX_DELAY_SECONDS 120

typedef struct envelope_buffer {
    double *values;
    size_t count;
} envelope_buffer;

typedef struct pcm_buffer {
    int16_t *samples;
    size_t count;
} pcm_buffer;

static void prepare_decoder_process(void)
{
    (void)prctl(PR_SET_PDEATHSIG, SIGKILL);
    (void)setpgid(0, 0);
}

static void cancel_decoder(pid_t child)
{
    (void)kill(-child, SIGKILL);
    (void)kill(child, SIGKILL);
}

static void free_envelope(envelope_buffer *envelope)
{
    free(envelope->values);
    envelope->values = NULL;
    envelope->count = 0;
}

static void free_pcm(pcm_buffer *pcm)
{
    free(pcm->samples);
    pcm->samples = NULL;
    pcm->count = 0;
}

static int decode_pcm_segment(const char *path, double start_seconds,
                              double duration_seconds, pcm_buffer *result)
{
    int output_pipe[2];
    pid_t child;
    FILE *stream;
    size_t capacity = 0;
    int child_status = 0;
    char start_text[64];
    char duration_text[64];

    result->samples = NULL;
    result->count = 0;
    format_ffmpeg_decimal(start_text, sizeof(start_text), start_seconds);
    format_ffmpeg_decimal(duration_text, sizeof(duration_text), duration_seconds);
    if (pipe(output_pipe) != 0) {
        return 0;
    }
    child = fork();
    if (child == 0) {
        prepare_decoder_process();
        (void)dup2(output_pipe[1], STDOUT_FILENO);
        (void)close(output_pipe[0]);
        (void)close(output_pipe[1]);
        (void)execlp("ffmpeg", "ffmpeg", "-v", "error", "-nostdin",
                     "-i", path, "-ss", start_text, "-t", duration_text,
                     "-map", "0:a:0", "-vn", "-sn", "-dn", "-ac", "1",
                     "-ar", "8000", "-af", "highpass=f=80,lowpass=f=3500",
                     "-f", "s16le", "-acodec", "pcm_s16le", "pipe:1",
                     (char *)NULL);
        _exit(127);
    }
    (void)close(output_pipe[1]);
    if (child < 0) {
        (void)close(output_pipe[0]);
        return 0;
    }
    (void)setpgid(child, child);
    stream = fdopen(output_pipe[0], "rb");
    if (stream == NULL) {
        (void)close(output_pipe[0]);
        (void)waitpid(child, &child_status, 0);
        return 0;
    }
    while (1) {
        size_t available;
        size_t read_count;
        if (result->count == capacity) {
            const size_t new_capacity = capacity == 0 ? 65536 : capacity * 2;
            int16_t *grown = realloc(result->samples, new_capacity * sizeof(*grown));
            if (grown == NULL) {
                (void)fclose(stream);
                (void)waitpid(child, &child_status, 0);
                free_pcm(result);
                return 0;
            }
            result->samples = grown;
            capacity = new_capacity;
        }
        available = capacity - result->count;
        read_count = fread(result->samples + result->count, sizeof(int16_t),
                           available, stream);
        result->count += read_count;
        if (autosync_cancel_requested()) {
            cancel_decoder(child);
            break;
        }
        if (read_count == 0) {
            break;
        }
    }
    (void)fclose(stream);
    (void)waitpid(child, &child_status, 0);
    return WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0 &&
           result->count >= (size_t)(ANALYSIS_SAMPLE_RATE * 5);
}

static int refine_waveform_delay(const char *reference_path, const char *source_path,
                                 double reference_duration, double source_duration,
                                 double coarse_delay_ms, double *precise_delay_ms,
                                 double *waveform_score)
{
    const double source_offset = -coarse_delay_ms / 1000.0;
    double overlap_start = fmax(2.0, 2.0 - source_offset);
    const double overlap_end = fmin(reference_duration - 2.0,
                                    source_duration - source_offset - 2.0);
    double window_duration = fmin(45.0, overlap_end - overlap_start);
    double reference_start;
    double source_start;
    pcm_buffer reference = {0};
    pcm_buffer source = {0};
    const int refine_radius = (int)(0.075 * ANALYSIS_SAMPLE_RATE);
    double best_score = -2.0;
    int best_lag = 0;
    int lag;

    if (window_duration < 5.0) {
        return 0;
    }
    reference_start = overlap_start + ((overlap_end - overlap_start) - window_duration) / 2.0;
    source_start = reference_start + source_offset;
    if (!decode_pcm_segment(reference_path, reference_start, window_duration, &reference) ||
        !decode_pcm_segment(source_path, source_start, window_duration, &source)) {
        free_pcm(&reference);
        free_pcm(&source);
        return 0;
    }

    for (lag = -refine_radius; lag <= refine_radius; ++lag) {
        const size_t ref_start = lag < 0 ? (size_t)(-lag) : 0;
        const size_t src_start = lag > 0 ? (size_t)lag : 0;
        const size_t ref_left = reference.count - ref_start;
        const size_t src_left = source.count - src_start;
        const size_t count = ref_left < src_left ? ref_left : src_left;
        double dot = 0.0;
        double ref_energy = 0.0;
        double src_energy = 0.0;
        size_t index;
        double score;
        for (index = 0; index < count; index += 2) {
            const double a = (double)reference.samples[ref_start + index];
            const double b = (double)source.samples[src_start + index];
            dot += a * b;
            ref_energy += a * a;
            src_energy += b * b;
        }
        if (ref_energy <= 1e-12 || src_energy <= 1e-12) {
            continue;
        }
        score = dot / sqrt(ref_energy * src_energy);
        if (score > best_score) {
            best_score = score;
            best_lag = lag;
        }
    }
    free_pcm(&reference);
    free_pcm(&source);
    if (best_score < 0.015) {
        return 0;
    }
    *precise_delay_ms = -(source_offset +
                          (double)best_lag / ANALYSIS_SAMPLE_RATE) * 1000.0;
    *waveform_score = best_score;
    return 1;
}

static int decode_envelope(const char *path, envelope_buffer *result)
{
    int output_pipe[2];
    pid_t child;
    FILE *stream;
    int16_t samples[4096];
    size_t used = 0;
    size_t capacity = 0;
    double energy = 0.0;
    size_t energy_samples = 0;
    int child_status = 0;

    result->values = NULL;
    result->count = 0;
    if (pipe(output_pipe) != 0) {
        return 0;
    }

    child = fork();
    if (child == 0) {
        prepare_decoder_process();
        (void)dup2(output_pipe[1], STDOUT_FILENO);
        (void)close(output_pipe[0]);
        (void)close(output_pipe[1]);
        (void)execlp("ffmpeg", "ffmpeg", "-v", "error", "-nostdin",
                     "-i", path, "-map", "0:a:0", "-vn", "-sn", "-dn",
                     "-t", "1200", "-ac", "1", "-ar", "8000",
                     "-af", "highpass=f=80,lowpass=f=3500",
                     "-f", "s16le", "-acodec", "pcm_s16le", "pipe:1",
                     (char *)NULL);
        _exit(127);
    }
    (void)close(output_pipe[1]);
    if (child < 0) {
        (void)close(output_pipe[0]);
        return 0;
    }
    (void)setpgid(child, child);

    stream = fdopen(output_pipe[0], "rb");
    if (stream == NULL) {
        (void)close(output_pipe[0]);
        (void)waitpid(child, &child_status, 0);
        return 0;
    }

    while (1) {
        const size_t read_count = fread(samples, sizeof(samples[0]),
                                        sizeof(samples) / sizeof(samples[0]), stream);
        size_t index;
        for (index = 0; index < read_count; ++index) {
            const double value = (double)samples[index] / 32768.0;
            energy += value * value;
            ++energy_samples;
            if (energy_samples == SAMPLES_PER_ENVELOPE) {
                if (used == capacity) {
                    const size_t new_capacity = capacity == 0 ? 4096 : capacity * 2;
                    double *grown = realloc(result->values,
                                            new_capacity * sizeof(*grown));
                    if (grown == NULL) {
                        (void)fclose(stream);
                        (void)waitpid(child, &child_status, 0);
                        free_envelope(result);
                        return 0;
                    }
                    result->values = grown;
                    capacity = new_capacity;
                }
                result->values[used++] = log1p(1000.0 * sqrt(energy / energy_samples));
                energy = 0.0;
                energy_samples = 0;
            }
        }
        if (autosync_cancel_requested()) {
            cancel_decoder(child);
            break;
        }
        if (read_count == 0) {
            break;
        }
    }
    (void)fclose(stream);
    (void)waitpid(child, &child_status, 0);
    result->count = used;
    return WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0 && used >= 200;
}

static void normalize_onsets(envelope_buffer *envelope)
{
    size_t index;
    double mean = 0.0;
    double norm = 0.0;
    double previous = envelope->values[0];

    for (index = 0; index < envelope->count; ++index) {
        const double current = envelope->values[index];
        const double delta = current - previous;
        envelope->values[index] = delta > 0.0 ? delta : 0.0;
        previous = current;
        mean += envelope->values[index];
    }
    mean /= (double)envelope->count;
    for (index = 0; index < envelope->count; ++index) {
        envelope->values[index] -= mean;
        norm += envelope->values[index] * envelope->values[index];
    }
    norm = sqrt(norm / (double)envelope->count);
    if (norm > 1e-12) {
        for (index = 0; index < envelope->count; ++index) {
            envelope->values[index] /= norm;
        }
    }
}

static int estimate_delay(const envelope_buffer *reference,
                          const envelope_buffer *source,
                          double *delay_ms,
                          double *confidence)
{
    const int max_lag = MAX_DELAY_SECONDS * ENVELOPE_RATE;
    const size_t minimum_overlap = 30 * ENVELOPE_RATE;
    double best_score = -2.0;
    double second_score = -2.0;
    int best_lag = 0;
    int lag;

    for (lag = -max_lag; lag <= max_lag; ++lag) {
        const size_t ref_start = lag < 0 ? (size_t)(-lag) : 0;
        const size_t src_start = lag > 0 ? (size_t)lag : 0;
        const size_t ref_left = reference->count - (ref_start < reference->count ? ref_start : reference->count);
        const size_t src_left = source->count - (src_start < source->count ? src_start : source->count);
        const size_t count = ref_left < src_left ? ref_left : src_left;
        double dot = 0.0;
        double ref_energy = 0.0;
        double src_energy = 0.0;
        size_t index;
        double score;
        if (count < minimum_overlap) {
            continue;
        }
        for (index = 0; index < count; ++index) {
            const double a = reference->values[ref_start + index];
            const double b = source->values[src_start + index];
            dot += a * b;
            ref_energy += a * a;
            src_energy += b * b;
        }
        if (ref_energy <= 1e-12 || src_energy <= 1e-12) {
            continue;
        }
        score = dot / sqrt(ref_energy * src_energy);
        if (score > best_score) {
            second_score = best_score;
            best_score = score;
            best_lag = lag;
        } else if (abs(lag - best_lag) > ENVELOPE_RATE && score > second_score) {
            second_score = score;
        }
    }
    if (best_score < 0.02) {
        return 0;
    }
    *delay_ms = -(double)best_lag * (1000.0 / ENVELOPE_RATE);
    *confidence = fmax(0.0, fmin(1.0, best_score * 1.5 +
                                 fmax(0.0, best_score - second_score) * 2.0));
    return 1;
}

autosync_status autosync_validate_request(const autosync_analysis_request *request,
                                      char *message,
                                      size_t message_capacity)
{
    if (request == NULL) {
        copy_message(message, message_capacity, "Missing analysis request.");
        return AUTOSYNC_STATUS_INVALID_ARGUMENT;
    }

    if (!path_is_present(request->hq_media_path)) {
        copy_message(message, message_capacity, "Select a valid reference file.");
        return AUTOSYNC_STATUS_FILE_NOT_FOUND;
    }

    if (!path_is_present(request->source_media_path)) {
        copy_message(message, message_capacity, "Select a valid audio source.");
        return AUTOSYNC_STATUS_FILE_NOT_FOUND;
    }

    if (request->subtitle_path != NULL && request->subtitle_path[0] != '\0' &&
        !path_is_present(request->subtitle_path)) {
        copy_message(message, message_capacity, "The subtitle file was not found.");
        return AUTOSYNC_STATUS_FILE_NOT_FOUND;
    }

    copy_message(message, message_capacity, "Files are ready for analysis.");
    return AUTOSYNC_STATUS_OK;
}

autosync_status autosync_analyze(const autosync_analysis_request *request,
                             autosync_analysis_summary *summary)
{
    char validation_message[256];
    autosync_status status;

    if (summary == NULL) {
        return AUTOSYNC_STATUS_INVALID_ARGUMENT;
    }

    (void)memset(summary, 0, sizeof(*summary));
    summary->scale = 1.0;

    status = autosync_validate_request(request, validation_message,
                                     sizeof(validation_message));
    if (status != AUTOSYNC_STATUS_OK) {
        summary->status = status;
        copy_message(summary->message, sizeof(summary->message), validation_message);
        return status;
    }

    {
        envelope_buffer reference = {0};
        envelope_buffer source = {0};
        double delay_ms = 0.0;
        double confidence = 0.0;
        report_progress(request->progress_callback, request->progress_user_data,
                        0.05, "Reading reference audio");
        if (!decode_envelope(request->hq_media_path, &reference)) {
            free_envelope(&reference);
            free_envelope(&source);
            summary->status = autosync_cancel_requested()
                                  ? AUTOSYNC_STATUS_CANCELLED
                                  : AUTOSYNC_STATUS_ANALYSIS_FAILED;
            copy_message(summary->message, sizeof(summary->message),
                         autosync_cancel_requested()
                             ? "Analysis cancelled."
                             : "FFmpeg could not read one of the audio tracks.");
            return summary->status;
        }
        report_progress(request->progress_callback, request->progress_user_data,
                        0.30, "Reading audio to be synchronized");
        if (!decode_envelope(request->source_media_path, &source)) {
            free_envelope(&reference);
            free_envelope(&source);
            summary->status = autosync_cancel_requested()
                                  ? AUTOSYNC_STATUS_CANCELLED
                                  : AUTOSYNC_STATUS_ANALYSIS_FAILED;
            copy_message(summary->message, sizeof(summary->message),
                         autosync_cancel_requested()
                             ? "Analysis cancelled."
                             : "FFmpeg could not read one of the audio tracks.");
            return summary->status;
        }
        report_progress(request->progress_callback, request->progress_user_data,
                        0.55, "Comparing audio envelopes");
        normalize_onsets(&reference);
        normalize_onsets(&source);
        if (!estimate_delay(&reference, &source, &delay_ms, &confidence)) {
            free_envelope(&reference);
            free_envelope(&source);
            summary->status = AUTOSYNC_STATUS_ANALYSIS_FAILED;
            copy_message(summary->message, sizeof(summary->message),
                         "Not enough correlation was found for a reliable delay.");
            return summary->status;
        }
        {
            double precise_delay_ms = delay_ms;
            double waveform_score = 0.0;
            report_progress(request->progress_callback, request->progress_user_data,
                            0.75, "Refining waveform sample by sample");
            if (refine_waveform_delay(request->hq_media_path,
                                      request->source_media_path,
                                      (double)reference.count / ENVELOPE_RATE,
                                      (double)source.count / ENVELOPE_RATE,
                                      delay_ms, &precise_delay_ms, &waveform_score)) {
                delay_ms = precise_delay_ms;
                confidence = fmax(confidence, fmin(0.99, fabs(waveform_score)));
            }
        }
        free_envelope(&reference);
        free_envelope(&source);
        summary->measured_offset_ms = delay_ms;
        summary->confidence = confidence;
    }

    summary->status = AUTOSYNC_STATUS_OK;
    summary->anchor_count = 1;
    copy_message(summary->recommended_method,
                 sizeof(summary->recommended_method), "constant-delay");
    (void)snprintf(summary->message, sizeof(summary->message),
                   "Constant delay found: %+.3f ms (%.0f%% confidence).",
                   summary->measured_offset_ms, summary->confidence * 100.0);
    report_progress(request->progress_callback, request->progress_user_data,
                    1.0, "Delay found");
    return summary->status;
}
