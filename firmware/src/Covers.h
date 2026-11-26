#pragma once
#include <Arduino.h>

#include "Storage.h"

// Book covers, cached in /.reader/<hash>.cov (format: lib/cover/src/Cover.h).
// EPUB: the cover picture named in the package. .xtp: the first page.
// Plain text has no cover.
namespace covers {

enum class State { Missing, None, Present };

// Is there a valid cache file? Missing means it still has to be made.
State state(const BookEntry& book);

// Make the cache file (decodes the picture, up to about a second).
// Returns the resulting state; never Missing unless the card is not writable.
State build(const BookEntry& book);

// Thumbnail centred in the cover::kThumbW × kThumbH box at x, y.
State drawThumb(const BookEntry& book, int x, int y);

// The cover on the whole (portrait) panel. Builds it first if needed.
bool drawFull(const BookEntry& book);

}  // namespace covers
