#include "core.h"

#include <signal.h>

static volatile sig_atomic_t cancellation_requested = 0;

const char *autosync_core_version(void)
{
    return "0.1.0";
}

const char *autosync_status_string(autosync_status status)
{
    switch (status) {
    case AUTOSYNC_STATUS_OK:
        return "ok";
    case AUTOSYNC_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case AUTOSYNC_STATUS_FILE_NOT_FOUND:
        return "file not found";
    case AUTOSYNC_STATUS_DEPENDENCY_MISSING:
        return "missing dependency";
    case AUTOSYNC_STATUS_ANALYSIS_FAILED:
        return "analysis failed";
    case AUTOSYNC_STATUS_ENCODING_FAILED:
        return "encoding failed";
    case AUTOSYNC_STATUS_MULTIPLEXING_FAILED:
        return "multiplexing failed";
    case AUTOSYNC_STATUS_CANCELLED:
        return "cancelled";
    case AUTOSYNC_STATUS_NOT_IMPLEMENTED:
        return "not implemented";
    default:
        return "unknown error";
    }
}

void autosync_reset_cancel(void)
{
    cancellation_requested = 0;
}

void autosync_request_cancel(void)
{
    cancellation_requested = 1;
}

int autosync_cancel_requested(void)
{
    return cancellation_requested != 0;
}
