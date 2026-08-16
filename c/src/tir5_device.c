#include "opentrackir/tir5.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#include <libusb.h>

#include "tir5_libusb_status.h"

struct otir_tir5v3_device {
    bool ir_led_enabled;
    bool is_streaming;
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uint8_t interface_number;
    uint16_t max_packet_size;
    uint32_t rng_state;
    otir_tir5v3_stream_parser parser;
    libusb_context *context;
    libusb_device_handle *handle;
};

static uint64_t monotonic_ms(void);
static void sleep_us(unsigned int delay_us);
static uint32_t random_next(otir_tir5v3_device *device);
static uint8_t random_byte(otir_tir5v3_device *device);
static int random_range(otir_tir5v3_device *device, int low, int high);

static void log_libusb_failure(const char *operation, int error_code);
static void log_device_status(const char *message);
static void log_parser_status(const char *message, size_t bytes_read);
static otir_status open_matching_device(otir_tir5v3_device *device);
static otir_status discover_endpoints(otir_tir5v3_device *device);
static otir_status send_packet(
    otir_tir5v3_device *device,
    const uint8_t *packet,
    unsigned int wait_us
);
static otir_status send_intent_1(otir_tir5v3_device *device, uint8_t value_1, unsigned int wait_us);
static otir_status send_intent_2(
    otir_tir5v3_device *device,
    uint8_t value_1,
    uint8_t value_2,
    unsigned int wait_us
);
static otir_status send_intent_4(
    otir_tir5v3_device *device,
    uint8_t value_1,
    uint8_t value_2,
    uint8_t value_3,
    uint8_t value_4,
    unsigned int wait_us
);
static otir_status read_status_packet(otir_tir5v3_device *device, otir_tir5v3_status *out_status);
static otir_status apply_shutdown_steps(otir_tir5v3_device *device);

otir_status otir_tir5v3_open(otir_tir5v3_device **out_device) {
    otir_tir5v3_device *device;

    if (out_device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    *out_device = NULL;

    device = (otir_tir5v3_device *)calloc(1, sizeof(*device));
    if (device == NULL) {
        return OTIR_STATUS_IO;
    }

    otir_tir5v3_stream_parser_init(&device->parser);
    device->rng_state = (uint32_t)(monotonic_ms() ^ (uintptr_t)device);
    device->interface_number = 0;

    {
        const int init_result = libusb_init(&device->context);
        if (init_result != 0) {
            const otir_status status = otir_tir5v3_map_libusb_error(init_result);

            log_libusb_failure("init", init_result);
            free(device);
            return status;
        }
    }

    {
        const otir_status status = open_matching_device(device);
        if (status != OTIR_STATUS_OK) {
            libusb_exit(device->context);
            free(device);
            return status;
        }
    }

    libusb_set_auto_detach_kernel_driver(device->handle, 1);
    {
        const int configuration_result = libusb_set_configuration(device->handle, 1);
        if (configuration_result != 0) {
            log_libusb_failure("set_configuration", configuration_result);
        }
    }
    {
        const int claim_result = libusb_claim_interface(device->handle, device->interface_number);
        if (claim_result != 0) {
            const otir_status status = otir_tir5v3_map_libusb_error(claim_result);

            log_libusb_failure("claim_interface", claim_result);
            libusb_close(device->handle);
            libusb_exit(device->context);
            free(device);
            return status;
        }
    }
    {
        const otir_status endpoint_status = discover_endpoints(device);
        if (endpoint_status != OTIR_STATUS_OK) {
            log_device_status("TrackIR endpoint discovery failed");
            libusb_release_interface(device->handle, device->interface_number);
            libusb_close(device->handle);
            libusb_exit(device->context);
            free(device);
            return endpoint_status;
        }
    }

    *out_device = device;
    return OTIR_STATUS_OK;
}

static otir_status open_matching_device(otir_tir5v3_device *device) {
    libusb_device **device_list = NULL;
    otir_status matching_device_status = OTIR_STATUS_NOT_FOUND;
    ssize_t device_count;
    ssize_t device_index;

    device_count = libusb_get_device_list(device->context, &device_list);
    if (device_count < 0) {
        log_libusb_failure("get_device_list", (int)device_count);
        return otir_tir5v3_map_libusb_error((int)device_count);
    }

    for (device_index = 0; device_index < device_count; ++device_index) {
        struct libusb_device_descriptor descriptor;
        int descriptor_result;
        int open_result;

        descriptor_result = libusb_get_device_descriptor(device_list[device_index], &descriptor);
        if (descriptor_result != 0) {
            log_libusb_failure("get_device_descriptor", descriptor_result);
            continue;
        }
        if (descriptor.idVendor != OTIR_TIR5V3_VENDOR_ID ||
            descriptor.idProduct != OTIR_TIR5V3_PRODUCT_ID) {
            continue;
        }

        open_result = libusb_open(device_list[device_index], &device->handle);
        if (open_result == 0 && device->handle != NULL) {
            matching_device_status = OTIR_STATUS_OK;
            break;
        }

        log_libusb_failure("open", open_result);
        if (matching_device_status != OTIR_STATUS_PERMISSION_DENIED) {
            matching_device_status = otir_tir5v3_map_libusb_error(open_result);
        }
    }

    libusb_free_device_list(device_list, 1);
    if (matching_device_status == OTIR_STATUS_NOT_FOUND) {
        log_device_status("TrackIR device not found during open");
    }
    return matching_device_status;
}

void otir_tir5v3_close(otir_tir5v3_device *device) {
    if (device == NULL) {
        return;
    }

    if (device->handle != NULL) {
        (void)apply_shutdown_steps(device);
        libusb_release_interface(device->handle, device->interface_number);
        libusb_close(device->handle);
    }
    if (device->context != NULL) {
        libusb_exit(device->context);
    }

    free(device);
}

static void log_libusb_failure(const char *operation, int error_code) {
    fprintf(
        stderr,
        "TrackIR libusb %s failed: %s (%d)\n",
        operation,
        libusb_error_name(error_code),
        error_code
    );
}

static void log_device_status(const char *message) {
    fprintf(stderr, "TrackIR device: %s\n", message);
}

static void log_parser_status(const char *message, size_t bytes_read) {
    fprintf(stderr, "TrackIR parser: %s (bytes=%zu)\n", message, bytes_read);
}

otir_status otir_tir5v3_get_device_summary(
    const otir_tir5v3_device *device,
    otir_tir5v3_device_summary *out_summary
) {
    if (device == NULL || out_summary == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (device->handle == NULL) {
        return OTIR_STATUS_NOT_OPEN;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->vendor_id = OTIR_TIR5V3_VENDOR_ID;
    out_summary->product_id = OTIR_TIR5V3_PRODUCT_ID;
    out_summary->endpoint_in = device->endpoint_in;
    out_summary->endpoint_out = device->endpoint_out;
    out_summary->interface_number = device->interface_number;
    out_summary->max_packet_size = device->max_packet_size;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_initialize(
    otir_tir5v3_device *device,
    otir_tir5v3_status *statuses,
    size_t status_capacity,
    size_t *status_count
) {
    static const uint8_t subcommands[] = {0x01, 0x02, 0x03};
    size_t index;
    otir_status status;

    if (device == NULL || statuses == NULL || status_count == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (status_capacity < OTIR_TIR5V3_INIT_STATUS_COUNT) {
        return OTIR_STATUS_BUFFER_TOO_SMALL;
    }

    otir_tir5v3_stream_parser_init(&device->parser);

    status = send_intent_2(device, 0x1A, 0x00, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_2(device, 0x1A, 0x00, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_1(device, 0x13, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    for (index = 0; index < sizeof(subcommands); ++index) {
        status = read_status_packet(device, &statuses[index]);
        if (status != OTIR_STATUS_OK) {
            return status;
        }
        status = send_intent_2(device, 0x1A, subcommands[index], 50000);
        if (status != OTIR_STATUS_OK) {
            return status;
        }
    }

    status = read_status_packet(device, &statuses[3]);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    status = otir_tir5v3_set_threshold(device, OTIR_TIR5V3_DEFAULT_THRESHOLD);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = otir_tir5v3_set_ir_brightness_raw(device, OTIR_TIR5V3_DEFAULT_BRIGHTNESS_RAW);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = otir_tir5v3_set_ir_led(device, true);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x19, 0x03, 0x00, 0x05, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x19, 0x04, 0x00, 0x00, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    *status_count = OTIR_TIR5V3_INIT_STATUS_COUNT;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_start_streaming(otir_tir5v3_device *device) {
    otir_status status;

    if (device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    status = otir_tir5v3_set_ir_led(device, true);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = otir_tir5v3_set_threshold(device, OTIR_TIR5V3_DEFAULT_THRESHOLD);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = otir_tir5v3_set_threshold(device, OTIR_TIR5V3_DEFAULT_THRESHOLD);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x19, 0x04, 0x00, 0x00, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x19, 0x03, 0x00, 0x05, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_2(device, 0x1A, 0x04, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = otir_tir5v3_set_ir_led(device, true);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    device->is_streaming = true;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_stop_streaming(otir_tir5v3_device *device) {
    otir_status status;

    if (device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    status = apply_shutdown_steps(device);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    device->is_streaming = false;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_set_threshold(otir_tir5v3_device *device, uint16_t threshold) {
    if (device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    return send_intent_4(
        device,
        0x19,
        0x05,
        (uint8_t)(threshold >> 7),
        (uint8_t)((threshold << 1) & 0xFFU),
        50000
    );
}

otir_status otir_tir5v3_set_ir_led(otir_tir5v3_device *device, bool enabled) {
    otir_status status;

    if (device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    status = send_intent_4(device, 0x19, 0x09, 0x00, enabled ? 0x01 : 0x00, 50000);
    if (status == OTIR_STATUS_OK) {
        device->ir_led_enabled = enabled;
    }
    return status;
}

otir_status otir_tir5v3_set_ir_brightness_raw(otir_tir5v3_device *device, uint16_t brightness) {
    otir_status status;

    if (device == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    status = send_intent_4(device, 0x23, 0x35, 0x02, (uint8_t)(brightness & 0xFFU), 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x23, 0x35, 0x01, (uint8_t)(brightness >> 8), 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x23, 0x35, 0x00, 0x00, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    status = send_intent_4(device, 0x23, 0x3B, 0x8F, (uint8_t)((brightness >> 4) & 0xFFU), 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }
    return send_intent_4(device, 0x23, 0x3B, 0x8E, (uint8_t)(brightness >> 12), 50000);
}

otir_status otir_tir5v3_read_chunk(
    otir_tir5v3_device *device,
    int timeout_ms,
    uint8_t *buffer,
    size_t buffer_capacity,
    size_t *bytes_read
) {
    int transferred = 0;
    int rc;

    if (device == NULL || buffer == NULL || bytes_read == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }
    if (buffer_capacity == 0 || buffer_capacity > INT_MAX) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    rc = libusb_bulk_transfer(
        device->handle,
        device->endpoint_in,
        buffer,
        (int)buffer_capacity,
        &transferred,
        timeout_ms
    );
    if (rc == LIBUSB_ERROR_TIMEOUT) {
        *bytes_read = 0;
        return OTIR_STATUS_OK;
    }
    if (rc != 0) {
        return otir_tir5v3_map_libusb_error(rc);
    }

    *bytes_read = (size_t)transferred;
    return OTIR_STATUS_OK;
}

otir_status otir_tir5v3_read_packet(
    otir_tir5v3_device *device,
    int timeout_ms,
    otir_tir5v3_packet *out_packet
) {
    uint64_t deadline_ms;
    uint8_t packet_buffer[OTIR_TIR5V3_MAX_PACKET_SIZE];
    size_t packet_length = 0;
    otir_status status;

    if (device == NULL || out_packet == NULL) {
        return OTIR_STATUS_INVALID_ARGUMENT;
    }

    deadline_ms = monotonic_ms() + (timeout_ms > 0 ? (uint64_t)timeout_ms : 0);
    while (timeout_ms <= 0 || monotonic_ms() <= deadline_ms) {
        size_t bytes_read = 0;
        uint8_t chunk[OTIR_TIR5V3_READ_SIZE];
        int chunk_timeout_ms;

        status = otir_tir5v3_stream_parser_next_packet(
            &device->parser,
            packet_buffer,
            sizeof(packet_buffer),
            &packet_length
        );
        if (status == OTIR_STATUS_OK) {
            return otir_tir5v3_parse_packet(packet_buffer, packet_length, out_packet);
        }
        if (status != OTIR_STATUS_AGAIN) {
            return status;
        }

        if (timeout_ms > 0) {
            uint64_t remaining_ms = deadline_ms > monotonic_ms() ? deadline_ms - monotonic_ms() : 0;
            if (remaining_ms == 0) {
                break;
            }
            chunk_timeout_ms = remaining_ms > 50 ? 50 : (int)remaining_ms;
        } else {
            chunk_timeout_ms = 50;
        }

        status = otir_tir5v3_read_chunk(device, chunk_timeout_ms, chunk, sizeof(chunk), &bytes_read);
        if (status != OTIR_STATUS_OK) {
            return status;
        }
        if (bytes_read == 0) {
            continue;
        }

        {
            bool did_resync = false;

            status = otir_tir5v3_stream_parser_push_resync(
                &device->parser,
                chunk,
                bytes_read,
                &did_resync
            );
            if (did_resync) {
                log_parser_status("overflow while pushing stream chunk; parser state reset", bytes_read);
            }
        }
        if (status != OTIR_STATUS_OK) {
            return status;
        }
    }

    return OTIR_STATUS_TIMEOUT;
}

static uint64_t monotonic_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000ULL) / (uint64_t)frequency.QuadPart);
#else
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000ULL + (uint64_t)(now.tv_nsec / 1000000ULL);
#endif
}

static void sleep_us(unsigned int delay_us) {
#if defined(_WIN32)
    Sleep((DWORD)((delay_us + 999U) / 1000U));
#else
    struct timespec delay;

    delay.tv_sec = delay_us / 1000000U;
    delay.tv_nsec = (long)((delay_us % 1000000U) * 1000U);
    nanosleep(&delay, NULL);
#endif
}

static uint32_t random_next(otir_tir5v3_device *device) {
    uint32_t x = device->rng_state;

    if (x == 0) {
        x = 0x6B84221DU;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    device->rng_state = x;
    return x;
}

static uint8_t random_byte(otir_tir5v3_device *device) {
    return (uint8_t)(random_next(device) & 0xFFU);
}

static int random_range(otir_tir5v3_device *device, int low, int high) {
    uint32_t span;

    if (high <= low) {
        return high;
    }

    span = (uint32_t)(high - low + 1);
    return low + (int)(random_next(device) % span);
}

static otir_status discover_endpoints(otir_tir5v3_device *device) {
    libusb_device *usb_device;
    struct libusb_config_descriptor *config = NULL;
    const struct libusb_interface_descriptor *interface_desc;
    int endpoint_index;

    usb_device = libusb_get_device(device->handle);
    {
        const int config_result = libusb_get_active_config_descriptor(usb_device, &config);
        if (config_result != 0 || config == NULL) {
            return otir_tir5v3_map_libusb_error(config_result);
        }
    }

    interface_desc = &config->interface[device->interface_number].altsetting[0];
    for (endpoint_index = 0; endpoint_index < interface_desc->bNumEndpoints; ++endpoint_index) {
        const struct libusb_endpoint_descriptor *endpoint = &interface_desc->endpoint[endpoint_index];
        if ((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN) {
            device->endpoint_in = endpoint->bEndpointAddress;
            device->max_packet_size = endpoint->wMaxPacketSize;
        } else {
            device->endpoint_out = endpoint->bEndpointAddress;
        }
    }

    libusb_free_config_descriptor(config);
    if (device->endpoint_in == 0 || device->endpoint_out == 0) {
        return OTIR_STATUS_IO;
    }

    return OTIR_STATUS_OK;
}

static otir_status send_packet(
    otir_tir5v3_device *device,
    const uint8_t *packet,
    unsigned int wait_us
) {
    uint8_t encoded[24];
    uint8_t r1;
    int transferred = 0;
    int rc;
    otir_status status;

    r1 = (uint8_t)((random_range(device, 1, 15) << 4) + random_range(device, 1, 15));
    status = otir_tir5v3_apply_transport(packet, r1, encoded);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    rc = libusb_bulk_transfer(
        device->handle,
        device->endpoint_out,
        encoded,
        (int)sizeof(encoded),
        &transferred,
        1000
    );
    if (rc != 0 || transferred != (int)sizeof(encoded)) {
        return otir_tir5v3_map_libusb_error(rc);
    }
    if (wait_us > 0) {
        sleep_us(wait_us);
    }
    return OTIR_STATUS_OK;
}

static otir_status send_intent_1(otir_tir5v3_device *device, uint8_t value_1, unsigned int wait_us) {
    uint8_t packet[24];
    size_t index;

    packet[0] = value_1;
    for (index = 1; index < sizeof(packet); ++index) {
        packet[index] = random_byte(device);
    }
    return send_packet(device, packet, wait_us);
}

static otir_status send_intent_2(
    otir_tir5v3_device *device,
    uint8_t value_1,
    uint8_t value_2,
    unsigned int wait_us
) {
    uint8_t packet[24];
    size_t index;
    int r1;
    int r2;

    packet[0] = value_1;
    for (index = 1; index < sizeof(packet); ++index) {
        packet[index] = random_byte(device);
    }

    r1 = random_range(device, 2, 14);
    r2 = random_range(device, 0, 3);
    packet[1] ^= (uint8_t)((packet[1] ^ r1) & 0x08);
    packet[2] ^= (uint8_t)((packet[2] ^ r1) & 0x04);
    packet[3] ^= (uint8_t)((packet[3] ^ r1) & 0x02);
    packet[4] ^= (uint8_t)((packet[4] ^ r1) & 0x01);
    packet[r1] = (uint8_t)(((value_2 << 4) | (packet[r1] & 0x0F)) ^ (packet[16] & 0xF0));
    packet[r1 + 1] = (uint8_t)((r2 << 6) | (packet[r1 + 1] & 0x3F));
    return send_packet(device, packet, wait_us);
}

static otir_status send_intent_4(
    otir_tir5v3_device *device,
    uint8_t value_1,
    uint8_t value_2,
    uint8_t value_3,
    uint8_t value_4,
    unsigned int wait_us
) {
    uint8_t packet[24];
    size_t index;
    int r1;

    packet[0] = value_1;
    for (index = 1; index < sizeof(packet); ++index) {
        packet[index] = random_byte(device);
    }

    r1 = random_range(device, 6, 14);
    packet[1] ^= (uint8_t)((packet[1] ^ r1) & 0x08);
    packet[2] ^= (uint8_t)((packet[2] ^ r1) & 0x04);
    packet[3] ^= (uint8_t)((packet[3] ^ r1) & 0x02);
    packet[4] ^= (uint8_t)((packet[4] ^ r1) & 0x01);
    packet[r1] = (uint8_t)(packet[16] ^ value_2);
    packet[r1 - 1] = (uint8_t)(packet[19] ^ value_3);
    packet[r1 + 1] = (uint8_t)(packet[18] ^ value_4);
    return send_packet(device, packet, wait_us);
}

static otir_status read_status_packet(otir_tir5v3_device *device, otir_tir5v3_status *out_status) {
    uint64_t deadline_ms;
    otir_status status;

    status = send_intent_2(device, 0x1A, 0x07, 50000);
    if (status != OTIR_STATUS_OK) {
        return status;
    }

    deadline_ms = monotonic_ms() + 2000U;
    while (monotonic_ms() <= deadline_ms) {
        uint8_t chunk[OTIR_TIR5V3_READ_SIZE];
        size_t bytes_read = 0;

        status = otir_tir5v3_read_chunk(device, 100, chunk, sizeof(chunk), &bytes_read);
        if (status != OTIR_STATUS_OK) {
            return status;
        }
        if (bytes_read == 0) {
            continue;
        }

        status = otir_tir5v3_parse_status(chunk, bytes_read, out_status);
        if (status == OTIR_STATUS_OK) {
            return OTIR_STATUS_OK;
        }
    }

    return OTIR_STATUS_TIMEOUT;
}

static otir_status apply_shutdown_steps(otir_tir5v3_device *device) {
    otir_tir5v3_shutdown_step steps[4];
    size_t count;
    size_t index;
    otir_status status;

    count = otir_tir5v3_shutdown_steps(device->is_streaming, device->ir_led_enabled, steps, 4);
    for (index = 0; index < count; ++index) {
        const otir_tir5v3_shutdown_step *step = &steps[index];
        if (step->action == OTIR_TIR5V3_SHUTDOWN_INTENT_1) {
            status = send_intent_1(device, step->values[0], 50000);
        } else if (step->action == OTIR_TIR5V3_SHUTDOWN_INTENT_2) {
            status = send_intent_2(device, step->values[0], step->values[1], 50000);
        } else if (step->action == OTIR_TIR5V3_SHUTDOWN_INTENT_4) {
            status = send_intent_4(
                device,
                step->values[0],
                step->values[1],
                step->values[2],
                step->values[3],
                50000
            );
            if (status == OTIR_STATUS_OK && step->values[0] == 0x19 && step->values[1] == 0x09 && step->values[2] == 0x00) {
                device->ir_led_enabled = step->values[3] != 0;
            }
        } else {
            return OTIR_STATUS_UNSUPPORTED;
        }
        if (status != OTIR_STATUS_OK) {
            return status;
        }
    }
    return OTIR_STATUS_OK;
}
