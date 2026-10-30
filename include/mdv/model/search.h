// Case-insensitive text search over a document.
//
// Match numbering contract: matches are counted in document order by visiting blocks
// depth-first and, inside each block, the searchable strings in the order produced by
// forEachSearchableText(). Renderers that highlight matches must walk the same strings in
// the same order so that "match #n" refers to the same occurrence everywhere.
#pragma once

#include "mdv/model/document.h"

#include <functional>
#include <string_view>
#include <vector>

namespace mdv::model {

// Byte offsets of non-overlapping, ASCII-case-insensitive occurrences of `needle`.
std::vector<std::size_t> findAll(std::string_view haystack, std::string_view needle);

// Visits searchable strings of one block (recursing into children).
void forEachSearchableText(const Block& block, const std::function<void(std::string_view)>& visit);

int countMatches(const Block& block, std::string_view needle);
int countMatches(const Document& doc, std::string_view needle);

} // namespace mdv::model
