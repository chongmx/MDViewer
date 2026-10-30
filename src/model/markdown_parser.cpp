// Markdown parser.
//
// Pass 1 splits the source into a block tree (keeping raw inline text) and collects link
// reference definitions. Pass 2 parses inline content now that all references are known.
// Inline HTML is reduced to what a viewer needs: <br>, <img>, <a href> and comments;
// other tags are dropped while their text content is kept.
#include "mdv/model/markdown_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace mdv::model {

namespace {

using RefMap = std::map<std::string, Link>;

// ---- Character / string helpers --------------------------------------------------

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
bool isPunct(char c) { return std::ispunct(static_cast<unsigned char>(c)) != 0; }
bool isAlnum(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }
bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }

std::string_view trim(std::string_view s)
{
    size_t b = 0, e = s.size();
    while (b < e && isSpace(s[b]))
        ++b;
    while (e > b && isSpace(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

std::string lower(std::string_view s)
{
    std::string out(s);
    for (char& c : out)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool isBlank(std::string_view s)
{
    for (char c : s)
        if (!isSpace(c))
            return false;
    return true;
}

int leadingSpaces(std::string_view s)
{
    int n = 0;
    while (size_t(n) < s.size() && s[size_t(n)] == ' ')
        ++n;
    return n;
}

std::string expandTabs(std::string_view line)
{
    std::string out;
    out.reserve(line.size());
    for (char c : line) {
        if (c == '\t') {
            size_t spaces = 4 - (out.size() % 4);
            out.append(spaces, ' ');
        } else {
            out += c;
        }
    }
    return out;
}

void appendUtf8(std::string& out, unsigned cp)
{
    if (cp == 0 || cp > 0x10FFFF)
        cp = 0xFFFD;
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

// Reference labels match case-insensitively with internal whitespace collapsed.
std::string normalizeLabel(std::string_view label)
{
    std::string out;
    bool space = false;
    for (char c : trim(label)) {
        if (isSpace(c)) {
            space = true;
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// ---- Block-level recognisers ---------------------------------------------------------

bool isThematicBreak(std::string_view line)
{
    if (leadingSpaces(line) > 3)
        return false;
    char marker = 0;
    int count = 0;
    for (char c : line) {
        if (c == ' ')
            continue;
        if (c != '*' && c != '-' && c != '_')
            return false;
        if (marker && c != marker)
            return false;
        marker = c;
        ++count;
    }
    return count >= 3;
}

int atxHeading(std::string_view line, std::string_view& content)
{
    int indent = leadingSpaces(line);
    if (indent > 3)
        return 0;
    std::string_view s = line.substr(size_t(indent));
    int level = 0;
    while (size_t(level) < s.size() && s[size_t(level)] == '#')
        ++level;
    if (level < 1 || level > 6)
        return 0;
    if (size_t(level) < s.size() && s[size_t(level)] != ' ')
        return 0;
    std::string_view rest = trim(s.substr(size_t(level)));
    // Optional closing sequence: " ###"
    size_t end = rest.size();
    while (end > 0 && rest[end - 1] == '#')
        --end;
    if (end == 0)
        rest = {};
    else if (end < rest.size() && rest[end - 1] == ' ')
        rest = trim(rest.substr(0, end));
    content = rest;
    return level;
}

struct Fence {
    char ch = 0;
    int length = 0;
    int indent = 0;
    std::string info;
};

bool fenceOpen(std::string_view line, Fence& fence)
{
    int indent = leadingSpaces(line);
    if (indent > 3)
        return false;
    std::string_view s = line.substr(size_t(indent));
    if (s.empty() || (s[0] != '`' && s[0] != '~'))
        return false;
    char ch = s[0];
    int n = 0;
    while (size_t(n) < s.size() && s[size_t(n)] == ch)
        ++n;
    if (n < 3)
        return false;
    std::string_view info = trim(s.substr(size_t(n)));
    if (ch == '`' && info.find('`') != std::string_view::npos)
        return false;
    fence.ch = ch;
    fence.length = n;
    fence.indent = indent;
    size_t sp = info.find(' ');
    fence.info = std::string(sp == std::string_view::npos ? info : info.substr(0, sp));
    return true;
}

bool fenceClose(std::string_view line, const Fence& fence)
{
    int indent = leadingSpaces(line);
    if (indent > 3)
        return false;
    std::string_view s = line.substr(size_t(indent));
    int n = 0;
    while (size_t(n) < s.size() && s[size_t(n)] == fence.ch)
        ++n;
    return n >= fence.length && isBlank(s.substr(size_t(n)));
}

struct ListMarker {
    bool ordered = false;
    char ch = 0;         // bullet character, or '.' / ')' for ordered lists
    int number = 1;
    int contentIndent = 0; // column where item content starts
    bool emptyContent = false;
};

bool listMarker(std::string_view line, ListMarker& m)
{
    int indent = leadingSpaces(line);
    if (indent > 3)
        return false;
    size_t i = size_t(indent);
    if (i >= line.size())
        return false;
    if (line[i] == '-' || line[i] == '+' || line[i] == '*') {
        m.ordered = false;
        m.ch = line[i];
        ++i;
    } else if (isDigit(line[i])) {
        size_t start = i;
        while (i < line.size() && isDigit(line[i]) && i - start < 9)
            ++i;
        if (i >= line.size() || (line[i] != '.' && line[i] != ')'))
            return false;
        m.ordered = true;
        m.number = std::atoi(std::string(line.substr(start, i - start)).c_str());
        m.ch = line[i];
        ++i;
    } else {
        return false;
    }
    if (i < line.size() && line[i] != ' ')
        return false;
    int spaces = 0;
    while (i + size_t(spaces) < line.size() && line[i + size_t(spaces)] == ' ')
        ++spaces;
    m.emptyContent = i + size_t(spaces) >= line.size();
    if (m.emptyContent || spaces > 4)
        spaces = 1;
    m.contentIndent = int(i) + spaces;
    return true;
}

bool isBlockQuoteLine(std::string_view line)
{
    int indent = leadingSpaces(line);
    return indent <= 3 && size_t(indent) < line.size() && line[size_t(indent)] == '>';
}

bool isHtmlBlockStart(std::string_view line)
{
    int indent = leadingSpaces(line);
    if (indent > 3)
        return false;
    std::string_view s = line.substr(size_t(indent));
    return s.size() >= 2 && s[0] == '<' && (isAlpha(s[1]) || s[1] == '/' || s[1] == '!');
}

int setextLevel(std::string_view line)
{
    if (leadingSpaces(line) > 3)
        return 0;
    std::string_view s = trim(line);
    if (s.empty() || (s[0] != '=' && s[0] != '-'))
        return 0;
    for (char c : s)
        if (c != s[0])
            return 0;
    return s[0] == '=' ? 1 : 2;
}

std::vector<std::string> splitTableRow(std::string_view line)
{
    std::string_view s = trim(line);
    if (!s.empty() && s.front() == '|')
        s.remove_prefix(1);
    if (!s.empty() && s.back() == '|' && (s.size() < 2 || s[s.size() - 2] != '\\'))
        s.remove_suffix(1);
    std::vector<std::string> cells;
    std::string cell;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '|') {
            cell += '|';
            ++i;
        } else if (s[i] == '|') {
            cells.emplace_back(trim(cell));
            cell.clear();
        } else {
            cell += s[i];
        }
    }
    cells.emplace_back(trim(cell));
    return cells;
}

bool tableDelimiterRow(std::string_view line, std::vector<Align>& aligns)
{
    if (line.find('-') == std::string_view::npos || leadingSpaces(line) > 3)
        return false;
    aligns.clear();
    for (const std::string& cell : splitTableRow(line)) {
        std::string_view c = trim(cell);
        if (c.empty())
            return false;
        bool left = c.front() == ':';
        bool right = c.back() == ':';
        std::string_view dashes = c.substr(left ? 1 : 0, c.size() - (left ? 1 : 0) - (right ? 1 : 0));
        if (dashes.empty())
            return false;
        for (char d : dashes)
            if (d != '-')
                return false;
        aligns.push_back(left && right ? Align::Center : right ? Align::Right : left ? Align::Left : Align::None);
    }
    return !aligns.empty();
}

// [label]: destination "optional title"
bool linkReferenceDefinition(std::string_view line, RefMap& refs)
{
    std::string_view s = trim(line);
    if (s.size() < 4 || s[0] != '[')
        return false;
    size_t close = s.find("]:");
    if (close == std::string_view::npos || close < 2)
        return false;
    std::string label = normalizeLabel(s.substr(1, close - 1));
    std::string_view rest = trim(s.substr(close + 2));
    if (label.empty() || rest.empty())
        return false;
    std::string url;
    if (rest[0] == '<') {
        size_t gt = rest.find('>');
        if (gt == std::string_view::npos)
            return false;
        url = std::string(rest.substr(1, gt - 1));
        rest = trim(rest.substr(gt + 1));
    } else {
        size_t sp = 0;
        while (sp < rest.size() && !isSpace(rest[sp]))
            ++sp;
        url = std::string(rest.substr(0, sp));
        rest = trim(rest.substr(sp));
    }
    std::string title;
    if (!rest.empty()) {
        char open = rest[0];
        char closeCh = open == '(' ? ')' : open;
        if ((open != '"' && open != '\'' && open != '(') || rest.size() < 2 || rest.back() != closeCh)
            return false;
        title = std::string(rest.substr(1, rest.size() - 2));
    }
    refs.emplace(label, Link{url, title}); // first definition wins
    return true;
}

// ---- Block parser --------------------------------------------------------------------

class BlockParser {
public:
    explicit BlockParser(RefMap& refs) : refs_(refs) {}

    void parse(const std::vector<std::string>& lines, std::vector<Block>& out)
    {
        size_t i = 0;
        const size_t n = lines.size();
        while (i < n) {
            const std::string& line = lines[i];
            if (isBlank(line)) {
                ++i;
                continue;
            }
            if (leadingSpaces(line) >= 4) {
                i = parseIndentedCode(lines, i, out);
                continue;
            }
            Fence fence;
            if (fenceOpen(line, fence)) {
                i = parseFencedCode(lines, i, fence, out);
                continue;
            }
            std::string_view headingText;
            if (int level = atxHeading(line, headingText)) {
                Block b;
                b.type = BlockType::Heading;
                b.level = level;
                b.raw = std::string(headingText);
                out.push_back(std::move(b));
                ++i;
                continue;
            }
            if (isThematicBreak(line)) {
                Block b;
                b.type = BlockType::ThematicBreak;
                out.push_back(std::move(b));
                ++i;
                continue;
            }
            if (isBlockQuoteLine(line)) {
                i = parseBlockQuote(lines, i, out);
                continue;
            }
            ListMarker marker;
            if (listMarker(line, marker)) {
                i = parseList(lines, i, marker, out);
                continue;
            }
            if (isHtmlBlockStart(line)) {
                i = parseHtmlBlock(lines, i, out);
                continue;
            }
            std::vector<Align> aligns;
            if (line.find('|') != std::string::npos && i + 1 < n && tableDelimiterRow(lines[i + 1], aligns) &&
                splitTableRow(line).size() == aligns.size()) {
                i = parseTable(lines, i, std::move(aligns), out);
                continue;
            }
            i = parseParagraph(lines, i, out);
        }
    }

private:
    static bool interruptsParagraph(std::string_view line)
    {
        Fence fence;
        std::string_view heading;
        ListMarker m;
        if (fenceOpen(line, fence) || atxHeading(line, heading) || isThematicBreak(line) || isBlockQuoteLine(line))
            return true;
        if (listMarker(line, m) && !m.emptyContent && (!m.ordered || m.number == 1))
            return true;
        return false;
    }

    size_t parseIndentedCode(const std::vector<std::string>& lines, size_t i, std::vector<Block>& out)
    {
        std::vector<std::string> buf;
        while (i < lines.size() && (isBlank(lines[i]) || leadingSpaces(lines[i]) >= 4)) {
            buf.push_back(isBlank(lines[i]) ? std::string() : lines[i].substr(4));
            ++i;
        }
        while (!buf.empty() && buf.back().empty())
            buf.pop_back();
        Block b;
        b.type = BlockType::CodeBlock;
        for (size_t k = 0; k < buf.size(); ++k)
            b.code += (k ? "\n" : "") + buf[k];
        out.push_back(std::move(b));
        return i;
    }

    size_t parseFencedCode(const std::vector<std::string>& lines, size_t i, const Fence& fence, std::vector<Block>& out)
    {
        Block b;
        b.type = BlockType::CodeBlock;
        b.language = fence.info;
        ++i;
        bool first = true;
        while (i < lines.size() && !fenceClose(lines[i], fence)) {
            std::string_view l = lines[i];
            int strip = std::min(leadingSpaces(l), fence.indent);
            if (!first)
                b.code += '\n';
            b.code.append(l.substr(size_t(strip)));
            first = false;
            ++i;
        }
        out.push_back(std::move(b));
        return i < lines.size() ? i + 1 : i; // skip closing fence
    }

    size_t parseBlockQuote(const std::vector<std::string>& lines, size_t i, std::vector<Block>& out)
    {
        std::vector<std::string> inner;
        bool lastWasText = false;
        while (i < lines.size()) {
            const std::string& l = lines[i];
            if (isBlockQuoteLine(l)) {
                size_t p = size_t(leadingSpaces(l)) + 1;
                if (p < l.size() && l[p] == ' ')
                    ++p;
                inner.push_back(l.substr(p));
                lastWasText = !isBlank(inner.back());
            } else if (lastWasText && !isBlank(l) && !interruptsParagraph(l)) {
                inner.push_back(l); // lazy continuation
            } else {
                break;
            }
            ++i;
        }
        Block b;
        b.type = BlockType::BlockQuote;
        parse(inner, b.children);
        out.push_back(std::move(b));
        return i;
    }

    size_t parseList(const std::vector<std::string>& lines, size_t i, const ListMarker& first, std::vector<Block>& out)
    {
        Block list;
        list.type = BlockType::List;
        list.ordered = first.ordered;
        list.start = first.number;

        while (i < lines.size()) {
            ListMarker m;
            if (isThematicBreak(lines[i]) || !listMarker(lines[i], m) || m.ordered != first.ordered || m.ch != first.ch)
                break;

            const int width = m.contentIndent;
            std::vector<std::string> itemLines;
            const std::string& head = lines[i];
            itemLines.push_back(size_t(width) < head.size() ? head.substr(size_t(width)) : std::string());

            size_t j = i + 1;
            bool lastBlank = m.emptyContent;
            bool inFence = false;
            Fence fence;
            if (fenceOpen(itemLines[0], fence))
                inFence = true;
            std::vector<size_t> blanksOutsideFences;
            while (j < lines.size()) {
                const std::string& l = lines[j];
                if (isBlank(l)) {
                    if (!inFence)
                        blanksOutsideFences.push_back(itemLines.size());
                    itemLines.emplace_back();
                    lastBlank = true;
                    ++j;
                    continue;
                }
                int indent = leadingSpaces(l);
                if (indent >= width) {
                    std::string content = l.substr(size_t(width));
                    if (inFence ? fenceClose(content, fence) : fenceOpen(content, fence))
                        inFence = !inFence;
                    itemLines.push_back(std::move(content));
                    lastBlank = false;
                    ++j;
                    continue;
                }
                ListMarker next;
                if (!lastBlank && !inFence && !listMarker(l, next) && !interruptsParagraph(l)) {
                    itemLines.push_back(l.substr(size_t(indent))); // lazy continuation
                    ++j;
                    continue;
                }
                break;
            }

            size_t trailingBlanks = 0;
            while (!itemLines.empty() && isBlank(itemLines.back())) {
                itemLines.pop_back();
                ++trailingBlanks;
            }

            Block item;
            item.type = BlockType::ListItem;
            if (!itemLines.empty()) {
                std::string& c = itemLines[0];
                if (c.size() >= 3 && c[0] == '[' && c[2] == ']' && (c[1] == ' ' || c[1] == 'x' || c[1] == 'X') &&
                    (c.size() == 3 || c[3] == ' ')) {
                    item.task = c[1] == ' ' ? 0 : 1;
                    c = c.size() > 4 ? c.substr(4) : std::string();
                }
            }
            for (size_t idx : blanksOutsideFences)
                if (idx < itemLines.size())
                    list.tight = false; // blank line between blocks inside an item

            parse(itemLines, item.children);
            list.children.push_back(std::move(item));
            i = j;

            if (trailingBlanks > 0) {
                ListMarker next;
                if (i < lines.size() && listMarker(lines[i], next) && next.ordered == first.ordered && next.ch == first.ch &&
                    !isThematicBreak(lines[i]))
                    list.tight = false; // blank line between items
            }
        }
        out.push_back(std::move(list));
        return i;
    }

    size_t parseHtmlBlock(const std::vector<std::string>& lines, size_t i, std::vector<Block>& out)
    {
        Block b;
        b.type = BlockType::Paragraph;
        bool inComment = false;
        while (i < lines.size() && (inComment || !isBlank(lines[i]))) {
            const std::string& l = lines[i];
            if (!b.raw.empty())
                b.raw += '\n';
            b.raw += l;
            if (l.find("<!--") != std::string::npos)
                inComment = true;
            if (l.find("-->") != std::string::npos)
                inComment = false;
            ++i;
        }
        out.push_back(std::move(b)); // tags are reduced during inline parsing
        return i;
    }

    size_t parseTable(const std::vector<std::string>& lines, size_t i, std::vector<Align> aligns, std::vector<Block>& out)
    {
        Block b;
        b.type = BlockType::Table;
        b.aligns = std::move(aligns);
        b.rawRows.push_back(splitTableRow(lines[i]));
        i += 2;
        while (i < lines.size() && !isBlank(lines[i]) && !interruptsParagraph(lines[i])) {
            std::vector<std::string> row = splitTableRow(lines[i]);
            row.resize(b.aligns.size());
            b.rawRows.push_back(std::move(row));
            ++i;
        }
        out.push_back(std::move(b));
        return i;
    }

    size_t parseParagraph(const std::vector<std::string>& lines, size_t i, std::vector<Block>& out)
    {
        std::vector<std::string_view> para;
        para.emplace_back(lines[i]);
        size_t j = i + 1;
        int setext = 0;
        while (j < lines.size() && !isBlank(lines[j])) {
            if (int level = setextLevel(lines[j])) {
                setext = level;
                ++j;
                break;
            }
            if (interruptsParagraph(lines[j]) || isHtmlBlockStart(lines[j]))
                break;
            para.emplace_back(lines[j]);
            ++j;
        }

        // Leading link reference definitions are consumed and produce no output.
        size_t first = 0;
        while (first < para.size() && linkReferenceDefinition(para[first], refs_))
            ++first;
        if (first == para.size()) {
            if (setext == 2) { // "---" after only definitions is a thematic break
                Block hr;
                hr.type = BlockType::ThematicBreak;
                out.push_back(std::move(hr));
            }
            return j;
        }

        Block b;
        b.type = setext ? BlockType::Heading : BlockType::Paragraph;
        b.level = setext;
        for (size_t k = first; k < para.size(); ++k) {
            std::string_view l = para[k];
            l.remove_prefix(size_t(leadingSpaces(l)));
            if (k + 1 == para.size())
                l = trim(l);
            if (k > first)
                b.raw += '\n';
            b.raw.append(l);
        }
        out.push_back(std::move(b));
        return j;
    }

    RefMap& refs_;
};

// ---- Inline parser -------------------------------------------------------------------

struct ParsedLink {
    std::string_view label;
    std::string url;
    std::string title;
    size_t end = 0; // index just past the construct
};

const std::map<std::string, const char*>& entityTable()
{
    static const std::map<std::string, const char*> table = {
        {"amp", "&"},         {"lt", "<"},          {"gt", ">"},           {"quot", "\""},
        {"apos", "'"},        {"nbsp", "\xC2\xA0"}, {"copy", "\xC2\xA9"},  {"reg", "\xC2\xAE"},
        {"trade", "\xE2\x84\xA2"}, {"hellip", "\xE2\x80\xA6"}, {"mdash", "\xE2\x80\x94"}, {"ndash", "\xE2\x80\x93"},
        {"laquo", "\xC2\xAB"}, {"raquo", "\xC2\xBB"}, {"larr", "\xE2\x86\x90"}, {"rarr", "\xE2\x86\x92"},
        {"uarr", "\xE2\x86\x91"}, {"darr", "\xE2\x86\x93"}, {"times", "\xC3\x97"}, {"deg", "\xC2\xB0"},
        {"middot", "\xC2\xB7"}, {"bull", "\xE2\x80\xA2"}, {"lsquo", "\xE2\x80\x98"}, {"rsquo", "\xE2\x80\x99"},
        {"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"}, {"euro", "\xE2\x82\xAC"}, {"pound", "\xC2\xA3"},
    };
    return table;
}

std::string htmlAttribute(std::string_view tag, std::string_view name)
{
    std::string lowerTag = lower(tag);
    size_t pos = 0;
    while ((pos = lowerTag.find(name, pos)) != std::string::npos) {
        bool boundary = pos > 0 && isSpace(lowerTag[pos - 1]);
        size_t p = pos + name.size();
        while (p < tag.size() && isSpace(tag[p]))
            ++p;
        if (!boundary || p >= tag.size() || tag[p] != '=') {
            pos += name.size();
            continue;
        }
        ++p;
        while (p < tag.size() && isSpace(tag[p]))
            ++p;
        if (p < tag.size() && (tag[p] == '"' || tag[p] == '\'')) {
            char q = tag[p];
            size_t end = tag.find(q, p + 1);
            return std::string(tag.substr(p + 1, end == std::string_view::npos ? std::string_view::npos : end - p - 1));
        }
        size_t end = p;
        while (end < tag.size() && !isSpace(tag[end]) && tag[end] != '>')
            ++end;
        return std::string(tag.substr(p, end - p));
    }
    return {};
}

class InlineParser {
public:
    InlineParser(const RefMap& refs, std::vector<Link>& links) : refs_(refs), links_(links) {}

    Inlines parse(std::string_view text)
    {
        Inlines out;
        parseRange(text, StyleNone, -1, out);
        // Drop breaks at the very start / end (e.g. left over from stripped HTML tags).
        while (!out.empty() && out.front().kind != SpanKind::Text && out.front().kind != SpanKind::Image)
            out.erase(out.begin());
        while (!out.empty() && out.back().kind != SpanKind::Text && out.back().kind != SpanKind::Image)
            out.pop_back();
        return out;
    }

private:
    int addLink(std::string url, std::string title)
    {
        links_.push_back({std::move(url), std::move(title)});
        return int(links_.size()) - 1;
    }

    static void emit(Inlines& out, SpanKind kind, std::string_view text, std::uint8_t style, int link)
    {
        if (kind == SpanKind::Text && text.empty())
            return;
        if (kind == SpanKind::Text && !out.empty()) {
            Span& last = out.back();
            if (last.kind == SpanKind::Text && last.style == style && last.link == link) {
                last.text.append(text);
                return;
            }
        }
        Span s;
        s.kind = kind;
        s.style = style;
        s.link = link;
        s.text = std::string(text);
        out.push_back(std::move(s));
    }

    // Returns the index just past a code span starting at `i`, or npos if unterminated.
    static size_t skipCodeSpan(std::string_view s, size_t i, size_t* contentBegin = nullptr, size_t* contentEnd = nullptr)
    {
        size_t run = 0;
        while (i + run < s.size() && s[i + run] == '`')
            ++run;
        size_t j = i + run;
        while (j < s.size()) {
            if (s[j] != '`') {
                ++j;
                continue;
            }
            size_t r = 0;
            while (j + r < s.size() && s[j + r] == '`')
                ++r;
            if (r == run) {
                if (contentBegin)
                    *contentBegin = i + run;
                if (contentEnd)
                    *contentEnd = j;
                return j + r;
            }
            j += r;
        }
        return std::string_view::npos;
    }

    // Finds the ']' matching the '[' at `open`, honouring nesting, escapes and code spans.
    static size_t matchingBracket(std::string_view s, size_t open)
    {
        int depth = 0;
        for (size_t i = open; i < s.size(); ++i) {
            char c = s[i];
            if (c == '\\') {
                ++i;
            } else if (c == '`') {
                size_t end = skipCodeSpan(s, i);
                if (end != std::string_view::npos)
                    i = end - 1;
            } else if (c == '[') {
                ++depth;
            } else if (c == ']') {
                if (--depth == 0)
                    return i;
            }
        }
        return std::string_view::npos;
    }

    bool parseLink(std::string_view s, size_t open, ParsedLink& out) const
    {
        size_t close = matchingBracket(s, open);
        if (close == std::string_view::npos)
            return false;
        out.label = s.substr(open + 1, close - open - 1);
        size_t p = close + 1;

        if (p < s.size() && s[p] == '(') {
            ++p;
            while (p < s.size() && isSpace(s[p]))
                ++p;
            std::string url;
            if (p < s.size() && s[p] == '<') {
                size_t gt = s.find('>', p);
                if (gt == std::string_view::npos)
                    return false;
                url = std::string(s.substr(p + 1, gt - p - 1));
                p = gt + 1;
            } else {
                int parens = 0;
                size_t start = p;
                while (p < s.size() && !isSpace(s[p])) {
                    if (s[p] == '\\' && p + 1 < s.size()) {
                        p += 2;
                        continue;
                    }
                    if (s[p] == '(')
                        ++parens;
                    if (s[p] == ')') {
                        if (parens == 0)
                            break;
                        --parens;
                    }
                    ++p;
                }
                url = std::string(s.substr(start, p - start));
            }
            while (p < s.size() && isSpace(s[p]))
                ++p;
            std::string title;
            if (p < s.size() && (s[p] == '"' || s[p] == '\'' || s[p] == '(')) {
                char closeCh = s[p] == '(' ? ')' : s[p];
                size_t end = s.find(closeCh, p + 1);
                if (end == std::string_view::npos)
                    return false;
                title = std::string(s.substr(p + 1, end - p - 1));
                p = end + 1;
                while (p < s.size() && isSpace(s[p]))
                    ++p;
            }
            if (p >= s.size() || s[p] != ')')
                return false;
            out.url = std::move(url);
            out.title = std::move(title);
            out.end = p + 1;
            return true;
        }

        std::string key;
        size_t end = close + 1;
        if (p < s.size() && s[p] == '[') {
            size_t refClose = s.find(']', p);
            if (refClose == std::string_view::npos)
                return false;
            key = normalizeLabel(s.substr(p + 1, refClose - p - 1));
            end = refClose + 1;
        }
        if (key.empty())
            key = normalizeLabel(out.label);
        auto it = refs_.find(key);
        if (it == refs_.end())
            return false;
        out.url = it->second.url;
        out.title = it->second.title;
        out.end = end;
        return true;
    }

    static bool leftFlanking(char prev, char next)
    {
        return !isSpace(next) && (!isPunct(next) || isSpace(prev) || isPunct(prev));
    }
    static bool rightFlanking(char prev, char next)
    {
        return !isSpace(prev) && (!isPunct(prev) || isSpace(next) || isPunct(next));
    }

    // Looks for the closing delimiter run for an emphasis opener; returns its position and length.
    static bool findEmphasisCloser(std::string_view s, size_t from, char d, size_t& closerPos, size_t& closerLen)
    {
        int depth = 0;
        size_t j = from;
        while (j < s.size()) {
            char c = s[j];
            if (c == '\\') {
                j += 2;
                continue;
            }
            if (c == '`') {
                size_t end = skipCodeSpan(s, j);
                if (end != std::string_view::npos) {
                    j = end;
                    continue;
                }
            }
            if (c != d) {
                ++j;
                continue;
            }
            size_t r = 0;
            while (j + r < s.size() && s[j + r] == d)
                ++r;
            char prev = j > 0 ? s[j - 1] : ' ';
            char next = j + r < s.size() ? s[j + r] : ' ';
            bool lf = leftFlanking(prev, next);
            bool rf = rightFlanking(prev, next);
            bool closes = rf && (d == '*' || !lf || isPunct(next));
            bool opens = lf && (d == '*' || !rf || isPunct(prev));
            if (closes) {
                if (depth == 0) {
                    closerPos = j;
                    closerLen = r;
                    return true;
                }
                --depth;
            } else if (opens) {
                ++depth;
            }
            j += r;
        }
        return false;
    }

    static bool decodeEntity(std::string_view s, size_t i, std::string& out, size_t& end)
    {
        size_t semi = s.find(';', i);
        if (semi == std::string_view::npos || semi - i > 10 || semi == i + 1)
            return false;
        std::string_view name = s.substr(i + 1, semi - i - 1);
        if (name[0] == '#') {
            unsigned cp = 0;
            bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
            std::string digits(name.substr(hex ? 2 : 1));
            if (digits.empty())
                return false;
            char* parseEnd = nullptr;
            cp = unsigned(std::strtoul(digits.c_str(), &parseEnd, hex ? 16 : 10));
            if (*parseEnd != '\0')
                return false;
            appendUtf8(out, cp);
        } else {
            auto it = entityTable().find(std::string(name));
            if (it == entityTable().end())
                return false;
            out += it->second;
        }
        end = semi + 1;
        return true;
    }

    static bool startsBareUrl(std::string_view s, size_t i)
    {
        if (i > 0 && !isSpace(s[i - 1]) && s[i - 1] != '(' && s[i - 1] != '*' && s[i - 1] != '_')
            return false;
        std::string_view rest = s.substr(i);
        return rest.rfind("http://", 0) == 0 || rest.rfind("https://", 0) == 0 || rest.rfind("www.", 0) == 0;
    }

    void parseRange(std::string_view s, std::uint8_t style, int link, Inlines& out)
    {
        std::string buf;
        int activeLink = link; // may be changed by inline <a href> tags
        auto flush = [&]() {
            emit(out, SpanKind::Text, buf, style, activeLink);
            buf.clear();
        };

        size_t i = 0;
        const size_t n = s.size();
        while (i < n) {
            const char c = s[i];
            switch (c) {
            case '\\':
                if (i + 1 < n && s[i + 1] == '\n') {
                    flush();
                    emit(out, SpanKind::HardBreak, {}, style, activeLink);
                    i += 2;
                    while (i < n && s[i] == ' ')
                        ++i;
                    continue;
                }
                if (i + 1 < n && isPunct(s[i + 1])) {
                    buf += s[i + 1];
                    i += 2;
                    continue;
                }
                buf += c;
                ++i;
                continue;

            case '`': {
                size_t b = 0, e = 0;
                size_t end = skipCodeSpan(s, i, &b, &e);
                if (end == std::string_view::npos) {
                    while (i < n && s[i] == '`')
                        buf += s[i++];
                    continue;
                }
                std::string code(s.substr(b, e - b));
                std::replace(code.begin(), code.end(), '\n', ' ');
                if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ' && !isBlank(code))
                    code = code.substr(1, code.size() - 2);
                flush();
                emit(out, SpanKind::Text, code, std::uint8_t(style | StyleCode), activeLink);
                i = end;
                continue;
            }

            case '!':
                if (i + 1 < n && s[i + 1] == '[') {
                    ParsedLink pl;
                    if (parseLink(s, i + 1, pl)) {
                        flush();
                        Inlines alt;
                        parseRange(pl.label, StyleNone, -1, alt);
                        emit(out, SpanKind::Image, plainText(alt), style, addLink(pl.url, pl.title));
                        // An image inside a link stays clickable as that link.
                        if (activeLink >= 0)
                            out.back().link = activeLink;
                        i = pl.end;
                        continue;
                    }
                }
                buf += c;
                ++i;
                continue;

            case '[': {
                ParsedLink pl;
                if (activeLink < 0 && parseLink(s, i, pl)) {
                    flush();
                    int index = addLink(pl.url, pl.title);
                    parseRange(pl.label, style, index, out);
                    i = pl.end;
                    continue;
                }
                buf += c;
                ++i;
                continue;
            }

            case '<': {
                if (s.substr(i, 4) == "<!--") {
                    size_t end = s.find("-->", i + 4);
                    i = end == std::string_view::npos ? n : end + 3;
                    continue;
                }
                size_t gt = s.find('>', i);
                if (gt == std::string_view::npos) {
                    buf += c;
                    ++i;
                    continue;
                }
                std::string_view inner = s.substr(i + 1, gt - i - 1);
                // Autolink: <https://example.com> or <user@example.com>
                if (!inner.empty() && inner.find(' ') == std::string_view::npos && inner.find('\n') == std::string_view::npos &&
                    (inner.find("://") != std::string_view::npos ||
                     (inner.find('@') != std::string_view::npos && inner.find('.') != std::string_view::npos && isAlnum(inner[0])))) {
                    flush();
                    std::string url(inner);
                    if (url.find("://") == std::string::npos && url.rfind("mailto:", 0) != 0)
                        url = "mailto:" + url;
                    emit(out, SpanKind::Text, inner, style, activeLink >= 0 ? activeLink : addLink(url, {}));
                    i = gt + 1;
                    continue;
                }
                // Inline HTML tag.
                bool closing = !inner.empty() && inner[0] == '/';
                std::string_view nameStart = inner.substr(closing ? 1 : 0);
                size_t nameLen = 0;
                while (nameLen < nameStart.size() && (isAlnum(nameStart[nameLen]) || nameStart[nameLen] == '-'))
                    ++nameLen;
                if (nameLen == 0 || !isAlpha(nameStart[0])) {
                    buf += c;
                    ++i;
                    continue;
                }
                std::string name = lower(nameStart.substr(0, nameLen));
                if (name == "br") {
                    flush();
                    emit(out, SpanKind::HardBreak, {}, style, activeLink);
                } else if (name == "img" && !closing) {
                    std::string src = htmlAttribute(inner, "src");
                    if (!src.empty()) {
                        flush();
                        emit(out, SpanKind::Image, htmlAttribute(inner, "alt"), style, addLink(src, htmlAttribute(inner, "title")));
                        if (activeLink >= 0)
                            out.back().link = activeLink;
                    }
                } else if (name == "a") {
                    flush();
                    if (closing) {
                        activeLink = link;
                    } else {
                        std::string href = htmlAttribute(inner, "href");
                        if (!href.empty())
                            activeLink = addLink(href, htmlAttribute(inner, "title"));
                    }
                } else if ((name == "p" || name == "div" || name == "li" || name == "tr") && closing) {
                    flush();
                    emit(out, SpanKind::HardBreak, {}, style, activeLink);
                }
                i = gt + 1;
                continue;
            }

            case '*':
            case '_': {
                size_t run = 0;
                while (i + run < n && s[i + run] == c)
                    ++run;
                char prev = i > 0 ? s[i - 1] : ' ';
                char next = i + run < n ? s[i + run] : ' ';
                bool lf = leftFlanking(prev, next);
                bool rf = rightFlanking(prev, next);
                bool canOpen = lf && (c == '*' || !rf || isPunct(prev));
                size_t closerPos = 0, closerLen = 0;
                if (canOpen && findEmphasisCloser(s, i + run, c, closerPos, closerLen) && closerPos > i + run) {
                    size_t k = std::min<size_t>(std::min(run, closerLen), 3);
                    const std::uint8_t add = k == 1   ? std::uint8_t(StyleItalic)
                                             : k == 2 ? std::uint8_t(StyleBold)
                                                      : std::uint8_t(StyleBold | StyleItalic);
                    buf.append(run - k, c);
                    flush();
                    parseRange(s.substr(i + run, closerPos - (i + run)), std::uint8_t(style | add), activeLink, out);
                    i = closerPos + k;
                    continue;
                }
                buf.append(run, c);
                i += run;
                continue;
            }

            case '~':
                if (i + 1 < n && s[i + 1] == '~') {
                    size_t close = s.find("~~", i + 2);
                    if (close != std::string_view::npos && close > i + 2) {
                        flush();
                        parseRange(s.substr(i + 2, close - i - 2), std::uint8_t(style | StyleStrike), activeLink, out);
                        i = close + 2;
                        continue;
                    }
                }
                buf += c;
                ++i;
                continue;

            case '\n': {
                size_t trailing = 0;
                while (!buf.empty() && buf.back() == ' ') {
                    buf.pop_back();
                    ++trailing;
                }
                flush();
                emit(out, trailing >= 2 ? SpanKind::HardBreak : SpanKind::SoftBreak, {}, style, activeLink);
                ++i;
                while (i < n && s[i] == ' ')
                    ++i;
                continue;
            }

            case '&': {
                size_t end = 0;
                if (decodeEntity(s, i, buf, end)) {
                    i = end;
                    continue;
                }
                buf += c;
                ++i;
                continue;
            }

            case 'h':
            case 'w':
                if (activeLink < 0 && startsBareUrl(s, i)) {
                    size_t end = i;
                    while (end < n && !isSpace(s[end]) && s[end] != '<')
                        ++end;
                    // Trim trailing punctuation and unbalanced closing parentheses.
                    while (end > i) {
                        char t = s[end - 1];
                        if (t == '.' || t == ',' || t == ':' || t == ';' || t == '!' || t == '?' || t == '"' ||
                            t == '\'' || t == '*' || t == '_' || t == '~') {
                            --end;
                            continue;
                        }
                        if (t == ')') {
                            std::string_view u = s.substr(i, end - i);
                            if (std::count(u.begin(), u.end(), '(') < std::count(u.begin(), u.end(), ')')) {
                                --end;
                                continue;
                            }
                        }
                        break;
                    }
                    std::string_view text = s.substr(i, end - i);
                    std::string url(text);
                    if (url.rfind("www.", 0) == 0)
                        url = "https://" + url;
                    if (text.size() > 4) {
                        flush();
                        emit(out, SpanKind::Text, text, style, addLink(url, {}));
                        i = end;
                        continue;
                    }
                }
                buf += c;
                ++i;
                continue;

            default:
                buf += c;
                ++i;
                continue;
            }
        }
        flush();
    }

    const RefMap& refs_;
    std::vector<Link>& links_;
};

// ---- Pass 2 ---------------------------------------------------------------------------

bool hasVisibleContent(const Inlines& inlines)
{
    for (const Span& s : inlines)
        if (s.kind == SpanKind::Image || (s.kind == SpanKind::Text && !isBlank(s.text)))
            return true;
    return false;
}

struct InlinePass {
    InlineParser& parser;
    Document& doc;
    std::map<std::string, int> slugCounts;

    void run(std::vector<Block>& blocks)
    {
        for (Block& b : blocks) {
            switch (b.type) {
            case BlockType::Paragraph:
            case BlockType::Heading:
                b.inlines = parser.parse(b.raw);
                b.raw.clear();
                if (b.type == BlockType::Heading) {
                    std::string text = plainText(b.inlines);
                    std::string slug = slugify(text);
                    int& count = slugCounts[slug];
                    b.anchor = count == 0 ? slug : slug + "-" + std::to_string(count);
                    ++count;
                    doc.headings.push_back({b.level, text, b.anchor});
                }
                break;
            case BlockType::Table:
                for (std::vector<std::string>& rawRow : b.rawRows) {
                    std::vector<Inlines> row;
                    for (const std::string& cell : rawRow)
                        row.push_back(parser.parse(cell));
                    b.rows.push_back(std::move(row));
                }
                b.rawRows.clear();
                break;
            default:
                break;
            }
            run(b.children);
        }
        // Remove paragraphs that became empty (e.g. HTML blocks made only of tags/comments).
        blocks.erase(std::remove_if(blocks.begin(), blocks.end(),
                                    [](const Block& b) {
                                        return b.type == BlockType::Paragraph && !hasVisibleContent(b.inlines);
                                    }),
                     blocks.end());
    }
};

} // namespace

std::string plainText(const Inlines& inlines)
{
    std::string out;
    for (const Span& s : inlines) {
        if (s.kind == SpanKind::Text || s.kind == SpanKind::Image)
            out += s.text;
        else
            out += ' ';
    }
    return out;
}

std::string slugify(std::string_view text)
{
    std::string out;
    for (char c : trim(text)) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u >= 0x80 || isAlnum(c) || c == '_' || c == '-')
            out += char(std::tolower(u));
        else if (c == ' ')
            out += '-';
    }
    return out;
}

Document parseMarkdown(std::string_view source)
{
    Document doc;
    doc.sourceBytes = source.size();

    // Strip a UTF-8 BOM, then split into tab-expanded lines.
    if (source.size() >= 3 && source.substr(0, 3) == "\xEF\xBB\xBF")
        source.remove_prefix(3);
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= source.size()) {
        size_t nl = source.find('\n', pos);
        std::string_view line = source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        lines.push_back(expandTabs(line));
        if (nl == std::string_view::npos)
            break;
        pos = nl + 1;
    }

    RefMap refs;
    BlockParser(refs).parse(lines, doc.blocks);

    InlineParser inlineParser(refs, doc.links);
    InlinePass{inlineParser, doc, {}}.run(doc.blocks);

    for (const HeadingRef& h : doc.headings) {
        if (h.level == 1) {
            doc.title = h.text;
            break;
        }
    }
    if (doc.title.empty() && !doc.headings.empty())
        doc.title = doc.headings.front().text;
    return doc;
}

} // namespace mdv::model
