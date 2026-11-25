#pragma once
// UTF-8 helpers, folding to the Latin-1 repertoire of the reader fonts, and
// EPUB path handling.

#include <stddef.h>
#include <stdint.h>

#include <string>

namespace epub {

void appendUtf8(uint32_t cp, std::string& out);
// Next code point of s starting at i (advances i). A byte that is not valid
// UTF-8 is taken as a Latin-1 character.
uint32_t nextCodepoint(const std::string& s, size_t& i);

// Whitespace for line breaking (not the no-break spaces).
bool isSpace(uint32_t cp);
// Appends cp in a form the Latin-1 fonts can show: typographic quotes and
// dashes become ASCII, accented letters outside Latin-1 lose the accent,
// invisible characters vanish, anything else becomes '?'.
void appendFolded(uint32_t cp, std::string& out);
// Whole string, with whitespace runs collapsed and trimmed.
std::string foldText(const std::string& s);

// "OEBPS/text/ch1.xhtml" -> "OEBPS/text/"
std::string dirOf(const std::string& path);
// Resolve an href against a directory: percent-decoding, "./" and "../",
// query and fragment dropped. Absolute URLs (with a scheme) give "".
std::string resolvePath(const std::string& dir, const std::string& href);
// Fragment of an href ("" when there is none).
std::string fragmentOf(const std::string& href);

}  // namespace epub
