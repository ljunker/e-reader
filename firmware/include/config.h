#pragma once

// Display mounted with the flex cable at the top of the case -> portrait.
// If the picture is upside down, use 3 instead of 1.
#define DISPLAY_ROTATION_PORTRAIT  1
#define DISPLAY_ROTATION_LANDSCAPE 0

// Full refresh after this many fast (partial) refreshes to clear ghosting.
#define FULL_REFRESH_EVERY 6

// Idle time before deep sleep.
#define SLEEP_AFTER_MS (3UL * 60UL * 1000UL)

// Battery: FeatherS3 divider (verify with a multimeter and adjust).
#define VBAT_SCALE 1.3619f
#define VBAT_EMPTY_MV 3300
#define VBAT_FULL_MV  4150

// Reading layout
#define MARGIN_X      12
#define MARGIN_TOP    8
#define STATUS_H      18   // status line at the bottom
#define PARA_INDENT   16

#define BOOKS_DIR "/books"
#define STATE_DIR "/.reader"
