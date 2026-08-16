#include "opentrackir/tir5.h"
#include "opentrackir/tir5_mouse.h"
#include "opentrackir/tir5_session.h"
#include "opentrackir/tir5_tooling.h"
#include "tir5_libusb_status.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void make_type5_packet(uint8_t packet_no, const uint8_t *payload, size_t payload_size, uint8_t *out_packet, size_t *out_length);
static void encode_tir5v3_stripe(
    int hstart,
    int vline,
    int points,
    int sum_x,
    int stripe_sum,
    uint8_t encoded[8]
);

static void test_apply_transport_obfuscates_header_and_nonce(void);
static void test_libusb_errors_map_to_actionable_statuses(void);
static void test_parse_status_reads_stage_fields(void);
static void test_stream_parser_recovers_split_packets(void);
static void test_stream_parser_resyncs_after_bad_leading_header(void);
static void test_stream_parser_push_resync_recovers_after_overflow(void);
static void test_parse_packet_handles_empty_type5_packet(void);
static void test_parse_packet_decodes_type5_stripe(void);
static void test_compute_weighted_centroid_matches_linuxtrack_formula(void);
static void test_normalize_minimum_blob_area_points_clamps_to_positive_values(void);
static void test_compute_blob_result_selects_largest_blob(void);
static void test_compute_blob_result_uses_previous_centroid_to_break_ties(void);
static void test_compute_blob_result_can_use_scaled_hull_centroid(void);
static void test_compute_blob_result_can_use_binary_centroid(void);
static void test_compute_blob_result_can_use_blended_centroid(void);
static void test_compute_blob_result_can_use_regularized_binary_centroid(void);
static void test_compute_blob_result_row_bucket_grouping_respects_row_adjacency(void);
static void test_shutdown_steps_turns_led_off_even_without_streaming(void);
static void test_build_frame_marks_stripes_and_stats(void);
static void test_normalize_maximum_frames_per_second_rejects_invalid_values(void);
static void test_should_process_frame_respects_processing_cap(void);
static void test_should_publish_frame_respects_maximum_rate(void);
static void test_session_processing_mode_prefers_low_power_and_preview_only(void);
static void test_session_source_rate_sample_disables_measurement_in_low_power(void);
static void test_mouse_transform_delta_applies_flip_and_rotation(void);
static void test_mouse_vertical_gain_scales_y_axis(void);
static void test_mouse_smoothing_mode_matches_python_thresholds(void);
static void test_mouse_jump_filter_uses_plain_pixel_thresholds(void);
static void test_mouse_deadzone_uses_short_average_magnitude(void);
static void test_mouse_tracker_applies_adaptive_smoothing(void);
static void test_mouse_step_suppresses_zero_delta(void);
static void test_cli_read_maximum_frames_per_second_accepts_optional_argument(void);

int main(void) {
    test_apply_transport_obfuscates_header_and_nonce();
    test_libusb_errors_map_to_actionable_statuses();
    test_parse_status_reads_stage_fields();
    test_stream_parser_recovers_split_packets();
    test_stream_parser_resyncs_after_bad_leading_header();
    test_stream_parser_push_resync_recovers_after_overflow();
    test_parse_packet_handles_empty_type5_packet();
    test_parse_packet_decodes_type5_stripe();
    test_compute_weighted_centroid_matches_linuxtrack_formula();
    test_normalize_minimum_blob_area_points_clamps_to_positive_values();
    test_compute_blob_result_selects_largest_blob();
    test_compute_blob_result_uses_previous_centroid_to_break_ties();
    test_compute_blob_result_can_use_scaled_hull_centroid();
    test_compute_blob_result_can_use_binary_centroid();
    test_compute_blob_result_can_use_blended_centroid();
    test_compute_blob_result_can_use_regularized_binary_centroid();
    test_compute_blob_result_row_bucket_grouping_respects_row_adjacency();
    test_shutdown_steps_turns_led_off_even_without_streaming();
    test_build_frame_marks_stripes_and_stats();
    test_normalize_maximum_frames_per_second_rejects_invalid_values();
    test_should_process_frame_respects_processing_cap();
    test_should_publish_frame_respects_maximum_rate();
    test_session_processing_mode_prefers_low_power_and_preview_only();
    test_session_source_rate_sample_disables_measurement_in_low_power();
    test_mouse_transform_delta_applies_flip_and_rotation();
    test_mouse_vertical_gain_scales_y_axis();
    test_mouse_smoothing_mode_matches_python_thresholds();
    test_mouse_jump_filter_uses_plain_pixel_thresholds();
    test_mouse_deadzone_uses_short_average_magnitude();
    test_mouse_tracker_applies_adaptive_smoothing();
    test_mouse_step_suppresses_zero_delta();
    test_cli_read_maximum_frames_per_second_accepts_optional_argument();
    puts("c/tests/test_tir5: all tests passed");
    return 0;
}

static void test_libusb_errors_map_to_actionable_statuses(void) {
    assert(otir_tir5v3_map_libusb_error(LIBUSB_ERROR_TIMEOUT) == OTIR_STATUS_TIMEOUT);
    assert(otir_tir5v3_map_libusb_error(LIBUSB_ERROR_ACCESS) == OTIR_STATUS_PERMISSION_DENIED);
    assert(otir_tir5v3_map_libusb_error(LIBUSB_ERROR_NOT_FOUND) == OTIR_STATUS_NOT_FOUND);
    assert(otir_tir5v3_map_libusb_error(LIBUSB_ERROR_NO_DEVICE) == OTIR_STATUS_NOT_FOUND);
    assert(otir_tir5v3_map_libusb_error(LIBUSB_ERROR_IO) == OTIR_STATUS_IO);
    assert(strcmp(otir_status_string(OTIR_STATUS_PERMISSION_DENIED), "permission_denied") == 0);
}

static void make_type5_packet(uint8_t packet_no, const uint8_t *payload, size_t payload_size, uint8_t *out_packet, size_t *out_length) {
    out_packet[0] = packet_no;
    out_packet[1] = 0x10;
    out_packet[2] = 0x05;
    out_packet[3] = (uint8_t)(packet_no ^ 0x10U ^ 0x05U ^ 0xAAU);
    if (payload_size > 0) {
        memcpy(out_packet + 4, payload, payload_size);
    }
    out_packet[4 + payload_size] = (uint8_t)((payload_size >> 24) & 0xFFU);
    out_packet[5 + payload_size] = (uint8_t)((payload_size >> 16) & 0xFFU);
    out_packet[6 + payload_size] = (uint8_t)((payload_size >> 8) & 0xFFU);
    out_packet[7 + payload_size] = (uint8_t)(payload_size & 0xFFU);
    *out_length = payload_size + 8;
}

static void encode_tir5v3_stripe(
    int hstart,
    int vline,
    int points,
    int sum_x,
    int stripe_sum,
    uint8_t encoded[8]
) {
    encoded[0] = (uint8_t)((hstart >> 2) & 0xFF);
    encoded[1] = (uint8_t)(((hstart & 0x03) << 6) | ((vline >> 3) & 0x3F));
    encoded[2] = (uint8_t)(((vline & 0x07) << 5) | ((points >> 5) & 0x1F));
    encoded[3] = (uint8_t)(((points & 0x1F) << 3) | ((sum_x >> 17) & 0x07));
    encoded[4] = (uint8_t)((sum_x >> 9) & 0xFF);
    encoded[5] = (uint8_t)((sum_x >> 1) & 0xFF);
    encoded[6] = (uint8_t)(((sum_x & 0x01) << 7) | ((stripe_sum >> 8) & 0x7F));
    encoded[7] = (uint8_t)(stripe_sum & 0xFF);
}

static void test_apply_transport_obfuscates_header_and_nonce(void) {
    uint8_t packet[24];
    uint8_t encoded[24];
    size_t index;

    for (index = 0; index < sizeof(packet); ++index) {
        packet[index] = (uint8_t)index;
    }

    assert(otir_tir5v3_apply_transport(packet, 0xAB, encoded) == OTIR_STATUS_OK);
    assert(encoded[0] == (uint8_t)(packet[0] ^ 0x69U ^ packet[0x0A] ^ packet[0x0B]));
    assert(encoded[17] == 0xAB);
    assert(memcmp(encoded + 1, packet + 1, 16) == 0);
    assert(memcmp(encoded + 18, packet + 18, 6) == 0);
}

static void test_parse_status_reads_stage_fields(void) {
    uint8_t packet[] = {0x07, 0x20, 0x02, 0x01, 0x7F, 0xAA, 0x55};
    otir_tir5v3_status status;

    assert(otir_tir5v3_parse_status(packet, sizeof(packet), &status) == OTIR_STATUS_OK);
    assert(status.length == 0x07);
    assert(status.message_id == 0x20);
    assert(status.stage == 0x02);
    assert(status.firmware_loaded);
    assert(status.status_flag == 0x7F);
    assert(status.checksum_hi == 0xAA);
    assert(status.checksum_lo == 0x55);
}

static void test_stream_parser_recovers_split_packets(void) {
    uint8_t packet_one[8];
    uint8_t packet_two[8];
    uint8_t raw_packet[16];
    size_t packet_one_length = 0;
    size_t packet_two_length = 0;
    size_t raw_length = 0;
    size_t packet_length = 0;
    otir_tir5v3_stream_parser parser;

    make_type5_packet(0x11, NULL, 0, packet_one, &packet_one_length);
    make_type5_packet(0x12, NULL, 0, packet_two, &packet_two_length);

    otir_tir5v3_stream_parser_init(&parser);
    raw_packet[0] = 0xFF;
    raw_packet[1] = 0x00;
    memcpy(raw_packet + 2, packet_one, packet_one_length);
    memcpy(raw_packet + 2 + packet_one_length, packet_two, 3);
    raw_length = 2 + packet_one_length + 3;

    assert(otir_tir5v3_stream_parser_push(&parser, raw_packet, raw_length) == OTIR_STATUS_OK);
    assert(otir_tir5v3_stream_parser_next_packet(&parser, raw_packet, sizeof(raw_packet), &packet_length) == OTIR_STATUS_OK);
    assert(packet_length == packet_one_length);
    assert(memcmp(raw_packet, packet_one, packet_one_length) == 0);

    assert(otir_tir5v3_stream_parser_push(&parser, packet_two + 3, packet_two_length - 3) == OTIR_STATUS_OK);
    assert(otir_tir5v3_stream_parser_next_packet(&parser, raw_packet, sizeof(raw_packet), &packet_length) == OTIR_STATUS_OK);
    assert(packet_length == packet_two_length);
    assert(memcmp(raw_packet, packet_two, packet_two_length) == 0);
}

static void test_stream_parser_resyncs_after_bad_leading_header(void) {
    const uint8_t corrupted[] = {0x11, 0x10, 0x05, 0xAE, 0x40, 0x4D, 0xE0, 0x58};
    uint8_t packet[8];
    uint8_t raw_stream[16];
    size_t packet_length = 0;
    size_t raw_length = 0;
    otir_tir5v3_stream_parser parser;

    make_type5_packet(0x12, NULL, 0, packet, &packet_length);
    memcpy(raw_stream, corrupted, sizeof(corrupted));
    memcpy(raw_stream + sizeof(corrupted), packet, packet_length);
    raw_length = sizeof(corrupted) + packet_length;

    otir_tir5v3_stream_parser_init(&parser);
    assert(otir_tir5v3_stream_parser_push(&parser, raw_stream, raw_length) == OTIR_STATUS_OK);
    assert(otir_tir5v3_stream_parser_next_packet(&parser, raw_stream, sizeof(raw_stream), &raw_length) == OTIR_STATUS_OK);
    assert(raw_length == packet_length);
    assert(memcmp(raw_stream, packet, packet_length) == 0);
}

static void test_stream_parser_push_resync_recovers_after_overflow(void) {
    uint8_t nearly_full[OTIR_TIR5V3_PENDING_CAPACITY - 8];
    uint8_t next_chunk[16];
    otir_tir5v3_stream_parser parser;
    bool did_resync = false;
    size_t index;

    memset(nearly_full, 0xFF, sizeof(nearly_full));
    for (index = 0; index < sizeof(next_chunk); ++index) {
        next_chunk[index] = (uint8_t)index;
    }

    otir_tir5v3_stream_parser_init(&parser);
    assert(otir_tir5v3_stream_parser_push(&parser, nearly_full, sizeof(nearly_full)) == OTIR_STATUS_OK);
    assert(parser.pending_length == sizeof(nearly_full));

    assert(otir_tir5v3_stream_parser_push_resync(&parser, next_chunk, sizeof(next_chunk), &did_resync) == OTIR_STATUS_OK);
    assert(did_resync);
    assert(parser.pending_length == sizeof(next_chunk));
    assert(memcmp(parser.pending, next_chunk, sizeof(next_chunk)) == 0);
}

static void test_parse_packet_handles_empty_type5_packet(void) {
    uint8_t packet[8];
    size_t packet_length = 0;
    otir_tir5v3_packet parsed;

    make_type5_packet(0x24, NULL, 0, packet, &packet_length);
    assert(otir_tir5v3_parse_packet(packet, packet_length, &parsed) == OTIR_STATUS_OK);
    assert(parsed.packet_no == 0x24);
    assert(parsed.packet_type == 0x05);
    assert(parsed.payload_size == 0);
    assert(parsed.stripe_count == 0);
}

static void test_parse_packet_decodes_type5_stripe(void) {
    uint8_t payload[8];
    uint8_t packet[16];
    size_t packet_length = 0;
    otir_tir5v3_packet parsed;

    encode_tir5v3_stripe(321, 245, 6, 15, 700, payload);
    make_type5_packet(0x33, payload, sizeof(payload), packet, &packet_length);

    assert(otir_tir5v3_parse_packet(packet, packet_length, &parsed) == OTIR_STATUS_OK);
    assert(parsed.stripe_count == 1);
    assert(parsed.stripes[0].hstart == 321);
    assert(parsed.stripes[0].hstop == 326);
    assert(parsed.stripes[0].vline == 245);
    assert(parsed.stripes[0].points == 6);
    assert(parsed.stripes[0].sum_x == 15);
    assert(parsed.stripes[0].sum == 700);
}

static void test_compute_weighted_centroid_matches_linuxtrack_formula(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 10, .hstop = 12, .vline = 4, .points = 3, .sum_x = 3, .sum = 3},
        {.hstart = 20, .hstop = 21, .vline = 6, .points = 2, .sum_x = 1, .sum = 2},
    };
    double centroid_x = 0.0;
    double centroid_y = 0.0;

    assert(otir_tir5v3_compute_weighted_centroid(stripes, 2, &centroid_x, &centroid_y));
    assert(fabs(centroid_x - 14.8) < 0.0001);
    assert(fabs(centroid_y - 4.8) < 0.0001);
}

static void test_normalize_minimum_blob_area_points_clamps_to_positive_values(void) {
    assert(otir_tir5v3_normalize_minimum_blob_area_points(4) == 4);
    assert(otir_tir5v3_normalize_minimum_blob_area_points(0) == 1);
    assert(otir_tir5v3_normalize_minimum_blob_area_points(-5) == 1);
}

static void test_compute_blob_result_selects_largest_blob(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 10, .hstop = 11, .vline = 10, .points = 2, .sum_x = 1, .sum = 2},
        {.hstart = 10, .hstop = 11, .vline = 11, .points = 2, .sum_x = 1, .sum = 2},
        {.hstart = 100, .hstop = 102, .vline = 20, .points = 3, .sum_x = 3, .sum = 3},
        {.hstart = 100, .hstop = 102, .vline = 21, .points = 3, .sum_x = 3, .sum = 3},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.use_scaled_hull_centroid = false;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        4,
        config,
        false,
        0.0,
        0.0,
        &result
    ));
    assert(result.blob_count == 2);
    assert(result.selected_blob_area_points == 6);
    assert(result.selected_blob_brightness_sum == 6);
    assert(result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB);
    assert(fabs(result.centroid_x - 101.0) < 0.0001);
    assert(fabs(result.centroid_y - 20.5) < 0.0001);
}

static void test_compute_blob_result_uses_previous_centroid_to_break_ties(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 10, .hstop = 11, .vline = 10, .points = 2, .sum_x = 1, .sum = 2},
        {.hstart = 10, .hstop = 11, .vline = 11, .points = 2, .sum_x = 1, .sum = 2},
        {.hstart = 40, .hstop = 41, .vline = 10, .points = 2, .sum_x = 1, .sum = 2},
        {.hstart = 40, .hstop = 41, .vline = 11, .points = 2, .sum_x = 1, .sum = 2},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.use_scaled_hull_centroid = false;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        4,
        config,
        true,
        40.5,
        10.5,
        &result
    ));
    assert(result.blob_count == 2);
    assert(fabs(result.centroid_x - 40.5) < 0.0001);
    assert(fabs(result.centroid_y - 10.5) < 0.0001);
}

static void test_compute_blob_result_can_use_scaled_hull_centroid(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 0, .hstop = 2, .vline = 0, .points = 3, .sum_x = 3, .sum = 3},
        {.hstart = 0, .hstop = 0, .vline = 1, .points = 1, .sum_x = 0, .sum = 1},
        {.hstart = 0, .hstop = 0, .vline = 2, .points = 1, .sum_x = 0, .sum = 1},
    };
    otir_tir5v3_blob_tracking_config raw_config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_tracking_config hull_config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result raw_result = {0};
    otir_tir5v3_blob_result hull_result = {0};

    raw_config.minimum_area_points = 1;
    raw_config.use_scaled_hull_centroid = false;
    raw_config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB;
    hull_config.minimum_area_points = 1;
    hull_config.use_scaled_hull_centroid = true;
    hull_config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_FILLED_HULL;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        3,
        raw_config,
        false,
        0.0,
        0.0,
        &raw_result
    ));
    assert(otir_tir5v3_compute_blob_result(
        stripes,
        3,
        hull_config,
        false,
        0.0,
        0.0,
        &hull_result
    ));
    assert(raw_result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB);
    assert(hull_result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_SCALED_HULL);
    assert(hull_result.centroid_x > raw_result.centroid_x);
    assert(hull_result.centroid_y > raw_result.centroid_y);
}

static void test_compute_blob_result_can_use_binary_centroid(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 0, .hstop = 1, .vline = 10, .points = 2, .sum_x = 5, .sum = 6},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_BINARY_BLOB;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        1,
        config,
        false,
        0.0,
        0.0,
        &result
    ));
    assert(result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_BINARY_BLOB);
    assert(fabs(result.centroid_x - 0.5) < 0.0001);
    assert(fabs(result.centroid_y - 10.0) < 0.0001);
}

static void test_compute_blob_result_can_use_blended_centroid(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 0, .hstop = 1, .vline = 10, .points = 2, .sum_x = 5, .sum = 6},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_BLENDED_BINARY_WEIGHTED;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        1,
        config,
        false,
        0.0,
        0.0,
        &result
    ));
    assert(result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_BLENDED_BINARY_WEIGHTED);
    assert(fabs(result.centroid_x - 0.5666666667) < 0.0001);
    assert(fabs(result.centroid_y - 10.0) < 0.0001);
}

static void test_compute_blob_result_can_use_regularized_binary_centroid(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 0, .hstop = 1, .vline = 10, .points = 2, .sum_x = 5, .sum = 6},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_REGULARIZED_BINARY;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        1,
        config,
        true,
        2.5,
        10.0,
        &result
    ));
    assert(result.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_REGULARIZED_BINARY);
    assert(fabs(result.centroid_x - 1.7) < 0.0001);
    assert(fabs(result.centroid_y - 10.0) < 0.0001);
}

static void test_compute_blob_result_row_bucket_grouping_respects_row_adjacency(void) {
    otir_tir5v3_stripe stripes[] = {
        {.hstart = 10, .hstop = 12, .vline = 10, .points = 3, .sum_x = 3, .sum = 3},
        {.hstart = 11, .hstop = 13, .vline = 11, .points = 3, .sum_x = 3, .sum = 3},
        {.hstart = 10, .hstop = 12, .vline = 14, .points = 3, .sum_x = 3, .sum = 3},
    };
    otir_tir5v3_blob_tracking_config config = otir_tir5v3_default_blob_tracking_config();
    otir_tir5v3_blob_result result = {0};

    config.minimum_area_points = 1;
    config.use_scaled_hull_centroid = false;
    config.centroid_mode = OTIR_TIR5V3_CENTROID_MODE_RAW_BLOB;
    config.row_adjacency = 1;

    assert(otir_tir5v3_compute_blob_result(
        stripes,
        3,
        config,
        false,
        0.0,
        0.0,
        &result
    ));
    assert(result.blob_count == 2);
    assert(result.selected_blob_area_points == 6);
    assert(fabs(result.centroid_y - 10.5) < 0.0001);
}

static void test_shutdown_steps_turns_led_off_even_without_streaming(void) {
    otir_tir5v3_shutdown_step steps[4];
    size_t count;

    count = otir_tir5v3_shutdown_steps(false, true, steps, 4);
    assert(count == 1);
    assert(steps[0].action == OTIR_TIR5V3_SHUTDOWN_INTENT_4);
    assert(steps[0].value_count == 4);
    assert(steps[0].values[0] == 0x19);
    assert(steps[0].values[1] == 0x09);
    assert(steps[0].values[2] == 0x00);
    assert(steps[0].values[3] == 0x00);
}

static void test_build_frame_marks_stripes_and_stats(void) {
    otir_tir5v3_packet packet;
    otir_tir5v3_frame_stats stats;
    uint8_t *frame = calloc(OTIR_TIR5V3_FRAME_HEIGHT * OTIR_TIR5V3_FRAME_WIDTH, sizeof(*frame));

    assert(frame != NULL);

    memset(&packet, 0, sizeof(packet));
    packet.packet_type = 0x05;
    packet.packet_no = 0x44;
    packet.stripe_count = 2;
    packet.stripes[0] = (otir_tir5v3_stripe){.hstart = 2, .hstop = 4, .vline = 1, .points = 3, .sum_x = 3, .sum = 300};
    packet.stripes[1] = (otir_tir5v3_stripe){.hstart = 10, .hstop = 11, .vline = 3, .points = 2, .sum_x = 1, .sum = 200};

    otir_tir5v3_build_frame(&packet, frame, OTIR_TIR5V3_FRAME_WIDTH);
    assert(frame[(1 * OTIR_TIR5V3_FRAME_WIDTH) + 2] == 100);
    assert(frame[(1 * OTIR_TIR5V3_FRAME_WIDTH) + 3] == 100);
    assert(frame[(1 * OTIR_TIR5V3_FRAME_WIDTH) + 4] == 100);
    assert(frame[0] == 0);

    otir_tir5v3_packet_stats(&packet, 7, &stats);
    assert(stats.frame_index == 7);
    assert(stats.packet_type == 0x05);
    assert(stats.stripe_count == 2);
    assert(stats.blob_count == 2);
    assert(stats.packet_no == 0x44);
    assert(!stats.has_centroid);
    assert(stats.selected_blob_area_points == 0);
    assert(stats.selected_blob_brightness_sum == 0);
    assert(stats.centroid_mode == OTIR_TIR5V3_CENTROID_MODE_NONE);

    free(frame);
}

static void test_should_publish_frame_respects_maximum_rate(void) {
    assert(!otir_tir5v3_should_publish_frame(0.0, 60.0));
    assert(!otir_tir5v3_should_publish_frame(0.01, 60.0));
    assert(otir_tir5v3_should_publish_frame(1.0 / 60.0, 60.0));
    assert(otir_tir5v3_should_publish_frame(0.05, 60.0));
    assert(!otir_tir5v3_should_publish_frame(0.05, 0.0));
}

static void test_session_processing_mode_prefers_low_power_and_preview_only(void) {
    assert(
        otir_trackir_session_select_processing_mode(true, false, true, false) ==
        OTIR_TRACKIR_SESSION_PROCESSING_MODE_LOW_POWER
    );
    assert(
        otir_trackir_session_select_processing_mode(false, false, true, false) ==
        OTIR_TRACKIR_SESSION_PROCESSING_MODE_PREVIEW_ONLY
    );
    assert(
        otir_trackir_session_select_processing_mode(false, true, false, true) ==
        OTIR_TRACKIR_SESSION_PROCESSING_MODE_REDUCED_TRACKING
    );
    assert(
        otir_trackir_session_select_processing_mode(false, true, false, false) ==
        OTIR_TRACKIR_SESSION_PROCESSING_MODE_FULL_TRACKING
    );
}

static void test_session_source_rate_sample_disables_measurement_in_low_power(void) {
    otir_trackir_session_source_rate_sample sample = {
        .frame_rate = 120.0,
        .has_frame_rate = true,
        .sampled_frame_count = 3,
        .sample_start_time = 1.0,
    };

    sample = otir_trackir_session_next_source_rate_sample(false, sample, 1.30);
    assert(sample.has_frame_rate);
    assert(fabs(sample.frame_rate - 13.3333333333) < 0.0001);
    assert(sample.sampled_frame_count == 0);
    assert(fabs(sample.sample_start_time - 1.30) < 0.0001);

    sample = otir_trackir_session_next_source_rate_sample(true, sample, 2.0);
    assert(!sample.has_frame_rate);
    assert(sample.frame_rate == 0.0);
    assert(sample.sampled_frame_count == 0);
    assert(fabs(sample.sample_start_time - 2.0) < 0.0001);
}

static void test_mouse_transform_delta_applies_flip_and_rotation(void) {
    otir_trackir_mouse_point flipped = otir_trackir_mouse_transform_delta(
        (otir_trackir_mouse_point){.x = 3.0, .y = -2.0},
        2.0,
        (otir_trackir_mouse_transform){
            .scale_x = -1.0,
            .scale_y = 1.0,
            .rotation_degrees = 0.0,
        }
    );
    otir_trackir_mouse_point rotated = otir_trackir_mouse_transform_delta(
        (otir_trackir_mouse_point){.x = 4.0, .y = 0.0},
        1.5,
        (otir_trackir_mouse_transform){
            .scale_x = 1.0,
            .scale_y = 1.0,
            .rotation_degrees = 90.0,
        }
    );

    assert(fabs(flipped.x + 6.0) < 0.0001);
    assert(fabs(flipped.y + 4.0) < 0.0001);
    assert(fabs(rotated.x) < 0.0001);
    assert(fabs(rotated.y - 6.0) < 0.0001);
}

static void test_mouse_vertical_gain_scales_y_axis(void) {
    otir_trackir_mouse_point delta = otir_trackir_mouse_apply_vertical_gain(
        (otir_trackir_mouse_point){.x = 8.0, .y = -4.0}
    );

    assert(fabs(delta.x - 8.0) < 0.0001);
    assert(fabs(delta.y + 5.0) < 0.0001);
}

static void test_mouse_smoothing_mode_matches_python_thresholds(void) {
    assert(
        otir_trackir_mouse_smoothing_mode_for_delta(
            (otir_trackir_mouse_point){.x = 0.1, .y = 0.1}
        ) == OTIR_TRACKIR_MOUSE_SMOOTHING_MODE_LONG
    );
    assert(
        otir_trackir_mouse_smoothing_mode_for_delta(
            (otir_trackir_mouse_point){.x = 0.5, .y = 0.0}
        ) == OTIR_TRACKIR_MOUSE_SMOOTHING_MODE_SHORT
    );
    assert(
        otir_trackir_mouse_smoothing_mode_for_delta(
            (otir_trackir_mouse_point){.x = 1.0, .y = 0.0}
        ) == OTIR_TRACKIR_MOUSE_SMOOTHING_MODE_RAW
    );
}

static void test_mouse_jump_filter_uses_plain_pixel_thresholds(void) {
    assert(
        otir_trackir_mouse_should_skip_jump(
            (otir_trackir_mouse_point){.x = 51.0, .y = 0.0},
            true,
            50.0
        )
    );
    assert(
        otir_trackir_mouse_should_skip_jump(
            (otir_trackir_mouse_point){.x = 0.0, .y = -51.0},
            true,
            50.0
        )
    );
    assert(
        !otir_trackir_mouse_should_skip_jump(
            (otir_trackir_mouse_point){.x = 7.0, .y = 7.0},
            true,
            50.0
        )
    );
    assert(
        !otir_trackir_mouse_should_skip_jump(
            (otir_trackir_mouse_point){.x = 80.0, .y = 0.0},
            false,
            50.0
        )
    );
}

static void test_mouse_deadzone_uses_short_average_magnitude(void) {
    assert(
        otir_trackir_mouse_is_inside_deadzone(
            (otir_trackir_mouse_point){.x = 0.02, .y = 0.02},
            0.04
        )
    );
    assert(
        !otir_trackir_mouse_is_inside_deadzone(
            (otir_trackir_mouse_point){.x = 0.0, .y = 0.0},
            0.04
        )
    );
    assert(
        !otir_trackir_mouse_is_inside_deadzone(
            (otir_trackir_mouse_point){.x = 0.08, .y = 0.0},
            0.04
        )
    );
}

static void test_mouse_tracker_applies_adaptive_smoothing(void) {
    otir_trackir_mouse_tracker_state state = {0};
    otir_trackir_mouse_tracker_config config = {
        .is_movement_enabled = true,
        .speed = 10.0,
        .smoothing = 3.0,
        .deadzone = 0.04,
        .avoid_mouse_jumps = true,
        .jump_threshold_pixels = 50.0,
        .transform = {
            .scale_x = 1.0,
            .scale_y = 1.0,
            .rotation_degrees = 0.0,
        },
    };
    otir_trackir_mouse_step step;

    step = otir_trackir_mouse_tracker_update(
        &state,
        true,
        (otir_trackir_mouse_point){.x = 10.0, .y = 10.0},
        config
    );
    assert(!step.has_cursor_delta);

    step = otir_trackir_mouse_tracker_update(
        &state,
        true,
        (otir_trackir_mouse_point){.x = 10.02, .y = 10.01},
        config
    );
    assert(!step.has_cursor_delta);

    step = otir_trackir_mouse_tracker_update(
        &state,
        true,
        (otir_trackir_mouse_point){.x = 10.52, .y = 10.01},
        config
    );
    assert(step.has_cursor_delta);
    assert(fabs(step.cursor_delta.x - 2.6) < 0.0001);
    assert(fabs(step.cursor_delta.y - 0.0625) < 0.0001);

    step = otir_trackir_mouse_tracker_update(
        &state,
        true,
        (otir_trackir_mouse_point){.x = 12.52, .y = 10.01},
        config
    );
    assert(step.has_cursor_delta);
    assert(fabs(step.cursor_delta.x - 20.0) < 0.0001);
    assert(fabs(step.cursor_delta.y) < 0.0001);

    step = otir_trackir_mouse_tracker_update(
        &state,
        true,
        (otir_trackir_mouse_point){.x = 80.0, .y = 10.1},
        config
    );
    assert(!step.has_cursor_delta);
}

static void test_mouse_step_suppresses_zero_delta(void) {
    otir_trackir_mouse_step first_step = otir_trackir_mouse_compute_step(
        false,
        (otir_trackir_mouse_point){0},
        true,
        (otir_trackir_mouse_point){.x = 10.0, .y = 20.0},
        true,
        1.0,
        (otir_trackir_mouse_transform){
            .scale_x = 1.0,
            .scale_y = 1.0,
            .rotation_degrees = 0.0,
        }
    );
    otir_trackir_mouse_step zero_delta_step = otir_trackir_mouse_compute_step(
        true,
        first_step.next_centroid,
        true,
        (otir_trackir_mouse_point){.x = 10.0, .y = 20.0},
        true,
        1.0,
        (otir_trackir_mouse_transform){
            .scale_x = 1.0,
            .scale_y = 1.0,
            .rotation_degrees = 0.0,
        }
    );

    assert(!first_step.has_cursor_delta);
    assert(first_step.has_next_centroid);
    assert(!zero_delta_step.has_cursor_delta);
    assert(zero_delta_step.has_next_centroid);
    assert(otir_trackir_mouse_point_is_zero(zero_delta_step.cursor_delta));
    assert(otir_trackir_mouse_short_smoothing_window(3.0) == 3);
    assert(otir_trackir_mouse_long_smoothing_window(3.0) == 10);
}

static void test_normalize_maximum_frames_per_second_rejects_invalid_values(void) {
    assert(otir_tir5v3_normalize_maximum_frames_per_second(60.0) == 60.0);
    assert(otir_tir5v3_normalize_maximum_frames_per_second(0.0) == 0.0);
    assert(otir_tir5v3_normalize_maximum_frames_per_second(-15.0) == 0.0);
    assert(otir_tir5v3_normalize_maximum_frames_per_second(NAN) == 0.0);
}

static void test_should_process_frame_respects_processing_cap(void) {
    assert(otir_tir5v3_should_process_frame(10.0, 0.0, false, 60.0));
    assert(otir_tir5v3_should_process_frame(10.0, 9.0, true, 0.0));
    assert(!otir_tir5v3_should_process_frame(10.01, 10.0, true, 60.0));
    assert(otir_tir5v3_should_process_frame(10.02, 10.0, true, 60.0));
}

static void test_cli_read_maximum_frames_per_second_accepts_optional_argument(void) {
    const char *without_flag[] = {"stream_dump"};
    const char *separate_flag[] = {"stream_dump", "--fps", "60"};
    const char *inline_flag[] = {"stream_dump", "--fps=30"};
    const char *invalid_flag[] = {"stream_dump", "--fps", "-1"};
    double frames_per_second = -1.0;

    assert(otir_cli_read_maximum_frames_per_second(1, without_flag, &frames_per_second) == OTIR_STATUS_OK);
    assert(frames_per_second == 0.0);

    assert(otir_cli_read_maximum_frames_per_second(3, separate_flag, &frames_per_second) == OTIR_STATUS_OK);
    assert(frames_per_second == 60.0);

    assert(otir_cli_read_maximum_frames_per_second(2, inline_flag, &frames_per_second) == OTIR_STATUS_OK);
    assert(frames_per_second == 30.0);

    assert(otir_cli_read_maximum_frames_per_second(3, invalid_flag, &frames_per_second) == OTIR_STATUS_INVALID_ARGUMENT);
}
