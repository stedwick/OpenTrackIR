#include "opentrackir/tir5_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

#define OTIR_TRACKIR_SESSION_MAX_PREVIEW_FRAMES_PER_SECOND 30.0
#define OTIR_TRACKIR_SESSION_LOW_POWER_FRAMES_PER_SECOND 2.0

typedef struct trackir_session_runtime_config {
    bool video_enabled;
    double maximum_tracking_frames_per_second;
    int minimum_blob_area_points;
    otir_tir5v3_centroid_mode centroid_mode;
    bool low_power_mode_enabled;
} trackir_session_runtime_config;

#ifdef _WIN32
typedef CRITICAL_SECTION otir_mutex;
typedef HANDLE otir_thread;
#define OTIR_THREAD_RESULT DWORD
#define OTIR_THREAD_CALL WINAPI
#define OTIR_THREAD_RETURN 0
typedef OTIR_THREAD_RESULT (OTIR_THREAD_CALL *otir_thread_entry)(void *);
static int otir_mutex_init(otir_mutex *mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}
static void otir_mutex_destroy(otir_mutex *mutex) {
    DeleteCriticalSection(mutex);
}
static void otir_mutex_lock(otir_mutex *mutex) {
    EnterCriticalSection(mutex);
}
static void otir_mutex_unlock(otir_mutex *mutex) {
    LeaveCriticalSection(mutex);
}
static int otir_thread_create(otir_thread *thread, otir_thread_entry entry, void *context) {
    *thread = CreateThread(NULL, 0, entry, context, 0, NULL);
    return *thread == NULL ? -1 : 0;
}
static void otir_thread_join(otir_thread thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}
#else
typedef pthread_mutex_t otir_mutex;
typedef pthread_t otir_thread;
#define OTIR_THREAD_RESULT void *
#define OTIR_THREAD_CALL
#define OTIR_THREAD_RETURN NULL
typedef OTIR_THREAD_RESULT (*otir_thread_entry)(void *);
static int otir_mutex_init(otir_mutex *mutex) {
    return pthread_mutex_init(mutex, NULL);
}
static void otir_mutex_destroy(otir_mutex *mutex) {
    pthread_mutex_destroy(mutex);
}
static void otir_mutex_lock(otir_mutex *mutex) {
    pthread_mutex_lock(mutex);
}
static void otir_mutex_unlock(otir_mutex *mutex) {
    pthread_mutex_unlock(mutex);
}
static int otir_thread_create(otir_thread *thread, otir_thread_entry entry, void *context) {
    return pthread_create(thread, NULL, entry, context);
}
static void otir_thread_join(otir_thread thread) {
    pthread_join(thread, NULL);
}
#endif

struct otir_trackir_session {
    otir_mutex mutex;
    otir_thread worker;
    bool worker_started;
    bool worker_exited;
    bool stop_requested;
    bool video_enabled;
    double maximum_tracking_frames_per_second;
    int minimum_blob_area_points;
    otir_tir5v3_centroid_mode centroid_mode;
    bool low_power_mode_enabled;
    otir_trackir_session_snapshot snapshot;
    uint8_t preview_frame[OTIR_TRACKIR_SESSION_FRAME_BYTES];
};

static OTIR_THREAD_RESULT OTIR_THREAD_CALL trackir_session_worker_main(void *context);
static void trackir_session_reset_snapshot_locked(otir_trackir_session *session);
static void trackir_session_clear_preview_locked(otir_trackir_session *session);
static void trackir_session_set_error_locked(otir_trackir_session *session, const char *message);
static void trackir_session_set_failure_locked(
    otir_trackir_session *session,
    otir_status status,
    const char *operation
);
static void trackir_session_join_worker_if_exited(otir_trackir_session *session);
static bool trackir_session_stop_requested(otir_trackir_session *session);
static trackir_session_runtime_config trackir_session_runtime_config_snapshot(
    otir_trackir_session *session
);
static void trackir_copy_string(char *destination, size_t capacity, const char *source);
static void trackir_session_format_failure_message(
    otir_status status,
    const char *operation,
    char *buffer,
    size_t capacity
);
static double trackir_session_now_seconds(void);

otir_trackir_session *otir_trackir_session_create(void) {
    otir_trackir_session *session = calloc(1, sizeof(*session));

    if (session == NULL) {
        return NULL;
    }
    if (otir_mutex_init(&session->mutex) != 0) {
        free(session);
        return NULL;
    }

    otir_mutex_lock(&session->mutex);
    session->video_enabled = true;
    session->maximum_tracking_frames_per_second =
        otir_tir5v3_normalize_maximum_frames_per_second(0.0);
    session->minimum_blob_area_points =
        otir_tir5v3_default_blob_tracking_config().minimum_area_points;
    session->centroid_mode = otir_tir5v3_default_blob_tracking_config().centroid_mode;
    session->low_power_mode_enabled = false;
    trackir_session_reset_snapshot_locked(session);
    otir_mutex_unlock(&session->mutex);
    return session;
}

void otir_trackir_session_destroy(otir_trackir_session *session) {
    if (session == NULL) {
        return;
    }

    otir_trackir_session_stop(session, true);
    otir_mutex_destroy(&session->mutex);
    free(session);
}

otir_status otir_trackir_session_start(otir_trackir_session *session) {
    if (session == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    trackir_session_join_worker_if_exited(session);

    otir_mutex_lock(&session->mutex);

    if (session->worker_started) {
        otir_mutex_unlock(&session->mutex);
        return OTIR_STATUS_OK;
    }

    trackir_session_reset_snapshot_locked(session);
    session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STARTING;
    session->stop_requested = false;
    session->worker_exited = false;

    if (otir_thread_create(&session->worker, trackir_session_worker_main, session) != 0) {
        trackir_session_set_failure_locked(session, OTIR_STATUS_IO, "Start session");
        otir_mutex_unlock(&session->mutex);
        return OTIR_STATUS_IO;
    }

    session->worker_started = true;
    otir_mutex_unlock(&session->mutex);
    return OTIR_STATUS_OK;
}

void otir_trackir_session_stop(otir_trackir_session *session, bool wait_for_shutdown) {
    if (session == NULL) {
        return;
    }

    otir_thread worker = 0;
    bool should_join = false;

    otir_mutex_lock(&session->mutex);
    if (session->worker_started) {
        session->stop_requested = true;
        worker = session->worker;
        should_join = wait_for_shutdown;
    }
    otir_mutex_unlock(&session->mutex);

    if (should_join) {
        otir_thread_join(worker);

        otir_mutex_lock(&session->mutex);
        session->worker_started = false;
        session->worker_exited = false;
        trackir_session_reset_snapshot_locked(session);
        otir_mutex_unlock(&session->mutex);
        return;
    }

    trackir_session_join_worker_if_exited(session);

    otir_mutex_lock(&session->mutex);
    if (!session->worker_started) {
        trackir_session_reset_snapshot_locked(session);
    }
    otir_mutex_unlock(&session->mutex);
}

void otir_trackir_session_set_video_enabled(otir_trackir_session *session, bool enabled) {
    if (session == NULL) {
        return;
    }

    otir_mutex_lock(&session->mutex);
    session->video_enabled = enabled;
    if (!enabled) {
        trackir_session_clear_preview_locked(session);
    }
    otir_mutex_unlock(&session->mutex);
}

void otir_trackir_session_set_maximum_tracking_frames_per_second(
    otir_trackir_session *session,
    double maximum_frames_per_second
) {
    if (session == NULL) {
        return;
    }

    otir_mutex_lock(&session->mutex);
    session->maximum_tracking_frames_per_second =
        otir_tir5v3_normalize_maximum_frames_per_second(maximum_frames_per_second);
    otir_mutex_unlock(&session->mutex);
}

void otir_trackir_session_set_minimum_blob_area_points(
    otir_trackir_session *session,
    int minimum_blob_area_points
) {
    if (session == NULL) {
        return;
    }

    otir_mutex_lock(&session->mutex);
    session->minimum_blob_area_points =
        otir_tir5v3_normalize_minimum_blob_area_points(minimum_blob_area_points);
    otir_mutex_unlock(&session->mutex);
}

void otir_trackir_session_set_scaled_hull_enabled(
    otir_trackir_session *session,
    bool enabled
) {
    otir_trackir_session_set_centroid_mode(
        session,
        enabled
            ? OTIR_TIR5V3_CENTROID_MODE_FILLED_HULL
            : OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB
    );
}

void otir_trackir_session_set_centroid_mode(
    otir_trackir_session *session,
    otir_tir5v3_centroid_mode mode
) {
    if (session == NULL) {
        return;
    }

    otir_mutex_lock(&session->mutex);
    if (mode < OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB ||
        mode > OTIR_TIR5V3_CENTROID_MODE_REGULARIZED_BINARY) {
        mode = OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB;
    }
    session->centroid_mode = mode;
    otir_mutex_unlock(&session->mutex);
}

void otir_trackir_session_set_low_power_mode_enabled(
    otir_trackir_session *session,
    bool enabled
) {
    if (session == NULL) {
        return;
    }

    otir_mutex_lock(&session->mutex);
    session->low_power_mode_enabled = enabled;
    session->snapshot.is_low_power_mode = enabled;
    otir_mutex_unlock(&session->mutex);
}

otir_trackir_session_processing_mode otir_trackir_session_select_processing_mode(
    bool low_power_mode_enabled,
    bool should_process_tracking,
    bool should_publish_preview,
    bool tracking_is_rate_limited
) {
    if (low_power_mode_enabled) {
        return OTIR_TRACKIR_SESSION_PROCESSING_MODE_LOW_POWER;
    }
    if (!should_process_tracking && should_publish_preview) {
        return OTIR_TRACKIR_SESSION_PROCESSING_MODE_PREVIEW_ONLY;
    }
    if (tracking_is_rate_limited) {
        return OTIR_TRACKIR_SESSION_PROCESSING_MODE_REDUCED_TRACKING;
    }
    return OTIR_TRACKIR_SESSION_PROCESSING_MODE_FULL_TRACKING;
}

otir_trackir_session_source_rate_sample otir_trackir_session_next_source_rate_sample(
    bool low_power_mode_enabled,
    otir_trackir_session_source_rate_sample current_sample,
    double now
) {
    if (low_power_mode_enabled) {
        return (otir_trackir_session_source_rate_sample){
            .frame_rate = 0.0,
            .has_frame_rate = false,
            .sampled_frame_count = 0,
            .sample_start_time = now,
        };
    }

    current_sample.sampled_frame_count += 1;
    if (now - current_sample.sample_start_time >= 0.25) {
        current_sample.frame_rate =
            (double)current_sample.sampled_frame_count / (now - current_sample.sample_start_time);
        current_sample.has_frame_rate = true;
        current_sample.sampled_frame_count = 0;
        current_sample.sample_start_time = now;
    }

    return current_sample;
}

void otir_trackir_session_copy_snapshot(
    otir_trackir_session *session,
    otir_trackir_session_snapshot *out_snapshot
) {
    if (session == NULL || out_snapshot == NULL) {
        return;
    }

    trackir_session_join_worker_if_exited(session);

    otir_mutex_lock(&session->mutex);
    memcpy(out_snapshot, &session->snapshot, sizeof(*out_snapshot));
    otir_mutex_unlock(&session->mutex);
}

bool otir_trackir_session_copy_preview_frame(
    otir_trackir_session *session,
    uint8_t *out_frame,
    size_t capacity,
    uint64_t *out_generation
) {
    if (session == NULL || out_frame == NULL || capacity < OTIR_TRACKIR_SESSION_FRAME_BYTES) {
        return false;
    }

    otir_mutex_lock(&session->mutex);
    if (!session->snapshot.has_preview_frame) {
        otir_mutex_unlock(&session->mutex);
        return false;
    }

    memcpy(out_frame, session->preview_frame, OTIR_TRACKIR_SESSION_FRAME_BYTES);
    if (out_generation != NULL) {
        *out_generation = session->snapshot.preview_frame_generation;
    }
    otir_mutex_unlock(&session->mutex);
    return true;
}

static OTIR_THREAD_RESULT OTIR_THREAD_CALL trackir_session_worker_main(void *context) {
    otir_trackir_session *session = context;
    otir_tir5v3_device *device = NULL;
    void *blob_workspace = malloc(otir_tir5v3_blob_workspace_bytes());
    otir_tir5v3_packet *packet = malloc(sizeof(*packet));
    otir_status status;
    uint64_t frame_index = 0;
    otir_trackir_session_source_rate_sample source_rate_sample = {
        .frame_rate = 0.0,
        .has_frame_rate = false,
        .sampled_frame_count = 0,
        .sample_start_time = trackir_session_now_seconds(),
    };
    double last_tracking_process_time = 0.0;
    bool has_tracking_process_time = false;
    double last_preview_publish_time = 0.0;
    bool has_preview_publish_time = false;
    bool has_previous_selected_centroid = false;
    double previous_selected_centroid_x = 0.0;
    double previous_selected_centroid_y = 0.0;
    uint8_t *frame_buffer = malloc(OTIR_TRACKIR_SESSION_FRAME_BYTES);

    if (blob_workspace == NULL || packet == NULL || frame_buffer == NULL) {
        otir_mutex_lock(&session->mutex);
        trackir_session_set_failure_locked(session, OTIR_STATUS_IO, "Allocate buffers");
        session->worker_exited = true;
        otir_mutex_unlock(&session->mutex);
        free(blob_workspace);
        free(packet);
        free(frame_buffer);
        return OTIR_THREAD_RETURN;
    }

    status = otir_tir5v3_open(&device);
    if (status != OTIR_STATUS_OK || device == NULL) {
        otir_mutex_lock(&session->mutex);
        trackir_session_set_failure_locked(session, status, "Open");
        session->worker_exited = true;
        otir_mutex_unlock(&session->mutex);
        free(blob_workspace);
        free(packet);
        free(frame_buffer);
        return OTIR_THREAD_RETURN;
    }

    otir_mutex_lock(&session->mutex);
    session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STARTING;
    otir_mutex_unlock(&session->mutex);

    do {
        otir_tir5v3_status init_statuses[OTIR_TIR5V3_INIT_STATUS_COUNT];
        size_t init_status_count = 0;

        status = otir_tir5v3_initialize(
            device,
            init_statuses,
            OTIR_TIR5V3_INIT_STATUS_COUNT,
            &init_status_count
        );
        if (status != OTIR_STATUS_OK) {
            break;
        }

        status = otir_tir5v3_start_streaming(device);
        if (status != OTIR_STATUS_OK) {
            break;
        }

        otir_mutex_lock(&session->mutex);
        session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STREAMING;
        session->snapshot.status = OTIR_STATUS_OK;
        session->snapshot.has_error_message = false;
        otir_mutex_unlock(&session->mutex);

        while (!trackir_session_stop_requested(session)) {
            otir_tir5v3_blob_result blob_result;
            otir_tir5v3_blob_tracking_config blob_config = otir_tir5v3_default_blob_tracking_config();
            trackir_session_runtime_config runtime_config;
            otir_trackir_session_processing_mode processing_mode;
            double centroid_x = 0.0;
            double centroid_y = 0.0;
            double effective_tracking_frames_per_second = 0.0;
            bool has_centroid = false;
            bool has_previous_blob_centroid = false;
            bool should_process_tracking = false;
            bool should_publish_preview = false;
            bool tracking_is_rate_limited = false;
            double now;
            double elapsed_since_last_tracking_process = 0.0;
            double elapsed_since_last_publish = 0.0;

            status = otir_tir5v3_read_packet(device, 50, packet);

            if (status == OTIR_STATUS_TIMEOUT) {
                continue;
            }
            if (status != OTIR_STATUS_OK) {
                break;
            }
            if (packet->packet_type != 0x00 && packet->packet_type != 0x05) {
                continue;
            }

            now = trackir_session_now_seconds();
            runtime_config = trackir_session_runtime_config_snapshot(session);
            source_rate_sample = otir_trackir_session_next_source_rate_sample(
                runtime_config.low_power_mode_enabled,
                source_rate_sample,
                now
            );

            if (has_tracking_process_time) {
                elapsed_since_last_tracking_process = now - last_tracking_process_time;
            }
            effective_tracking_frames_per_second = runtime_config.low_power_mode_enabled
                ? OTIR_TRACKIR_SESSION_LOW_POWER_FRAMES_PER_SECOND
                : runtime_config.maximum_tracking_frames_per_second;
            should_process_tracking = !has_tracking_process_time ||
                effective_tracking_frames_per_second <= 0.0 ||
                otir_tir5v3_should_publish_frame(
                    elapsed_since_last_tracking_process,
                    effective_tracking_frames_per_second
                );
            if (runtime_config.video_enabled) {
                if (has_preview_publish_time) {
                    elapsed_since_last_publish = now - last_preview_publish_time;
                }
                should_publish_preview = !has_preview_publish_time ||
                    otir_tir5v3_should_publish_frame(
                        elapsed_since_last_publish,
                        OTIR_TRACKIR_SESSION_MAX_PREVIEW_FRAMES_PER_SECOND
                    );
            }
            if (!should_process_tracking && !should_publish_preview) {
                continue;
            }

            tracking_is_rate_limited = !runtime_config.low_power_mode_enabled &&
                effective_tracking_frames_per_second > 0.0 &&
                source_rate_sample.has_frame_rate &&
                effective_tracking_frames_per_second < source_rate_sample.frame_rate;
            processing_mode = otir_trackir_session_select_processing_mode(
                runtime_config.low_power_mode_enabled,
                should_process_tracking,
                should_publish_preview,
                tracking_is_rate_limited
            );

            if (processing_mode != OTIR_TRACKIR_SESSION_PROCESSING_MODE_PREVIEW_ONLY) {
                frame_index += 1;
                if (processing_mode != OTIR_TRACKIR_SESSION_PROCESSING_MODE_LOW_POWER) {
                    blob_config.minimum_area_points = runtime_config.minimum_blob_area_points;
                    blob_config.centroid_mode = runtime_config.centroid_mode;
                    has_previous_blob_centroid = has_previous_selected_centroid;
                    blob_result = (otir_tir5v3_blob_result){0};
                    if (otir_tir5v3_compute_blob_result_with_workspace(
                        packet->stripes,
                        packet->stripe_count,
                        blob_config,
                        has_previous_blob_centroid,
                        previous_selected_centroid_x,
                        previous_selected_centroid_y,
                        blob_workspace,
                        otir_tir5v3_blob_workspace_bytes(),
                        &blob_result
                    )) {
                        has_previous_selected_centroid = true;
                        previous_selected_centroid_x = blob_result.centroid_x;
                        previous_selected_centroid_y = blob_result.centroid_y;
                    } else {
                        has_previous_selected_centroid = false;
                    }
                    if (blob_result.has_centroid) {
                        centroid_x = blob_result.centroid_x;
                        centroid_y = blob_result.centroid_y;
                        has_centroid = true;
                    }
                }

                otir_mutex_lock(&session->mutex);
                session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STREAMING;
                session->snapshot.status = OTIR_STATUS_OK;
                session->snapshot.frame_index = frame_index;
                session->snapshot.has_frame_rate = source_rate_sample.has_frame_rate;
                session->snapshot.frame_rate = source_rate_sample.frame_rate;
                session->snapshot.has_centroid = has_centroid;
                session->snapshot.centroid_x = centroid_x;
                session->snapshot.centroid_y = centroid_y;
                session->snapshot.has_packet_type = true;
                session->snapshot.packet_type = packet->packet_type;
                session->snapshot.is_low_power_mode =
                    processing_mode == OTIR_TRACKIR_SESSION_PROCESSING_MODE_LOW_POWER;
                otir_mutex_unlock(&session->mutex);

                last_tracking_process_time = now;
                has_tracking_process_time = true;
            } else {
                otir_mutex_lock(&session->mutex);
                session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_STREAMING;
                session->snapshot.status = OTIR_STATUS_OK;
                session->snapshot.has_frame_rate = source_rate_sample.has_frame_rate;
                session->snapshot.frame_rate = source_rate_sample.frame_rate;
                session->snapshot.has_packet_type = true;
                session->snapshot.packet_type = packet->packet_type;
                session->snapshot.is_low_power_mode = false;
                otir_mutex_unlock(&session->mutex);
            }

            if (!should_publish_preview) {
                continue;
            }

            otir_tir5v3_build_frame(packet, frame_buffer, OTIR_TIR5V3_FRAME_WIDTH);

            otir_mutex_lock(&session->mutex);
            memcpy(session->preview_frame, frame_buffer, OTIR_TRACKIR_SESSION_FRAME_BYTES);
            session->snapshot.has_preview_frame = true;
            session->snapshot.preview_width = OTIR_TIR5V3_FRAME_WIDTH;
            session->snapshot.preview_height = OTIR_TIR5V3_FRAME_HEIGHT;
            session->snapshot.preview_frame_generation += 1;
            otir_mutex_unlock(&session->mutex);

            last_preview_publish_time = now;
            has_preview_publish_time = true;
        }
    } while (0);

    if (status != OTIR_STATUS_OK && !trackir_session_stop_requested(session)) {
        otir_mutex_lock(&session->mutex);
        trackir_session_set_failure_locked(session, status, "TrackIR");
        otir_mutex_unlock(&session->mutex);
    }

    (void)otir_tir5v3_stop_streaming(device);
    otir_tir5v3_close(device);
    free(blob_workspace);
    free(packet);
    free(frame_buffer);

    otir_mutex_lock(&session->mutex);
    if (session->stop_requested) {
        trackir_session_reset_snapshot_locked(session);
    }
    session->worker_exited = true;
    otir_mutex_unlock(&session->mutex);
    return OTIR_THREAD_RETURN;
}

static void trackir_session_reset_snapshot_locked(otir_trackir_session *session) {
    memset(&session->snapshot, 0, sizeof(session->snapshot));
    session->snapshot.phase = OTIR_TRACKIR_SESSION_PHASE_IDLE;
    session->snapshot.status = OTIR_STATUS_OK;
    session->snapshot.is_low_power_mode = false;
}

static void trackir_session_clear_preview_locked(otir_trackir_session *session) {
    session->snapshot.has_preview_frame = false;
    session->snapshot.preview_width = 0;
    session->snapshot.preview_height = 0;
}

static void trackir_session_set_error_locked(otir_trackir_session *session, const char *message) {
    if (message == NULL || message[0] == '\0') {
        session->snapshot.has_error_message = false;
        session->snapshot.error_message[0] = '\0';
        return;
    }

    session->snapshot.has_error_message = true;
    trackir_copy_string(
        session->snapshot.error_message,
        sizeof(session->snapshot.error_message),
        message
    );
}

static void trackir_session_set_failure_locked(
    otir_trackir_session *session,
    otir_status status,
    const char *operation
) {
    char message[OTIR_TRACKIR_SESSION_ERROR_MESSAGE_LENGTH];

    trackir_session_format_failure_message(status, operation, message, sizeof(message));
    trackir_session_reset_snapshot_locked(session);
    session->snapshot.status = status;
    session->snapshot.phase = status == OTIR_STATUS_NOT_FOUND
        ? OTIR_TRACKIR_SESSION_PHASE_UNAVAILABLE
        : OTIR_TRACKIR_SESSION_PHASE_FAILED;
    trackir_session_set_error_locked(session, message);
}

static void trackir_session_join_worker_if_exited(otir_trackir_session *session) {
    otir_thread worker = 0;
    bool should_join = false;

    otir_mutex_lock(&session->mutex);
    if (session->worker_started && session->worker_exited) {
        worker = session->worker;
        should_join = true;
    }
    otir_mutex_unlock(&session->mutex);

    if (!should_join) {
        return;
    }

    otir_thread_join(worker);

    otir_mutex_lock(&session->mutex);
    session->worker_started = false;
    session->worker_exited = false;
    session->stop_requested = false;
    otir_mutex_unlock(&session->mutex);
}

static bool trackir_session_stop_requested(otir_trackir_session *session) {
    bool stop_requested;

    otir_mutex_lock(&session->mutex);
    stop_requested = session->stop_requested;
    otir_mutex_unlock(&session->mutex);
    return stop_requested;
}

static trackir_session_runtime_config trackir_session_runtime_config_snapshot(
    otir_trackir_session *session
) {
    trackir_session_runtime_config runtime_config;

    otir_mutex_lock(&session->mutex);
    runtime_config.video_enabled = session->video_enabled;
    runtime_config.maximum_tracking_frames_per_second =
        session->maximum_tracking_frames_per_second;
    runtime_config.minimum_blob_area_points = session->minimum_blob_area_points;
    runtime_config.centroid_mode = session->centroid_mode;
    runtime_config.low_power_mode_enabled = session->low_power_mode_enabled;
    otir_mutex_unlock(&session->mutex);
    return runtime_config;
}

static void trackir_copy_string(char *destination, size_t capacity, const char *source) {
    if (destination == NULL || capacity == 0) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, capacity - 1);
    destination[capacity - 1] = '\0';
}

static void trackir_session_format_failure_message(
    otir_status status,
    const char *operation,
    char *buffer,
    size_t capacity
) {
    const char *status_description;

    if (buffer == NULL || capacity == 0) {
        return;
    }
    if (status == OTIR_STATUS_NOT_FOUND) {
        trackir_copy_string(
            buffer,
            capacity,
            "TrackIR not found. Connect the device and try again."
        );
        return;
    }
    if (status == OTIR_STATUS_PERMISSION_DENIED) {
        trackir_copy_string(
            buffer,
            capacity,
            "TrackIR access denied. Install the OpenTrackIR udev rule, reconnect the device, and try again."
        );
        return;
    }

    status_description = otir_status_string(status);
    if (operation == NULL || operation[0] == '\0') {
        snprintf(buffer, capacity, "%s.", status_description);
        return;
    }

    snprintf(buffer, capacity, "%s failed: %s.", operation, status_description);
}

static double trackir_session_now_seconds(void) {
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    if (!QueryPerformanceCounter(&counter) || frequency.QuadPart == 0) {
        return 0.0;
    }

    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec time_spec;

    if (clock_gettime(CLOCK_MONOTONIC, &time_spec) != 0) {
        return 0.0;
    }

    return (double)time_spec.tv_sec + ((double)time_spec.tv_nsec / 1000000000.0);
#endif
}
