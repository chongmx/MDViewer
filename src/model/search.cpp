#include "mdv/model/search.h"

#include <cctype>

namespace mdv::model {

namespace {

char fold(char c) { return char(std::tolower(static_cast<unsigned char>(c))); }

void visitInlines(const Inlines& inlines, const std::function<void(std::string_view)>& visit)
{
    for (const Span& span : inlines)
        if (span.kind == SpanKind::Text)
            visit(span.text);
}

} // namespace

std::vector<std::size_t> findAll(std::string_view haystack, std::string_view needle)
{
    std::vector<std::size_t> hits;
    if (needle.empty() || needle.size() > haystack.size())
        return hits;
    for (std::size_t i = 0; i + needle.size() <= haystack.size();) {
        std::size_t k = 0;
        while (k < needle.size() && fold(haystack[i + k]) == fold(needle[k]))
            ++k;
        if (k == needle.size()) {
            hits.push_back(i);
            i += needle.size();
        } else {
            ++i;
        }
    }
    return hits;
}

void forEachSearchableText(const Block& block, const std::function<void(std::string_view)>& visit)
{
    switch (block.type) {
    case BlockType::Paragraph:
    case BlockType::Heading:
        visitInlines(block.inlines, visit);
        break;
    case BlockType::CodeBlock:
        visit(block.code);
        break;
    case BlockType::Table:
        for (const auto& row : block.rows)
            for (const Inlines& cell : row)
                visitInlines(cell, visit);
        break;
    default:
        break;
    }
    for (const Block& child : block.children)
        forEachSearchableText(child, visit);
}

int countMatches(const Block& block, std::string_view needle)
{
    int count = 0;
    forEachSearchableText(block, [&](std::string_view text) { count += int(findAll(text, needle).size()); });
    return count;
}

int countMatches(const Document& doc, std::string_view needle)
{
    int count = 0;
    for (const Block& block : doc.blocks)
        count += countMatches(block, needle);
    return count;
}

} // namespace mdv::model
