#pragma once

#include "lvgl.h"

#define MARVY_QR_SIZE 180
#define MARVY_QR_MARK_OFFSET 68
#define MARVY_QR_MARK_SIZE 44

/* Position the mark at QR origin + MARK_OFFSET, with neither image scaled. */
LV_IMAGE_DECLARE(marvy_follow_qr);
LV_IMAGE_DECLARE(marvy_follow_qr_mark);
