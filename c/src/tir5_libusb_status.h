#ifndef OPENTRACKIR_TIR5_LIBUSB_STATUS_H
#define OPENTRACKIR_TIR5_LIBUSB_STATUS_H

#include <libusb.h>

#include "opentrackir/tir5.h"

static inline otir_status otir_tir5v3_map_libusb_error(int error_code) {
    switch (error_code) {
        case LIBUSB_ERROR_TIMEOUT:
            return OTIR_STATUS_TIMEOUT;
        case LIBUSB_ERROR_ACCESS:
            return OTIR_STATUS_PERMISSION_DENIED;
        case LIBUSB_ERROR_NOT_FOUND:
        case LIBUSB_ERROR_NO_DEVICE:
            return OTIR_STATUS_NOT_FOUND;
        default:
            return OTIR_STATUS_IO;
    }
}

#endif
