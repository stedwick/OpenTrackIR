#include "opentrackir/tir5.h"

#include <limits.h>
#include <string.h>

typedef struct packet_search_result {
    bool has_packet;
    bool has_incomplete_start;
    size_t packet_start;
    size_t packet_length;
    size_t incomplete_start;
} packet_search_result;

static otir_status decode_tir5v3_stripe(const uint8_t *payload, otir_tir5v3_stripe *out_stripe);
static otir_status decode_tir4_style_stripe(const uint8_t *payload, otir_tir5v3_stripe *out_stripe);
static bool looks_like_packet_start(const uint8_t *data, size_t length);
static bool looks_like_packet_prefix(const uint8_t *data, size_t length);
static bool extract_stream_packet_length(const uint8_t *data, size_t length, size_t *packet_length);
static packet_search_result find_next_complete_packet(const uint8_t *data, size_t length);
static void retain_pending_tail(otir_tir5v3_stream_parser *parser);

const char *otir_status_string(otir_status status) {
    switch (status) {
        case OTIR_STATUS_OK:
            return "ok";
        case OTIR_STATUS_AGAIN:
            return "again";
        case OTIR_STATUS_INVALID_ARGUMENT:
            return "invalid_argument";
        case OTIR_STATUS_BUFFER_TOO_SMALL:
            return "buffer_too_small";
        case OTIR_STATUS_OVERFLOW:
            return "overflow";
        case OTIR_STATUS_BAD_PACKET:
            return "bad_packet";
        case OTIR_STATUS_TIMEOUT:
            return "timeout";
        case OTIR_STATUS_IO:
            return "io";
        case OTIR_STATUS_NOT_OPEN:
            return "not_open";
        case OTIR_STATUS_NOT_FOUND:
            return "not_found";
        case OTIR_STATUS_DEPENDENCY_UNAVAILABLE:
            return "dependency_unavailable";
        case OTIR_STATUS_UNSUPPORTED:
            return "unsupported";
        case OTIR_STATUS_PERMISSION_DENIED:
            return "permission_denied";
        default:
            return "unknown";
    }
}

otir_status otir_tir5v3_apply_transport(
    const uint8_t packet[24],
    uint8_t r1,
    uint8_t encoded[24]
) {
    if (packet == NULL || encoded == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    memcpy(encoded, packet, 24);
    encoded[0] ^= (uint8_t)(0x69U ^ encoded[r1 >> 4] ^ encoded[r1 & 0x0FU]);
    encoded[17] = r1;
    return OTIR_STATUS_OK;
}

bool otir_tir5v3_is_stream_header(const uint8_t *data, size_t length) {
    if (data == NULL || length < 4) {
        return false;
    }

    return data[1] == 0x10U
        && (data[2] == 0x00U || data[2] == 0x05U)
        && (uint8_t)(data[0] ^ data[1] ^ data[2] ^ data[3]) == 0xAAU;
}

bool otir_tir5v3_is_type3_prefix(const uint8_t *data, size_t length) {
    return data != NULL && length >= 3 && data[0] >= 3 && data[1] == 0x10U && data[2] == 0x03U;
}

void otir_tir5v3_stream_parser_init(otir_tir5v3_stream_parser *parser) {
    if (parser == NULL) {
        return;
    }

    memset(parser->pending, 0, sizeof(parser->pending));
    parser->pending_length = 0;
}

otir_status otir_tir5v3_stream_parser_push(
    otir_tir5v3_stream_parser *parser,
    const uint8_t *data,
    size_t length
) {
    if (parser == NULL || (length > 0 && data == NULL)) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (parser->pending_length + length > sizeof(parser->pending)) {
        return OTIR_STATUS_OVERFLOW;
    }

    memcpy(parser->pending + parser->pending_length, data, length);
    parser->pending_length += length;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_stream_parser_push_resync(
    otir_tir5v3_stream_parser *parser,
    const uint8_t *data,
    size_t length,
    bool *did_resync
) {
    otir_status status;

    if (did_resync != NULL) {
        *did_resync = false;
    }

    status = otir_tir5v3_stream_parser_push(parser, data, length);
    if (status != OTIR_STATUS_OVERFLOW) {
        return status;
    }

    otir_tir5v3_stream_parser_init(parser);
    if (did_resync != NULL) {
        *did_resync = true;
    }

    return otir_tir5v3_stream_parser_push(parser, data, length);
}

otir_status otir_tir5v3_stream_parser_next_packet(
    otir_tir5v3_stream_parser *parser,
    uint8_t *packet_buffer,
    size_t packet_capacity,
    size_t *packet_length
) {
    packet_search_result result;
    size_t remaining_length;

    if (parser == NULL || packet_buffer == NULL || packet_length == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    result = find_next_complete_packet(parser->pending, parser->pending_length);
    if (!result.has_packet) {
        if (result.has_incomplete_start) {
            if (result.incomplete_start > 0) {
                remaining_length = parser->pending_length - result.incomplete_start;
                memmove(parser->pending, parser->pending + result.incomplete_start, remaining_length);
                parser->pending_length = remaining_length;
            }
        } else {
            retain_pending_tail(parser);
        }
        *packet_length = 0;
        return OTIR_STATUS_AGAIN;
    }

    if (result.packet_length > packet_capacity) {
        return OTIR_STATUS_BUFFER_TOO_SMALL;
    }

    memcpy(packet_buffer, parser->pending + result.packet_start, result.packet_length);
    *packet_length = result.packet_length;

    remaining_length = parser->pending_length - (result.packet_start + result.packet_length);
    memmove(
        parser->pending,
        parser->pending + result.packet_start + result.packet_length,
        remaining_length
    );
    parser->pending_length = remaining_length;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_parse_status(
    const uint8_t *packet,
    size_t length,
    otir_tir5v3_status *out_status
) {
    if (packet == NULL || out_status == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (length < 7 || packet[1] != 0x20U) {
        return OTIR_STATUS_BAD_PACKET;
    }

    memset(out_status, 0, sizeof(*out_status));
    out_status->raw_length = length > sizeof(out_status->raw) ? sizeof(out_status->raw) : length;
    memcpy(out_status->raw, packet, out_status->raw_length);
    out_status->length = packet[0];
    out_status->message_id = packet[1];
    out_status->stage = packet[2];
    out_status->firmware_loaded = packet[3] == 0x01U;
    out_status->status_flag = packet[4];
    out_status->checksum_hi = packet[5];
    out_status->checksum_lo = packet[6];
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_parse_packet(
    const uint8_t *packet,
    size_t length,
    otir_tir5v3_packet *out_packet
) {
    size_t payload_size;
    const uint8_t *payload;
    size_t offset;
    size_t stripe_size;
    otir_status status;

    if (packet == NULL || out_packet == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (length < 3) {
        return OTIR_STATUS_BAD_PACKET;
    }
    if (length > sizeof(out_packet->raw)) {
        return OTIR_STATUS_BUFFER_TOO_SMALL;
    }

    memset(out_packet, 0, sizeof(*out_packet));
    memcpy(out_packet->raw, packet, length);
    out_packet->raw_length = length;
    out_packet->packet_no = -1;

    if (otir_tir5v3_is_stream_header(packet, length)) {
        if (length < 8) {
            return OTIR_STATUS_BAD_PACKET;
        }

        payload_size = ((size_t)packet[length - 4] << 24)
            | ((size_t)packet[length - 3] << 16)
            | ((size_t)packet[length - 2] << 8)
            | (size_t)packet[length - 1];
        if (payload_size != length - 8) {
            return OTIR_STATUS_BAD_PACKET;
        }

        out_packet->packet_no = (int)packet[0];
        out_packet->packet_type = packet[2];
        out_packet->payload_size = payload_size;

        payload = packet + 4;
        stripe_size = out_packet->packet_type == 0x05U ? 8U : 4U;
        for (offset = 0; offset + stripe_size <= payload_size; offset += stripe_size) {
            if (out_packet->stripe_count >= OTIR_TIR5V3_MAX_STRIPES) {
                return OTIR_STATUS_OVERFLOW;
            }

            if (out_packet->packet_type == 0x05U) {
                status = decode_tir5v3_stripe(payload + offset, &out_packet->stripes[out_packet->stripe_count]);
            } else {
                status = decode_tir4_style_stripe(payload + offset, &out_packet->stripes[out_packet->stripe_count]);
            }
            if (status != OTIR_STATUS_OK) {
                return status;
            }
            out_packet->stripe_count += 1;
        }
        return OTIR_STATUS_OK;
    }

    if (otir_tir5v3_is_type3_prefix(packet, length)) {
        out_packet->packet_type = 0x03U;
        out_packet->payload_size = length > 2 ? length - 2 : 0;
        return OTIR_STATUS_OK;
    }

    return OTIR_STATUS_BAD_PACKET;
}

bool otir_tir5v3_compute_weighted_centroid(
    const otir_tir5v3_stripe *stripes,
    size_t stripe_count,
    double *out_x,
    double *out_y
) {
    size_t index;
    long long total_weight = 0;
    long long sum_x = 0;
    long long sum_y = 0;

    if (stripes == NULL || out_x == NULL || out_y == NULL || stripe_count == 0) {
        return false;
    }

    for (index = 0; index < stripe_count; ++index) {
        if (stripes[index].sum <= 0) {
            continue;
        }
        total_weight += stripes[index].sum;
        sum_x += (long long)stripes[index].sum * stripes[index].hstart + stripes[index].sum_x;
        sum_y += (long long)stripes[index].sum * stripes[index].vline;
    }

    if (total_weight <= 0) {
        return false;
    }

    *out_x = (double)sum_x / (double)total_weight;
    *out_y = (double)sum_y / (double)total_weight;
    return true;
}

size_t otir_tir5v3_shutdown_steps(
    bool is_streaming,
    bool ir_led_enabled,
    otir_tir5v3_shutdown_step *steps,
    size_t capacity
) {
    size_t count = 0;

    if (is_streaming) {
        if (count < capacity) {
            steps[count].action = OTIR_TIR5V3_SHUTDOWN_INTENT_2;
            steps[count].value_count = 2;
            steps[count].values[0] = 0x1A;
            steps[count].values[1] = 0x05;
        }
        count += 1;
    }
    if (ir_led_enabled) {
        if (count < capacity) {
            steps[count].action = OTIR_TIR5V3_SHUTDOWN_INTENT_4;
            steps[count].value_count = 4;
            steps[count].values[0] = 0x19;
            steps[count].values[1] = 0x09;
            steps[count].values[2] = 0x00;
            steps[count].values[3] = 0x00;
        }
        count += 1;
    }
    if (is_streaming) {
        if (count < capacity) {
            steps[count].action = OTIR_TIR5V3_SHUTDOWN_INTENT_2;
            steps[count].value_count = 2;
            steps[count].values[0] = 0x1A;
            steps[count].values[1] = 0x06;
        }
        count += 1;

        if (count < capacity) {
            steps[count].action = OTIR_TIR5V3_SHUTDOWN_INTENT_1;
            steps[count].value_count = 1;
            steps[count].values[0] = 0x13;
        }
        count += 1;
    }

    return count;
}

static otir_status decode_tir5v3_stripe(const uint8_t *payload, otir_tir5v3_stripe *out_stripe) {
    int hstart;
    int vline;
    int points;
    int sum_x;
    int stripe_sum;

    if (payload == NULL || out_stripe == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    hstart = ((int)payload[0] << 2) | (payload[1] >> 6);
    vline = ((int)(payload[1] & 0x3FU) << 3) | ((payload[2] & 0xE0U) >> 5);
    points = ((int)(payload[2] & 0x1FU) << 5) | (payload[3] >> 3);
    sum_x = ((int)(payload[3] & 0x07U) << 17)
        | ((int)payload[4] << 9)
        | ((int)payload[5] << 1)
        | (payload[6] >> 7);
    stripe_sum = ((int)(payload[6] & 0x7FU) << 8) | payload[7];

    out_stripe->hstart = hstart;
    out_stripe->hstop = hstart + points - 1;
    out_stripe->vline = vline;
    out_stripe->points = points;
    out_stripe->sum_x = sum_x;
    out_stripe->sum = stripe_sum;
    return OTIR_STATUS_OK;
}

static otir_status decode_tir4_style_stripe(const uint8_t *payload, otir_tir5v3_stripe *out_stripe) {
    int vline;
    int hstart;
    int hstop;
    uint8_t rest;
    int points;

    if (payload == NULL || out_stripe == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    vline = payload[0];
    hstart = payload[1];
    hstop = payload[2];
    rest = payload[3];
    if ((rest & 0x20U) != 0U) {
        vline |= 0x100;
    }
    if ((rest & 0x80U) != 0U) {
        hstart |= 0x100;
    }
    if ((rest & 0x40U) != 0U) {
        hstop |= 0x100;
    }
    if ((rest & 0x10U) != 0U) {
        hstart |= 0x200;
    }
    if ((rest & 0x08U) != 0U) {
        hstop |= 0x200;
    }

    points = hstop - hstart + 1;
    out_stripe->hstart = hstart;
    out_stripe->hstop = hstop;
    out_stripe->vline = vline;
    out_stripe->points = points;
    out_stripe->sum_x = points * (points - 1) / 2;
    out_stripe->sum = points;
    return OTIR_STATUS_OK;
}

static bool looks_like_packet_start(const uint8_t *data, size_t length) {
    return otir_tir5v3_is_stream_header(data, length) || otir_tir5v3_is_type3_prefix(data, length);
}

static bool looks_like_packet_prefix(const uint8_t *data, size_t length) {
    if (length == 0) {
        return true;
    }
    if (looks_like_packet_start(data, length)) {
        return true;
    }
    if (length == 1) {
        return true;
    }
    if (length == 2) {
        return data[1] == 0x10U;
    }
    if (length == 3) {
        return data[1] == 0x10U && (data[2] == 0x00U || data[2] == 0x03U || data[2] == 0x05U);
    }
    return false;
}

static bool extract_stream_packet_length(const uint8_t *data, size_t length, size_t *packet_length) {
    size_t candidate;
    size_t payload_size;

    if (data == NULL || packet_length == NULL || length < 8) {
        return false;
    }

    for (candidate = 8; candidate <= length; candidate += 8) {
        payload_size = ((size_t)data[candidate - 4] << 24)
            | ((size_t)data[candidate - 3] << 16)
            | ((size_t)data[candidate - 2] << 8)
            | (size_t)data[candidate - 1];
        if (payload_size != candidate - 8) {
            continue;
        }
        if (candidate < length && !looks_like_packet_prefix(data + candidate, length - candidate)) {
            continue;
        }
        *packet_length = candidate;
        return true;
    }

    return false;
}

static packet_search_result find_next_complete_packet(const uint8_t *data, size_t length) {
    packet_search_result result;
    size_t index;
    size_t remaining_length;
    size_t packet_length;

    result.has_packet = false;
    result.has_incomplete_start = false;
    result.packet_start = 0;
    result.packet_length = 0;
    result.incomplete_start = 0;

    if (data == NULL) {
        return result;
    }

    for (index = 0; index + 2 < length; ++index) {
        remaining_length = length - index;
        if (otir_tir5v3_is_stream_header(data + index, remaining_length)) {
            if (extract_stream_packet_length(data + index, remaining_length, &packet_length)) {
                result.has_packet = true;
                result.packet_start = index;
                result.packet_length = packet_length;
                return result;
            }
            if (!result.has_incomplete_start) {
                result.has_incomplete_start = true;
                result.incomplete_start = index;
            }
            continue;
        }

        if (otir_tir5v3_is_type3_prefix(data + index, remaining_length)) {
            packet_length = data[index];
            if (remaining_length >= packet_length) {
                result.has_packet = true;
                result.packet_start = index;
                result.packet_length = packet_length;
                return result;
            }
            if (!result.has_incomplete_start) {
                result.has_incomplete_start = true;
                result.incomplete_start = index;
            }
        }
    }

    return result;
}

static void retain_pending_tail(otir_tir5v3_stream_parser *parser) {
    size_t keep_length;

    if (parser->pending_length <= 7) {
        return;
    }

    keep_length = 7;
    memmove(parser->pending, parser->pending + parser->pending_length - keep_length, keep_length);
    parser->pending_length = keep_length;
}
