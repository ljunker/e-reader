#pragma once

// Display mounted with the flex cable at the top of the case -> portrait.
// If the picture is upside down, use 3 instead of 1.
#define DISPLAY_ROTATION_PORTRAIT  1
#define DISPLAY_ROTATION_LANDSCAPE 0

// Full refresh after this many fast (partial) refreshes to clear ghosting.
#define FULL_REFRESH_EVERY 6

// Idle time before deep sleep.
#define SLEEP_AFTER_MS (3UL * 60UL * 1000UL)

// Reading layout
#define MARGIN_X      12
#define MARGIN_TOP    8
#define STATUS_H      18   // status line at the bottom
#define PARA_INDENT   16

#define BOOKS_DIR "/books"
#define STATE_DIR "/.reader"

// Wi-Fi upload page: networks to try, one "SSID = password" per line.
#define WIFI_FILE       "/wifi.txt"
#define WIFI_CONNECT_MS (20UL * 1000UL)        // give up joining after this
#define WIFI_IDLE_MS    (10UL * 60UL * 1000UL) // switch Wi-Fi off after this long without a request

// Online library: OPDS catalogs, one "Name = URL" per line.
#define OPDS_FILE "/opds.txt"
