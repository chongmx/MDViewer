// Markdown document model: a block tree whose leaves are flat runs of styled inline spans.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mdv::model {

enum SpanStyle : std::uint8_t {
    StyleNone = 0,
    StyleBold = 1 << 0,
    StyleItalic = 1 << 1,
    StyleStrike = 1 << 2,
    StyleCode = 1 << 3,
};

enum class SpanKind : std::uint8_t {
    Text,
    SoftBreak, // newline inside a paragraph (rendered as a space)
    HardBreak, // forced line break
    Image,     // `text` holds the alt text, `link` the image URL
};

struct Span {
    SpanKind kind = SpanKind::Text;
    std::uint8_t style = StyleNone;
    int link = -1; // index into Document::links, or -1
    std::string text;
};

using Inlines = std::vector<Span>;

struct Link {
    std::string url;
    std::string title;
};

enum class BlockType {
    Paragraph,
    Heading,
    CodeBlock,
    BlockQuote,
    List,
    ListItem,
    ThematicBreak,
    Table,
};

enum class Align : std::uint8_t { None, Left, Center, Right };

struct Block {
    BlockType type = BlockType::Paragraph;

    Inlines inlines;            // Paragraph, Heading
    int level = 0;              // Heading: 1..6
    std::string anchor;         // Heading: GitHub-style slug used for "#fragment" links

    std::string code;           // CodeBlock
    std::string language;       // CodeBlock info string

    std::vector<Block> children; // BlockQuote, List (ListItems), ListItem

    bool ordered = false;       // List
    int start = 1;              // List: first number of an ordered list
    bool tight = true;          // List: no blank lines between items
    int task = -1;              // ListItem: -1 = not a task, 0 = [ ], 1 = [x]

    std::vector<Align> aligns;          // Table: per-column alignment
    std::vector<std::vector<Inlines>> rows; // Table: rows[0] is the header row

    // Parser scratch: raw inline source, converted to `inlines` once link references are known.
    std::string raw;
    std::vector<std::vector<std::string>> rawRows;
};

struct HeadingRef {
    int level = 0;
    std::string text;
    std::string anchor;
};

struct Document {
    std::string location; // file path or URL it was loaded from
    std::string title;
    std::vector<Block> blocks;
    std::vector<Link> links;
    std::vector<HeadingRef> headings; // document order, for the table of contents
    std::size_t sourceBytes = 0;
};

using DocumentPtr = std::shared_ptr<const Document>;

// Concatenated text of a span run (images contribute their alt text).
std::string plainText(const Inlines& inlines);

} // namespace mdv::model
