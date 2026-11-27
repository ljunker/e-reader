#pragma once
#include <Arduino.h>

#include <functional>
#include <vector>

#include "Opds.h"

// The online library: OPDS catalogs listed in /opds.txt (a Calibre content
// server, Project Gutenberg ...), browsed and downloaded into /books.
namespace catalog {

// Reads /opds.txt; false with `error` set if it is missing or lists nothing.
bool readList(std::vector<opds::Catalog>& out, String& error);
bool configured();   // /opds.txt exists

// Credentials of this catalog are used from now on.
void use(const opds::Catalog& c);

// Loads one page of a feed.
bool load(const std::string& url, opds::Feed& out, String& error);

// Where a book from the catalog is stored: /books/<Title - Author>.epub
String pathFor(const opds::Entry& e);
bool onCard(const opds::Entry& e);

// Downloads the book into pathFor(e), replacing a file of the same name.
// `progress` gets the bytes so far and the total (-1 if unknown).
using Progress = std::function<void(uint32_t done, int32_t total)>;
bool download(const opds::Entry& e, String& path, String& error, const Progress& progress);

}  // namespace catalog
