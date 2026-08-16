#ifndef OPENTRACKIR_TIR5_H
#define OPENTRACKIR_TIR5_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTIR_TIR5V3_VENDOR_ID 0x131D
#define OTIR_TIR5V3_PRODUCT_ID 0x0159
#define OTIR_TIR5V3_FRAME_WIDTH 640
#define OTIR_TIR5V3_FRAME_HEIGHT 480
#define OTIR_TIR5V3_READ_SIZE 0x4000
#define OTIR_TIR5V3_DEFAULT_THRESHOLD 0x96
#define OTIR_TIR5V3_DEFAULT_BRIGHTNESS_RAW 0x780
#define OTIR_TIR5V3_MAX_PACKET_SIZE OTIR_TIR5V3_READ_SIZE
#define OTIR_TIR5V3_MAX_STRIPES (OTIR_TIR5V3_MAX_PACKET_SIZE / 4)
#define OTIR_TIR5V3_PENDING_CAPACITY (OTIR_TIR5V3_READ_SIZE * 2)
#define OTIR_TIR5V3_INIT_STATUS_COUNT 4

typedef enum otir_status {
    OTIR_STATUS_OK = 0,
    OTIR_STATUS_AGAIN = 1,
    OTIR_STATUS_INVALID_ARGUMENT = -1,
    OTIR_STATUS_BUFFER_TOO_SMALL = -2,
    OTIR_STATUS_OVERFLOW = -3,
    OTIR_STATUS_BAD_PACKET = -4,
    OTIR_STATUS_TIMEOUT = -5,
    OTIR_STATUS_IO = -6,
    OTIR_STATUS_NOT_OPEN = -7,
    OTIR_STATUS_NOT_FOUND = -8,
    OTIR_STATUS_DEPENDENCY_UNAVAILABLE = -9,
    OTIR_STATUS_UNSUPPORTED = -10,
    OTIR_STATUS_PERMISSION_DENIED = -11
} otir_status;

typedef struct otir_tir5v3_status {
    uint8_t raw[24];
    size_t raw_length;
    uint8_t length;
    uint8_t message_id;
    uint8_t stage;
    bool firmware_loaded;
    uint8_t status_flag;
    uint8_t checksum_hi;
    uint8_t checksum_lo;
} otir_tir5v3_status;

typedef struct otir_tir5v3_stripe {
    int hstart;
    int hstop;
    int vline;
    int points;
    int sum_x;
    int sum;
} otir_tir5v3_stripe;

typedef struct otir_tir5v3_packet {
    uint8_t raw[OTIR_TIR5V3_MAX_PACKET_SIZE];
    size_t raw_length;
    int packet_no;
    uint8_t packet_type;
    size_t payload_size;
    size_t stripe_count;
    otir_tir5v3_stripe stripes[OTIR_TIR5V3_MAX_STRIPES];
} otir_tir5v3_packet;

typedef struct otir_tir5v3_stream_parser {
    uint8_t pending[OTIR_TIR5V3_PENDING_CAPACITY];
    size_t pending_length;
} otir_tir5v3_stream_parser;

typedef struct otir_tir5v3_frame_stats {
    uint64_t frame_index;
    uint8_t packet_type;
    size_t stripe_count;
    size_t blob_count;
    bool has_centroid;
    double centroid_x;
    double centroid_y;
    int selected_blob_area_points;
    int selected_blob_brightness_sum;
    int centroid_mode;
    int packet_no;
} otir_tir5v3_frame_stats;

typedef enum otir_tir5v3_centroid_mode {
    OTIR_TIR5V3_CENTROID_MODE_NONE = 0,
    OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB = 1,
    OTIR_TIR5V3_CENTROID_MODE_FILLED_HULL = 2,
    OTIR_TIR5V3_CENTROID_MODE_BINARY_BLOB = 3,
    OTIR_TIR5V3_CENTROID_MODE_BLENDED_BINARY_WEIGHTED = 4,
    OTIR_TIR5V3_CENTROID_MODE_REGULARIZED_BINARY = 5,
    OTIR_TIR5V3_CENTROID_MODE_SCALED_HULL = OTIR_TIR5V3_CENTROID_MODE_FILLED_HULL
} otir_tir5v3_centroid_mode;

typedef struct otir_tir5v3_blob_tracking_config {
    int minimum_area_points;
    bool use_scaled_hull_centroid;
    otir_tir5v3_centroid_mode centroid_mode;
    int row_adjacency;
    double hull_scale;
} otir_tir5v3_blob_tracking_config;

typedef struct otir_tir5v3_blob_result {
    bool has_centroid;
    double centroid_x;
    double centroid_y;
    size_t blob_count;
    int selected_blob_area_points;
    int selected_blob_brightness_sum;
    otir_tir5v3_centroid_mode centroid_mode;
} otir_tir5v3_blob_result;

typedef enum otir_tir5v3_shutdown_action {
    OTIR_TIR5V3_SHUTDOWN_INTENT_1 = 1,
    OTIR_TIR5V3_SHUTDOWN_INTENT_2 = 2,
    OTIR_TIR5V3_SHUTDOWN_INTENT_4 = 4
} otir_tir5v3_shutdown_action;

typedef struct otir_tir5v3_shutdown_step {
    otir_tir5v3_shutdown_action action;
    uint8_t values[4];
    size_t value_count;
} otir_tir5v3_shutdown_step;

typedef struct otir_tir5v3_device_summary {
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uint8_t interface_number;
    uint16_t max_packet_size;
} otir_tir5v3_device_summary;

typedef struct otir_tir5v3_device otir_tir5v3_device;

const char *otir_status_string(otir_status status);

otir_status otir_tir5v3_apply_transport(
    const uint8_t packet[24],
    uint8_t r1,
    uint8_t encoded[24]
);

bool otir_tir5v3_is_stream_header(const uint8_t *data, size_t length);
bool otir_tir5v3_is_type3_prefix(const uint8_t *data, size_t length);

void otir_tir5v3_stream_parser_init(otir_tir5v3_stream_parser *parser);
otir_status otir_tir5v3_stream_parser_push(
    otir_tir5v3_stream_parser *parser,
    const uint8_t *data,
    size_t length
);
otir_status otir_tir5v3_stream_parser_push_resync(
    otir_tir5v3_stream_parser *parser,
    const uint8_t *data,
    size_t length,
    bool *did_resync
);
otir_status otir_tir5v3_stream_parser_next_packet(
    otir_tir5v3_stream_parser *parser,
    uint8_t *packet_buffer,
    size_t packet_capacity,
    size_t *packet_length
);

otir_status otir_tir5v3_parse_status(
    const uint8_t *packet,
    size_t length,
    otir_tir5v3_status *out_status
);
otir_status otir_tir5v3_parse_packet(
    const uint8_t *packet,
    size_t length,
    otir_tir5v3_packet *out_packet
);

bool otir_tir5v3_compute_weighted_centroid(
    const otir_tir5v3_stripe *stripes,
    size_t stripe_count,
    double *out_x,
    double *out_y
);
int otir_tir5v3_normalize_minimum_blob_area_points(int minimum_area_points);
otir_tir5v3_blob_tracking_config otir_tir5v3_default_blob_tracking_config(void);
size_t otir_tir5v3_blob_workspace_bytes(void);
bool otir_tir5v3_compute_blob_result_with_workspace(
    const otir_tir5v3_stripe *stripes,
    size_t stripe_count,
    otir_tir5v3_blob_tracking_config config,
    bool has_previous_centroid,
    double previous_centroid_x,
    double previous_centroid_y,
    void *workspace,
    size_t workspace_size,
    otir_tir5v3_blob_result *out_result
);
bool otir_tir5v3_compute_blob_result(
    const otir_tir5v3_stripe *stripes,
    size_t stripe_count,
    otir_tir5v3_blob_tracking_config config,
    bool has_previous_centroid,
    double previous_centroid_x,
    double previous_centroid_y,
    otir_tir5v3_blob_result *out_result
);

size_t otir_tir5v3_shutdown_steps(
    bool is_streaming,
    bool ir_led_enabled,
    otir_tir5v3_shutdown_step *steps,
    size_t capacity
);

void otir_tir5v3_build_frame(
    const otir_tir5v3_packet *packet,
    uint8_t *frame,
    size_t stride
);
double otir_tir5v3_normalize_maximum_frames_per_second(double maximum_frames_per_second);
bool otir_tir5v3_should_process_frame(
    double current_time_seconds,
    double last_processed_time_seconds,
    bool has_last_processed_time,
    double maximum_frames_per_second
);
bool otir_tir5v3_should_publish_frame(
    double elapsed_since_last_frame,
    double maximum_frames_per_second
);
void otir_tir5v3_packet_stats(
    const otir_tir5v3_packet *packet,
    uint64_t frame_index,
    otir_tir5v3_frame_stats *out_stats
);

otir_status otir_tir5v3_open(otir_tir5v3_device **out_device);
void otir_tir5v3_close(otir_tir5v3_device *device);
otir_status otir_tir5v3_get_device_summary(
    const otir_tir5v3_device *device,
    otir_tir5v3_device_summary *out_summary
);
otir_status otir_tir5v3_initialize(
    otir_tir5v3_device *device,
    otir_tir5v3_status *statuses,
    size_t status_capacity,
    size_t *status_count
);
otir_status otir_tir5v3_start_streaming(otir_tir5v3_device *device);
otir_status otir_tir5v3_stop_streaming(otir_tir5v3_device *device);
otir_status otir_tir5v3_set_threshold(otir_tir5v3_device *device, uint16_t threshold);
otir_status otir_tir5v3_set_ir_led(otir_tir5v3_device *device, bool enabled);
otir_status otir_tir5v3_set_ir_brightness_raw(otir_tir5v3_device *device, uint16_t brightness);
otir_status otir_tir5v3_read_chunk(
    otir_tir5v3_device *device,
    int timeout_ms,
    uint8_t *buffer,
    size_t buffer_capacity,
    size_t *bytes_read
);
otir_status otir_tir5v3_read_packet(
    otir_tir5v3_device *device,
    int timeout_ms,
    otir_tir5v3_packet *out_packet
);

#ifdef __cplusplus
}
#endif

#endif
