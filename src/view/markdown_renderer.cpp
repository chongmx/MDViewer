#include "mdv/view/markdown_renderer.h"

#include "mdv/model/search.h"

#include "IconsFontAwesome6.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace mdv::view {

using model::Align;
using model::Block;
using model::BlockType;
using model::Inlines;
using model::Span;
using model::SpanKind;

namespace {

constexpr float kLineSpacing = 1.5f;
constexpr float kCodeScale = 0.88f;
constexpr float kItalicShear = 0.2f;
constexpr float kHeadingScale[6] = {2.0f, 1.5f, 1.25f, 1.0f, 0.875f, 0.85f};

// ---- Minimal, language-agnostic syntax highlighting -----------------------------------

enum TokenClass : std::uint8_t { TokPlain, TokKeyword, TokString, TokComment, TokNumber };

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

const std::unordered_set<std::string_view>& keywords()
{
    static const std::unordered_set<std::string_view> set = {
        "if", "else", "for", "while", "do", "return", "break", "continue", "switch", "case", "default", "goto",
        "class", "struct", "enum", "union", "namespace", "public", "private", "protected", "virtual", "override",
        "const", "constexpr", "static", "inline", "extern", "void", "int", "float", "double", "char", "bool",
        "auto", "true", "false", "null", "nullptr", "None", "True", "False", "def", "fn", "func", "function",
        "let", "var", "mut", "pub", "use", "mod", "impl", "trait", "import", "from", "as", "package", "yield",
        "async", "await", "try", "catch", "except", "finally", "raise", "throw", "new", "delete", "this", "self",
        "super", "typedef", "template", "typename", "sizeof", "unsigned", "signed", "long", "short", "in", "is",
        "not", "and", "or", "lambda", "with", "pass", "elif", "end", "then", "local", "module", "export",
        "extends", "implements", "interface", "type", "match", "loop", "where", "go", "defer", "chan", "select",
        "range", "string", "echo", "fi", "esac", "done", "include", "define", "using", "operator", "final",
        "noexcept", "volatile", "register", "const_cast", "static_cast", "dynamic_cast", "reinterpret_cast",
    };
    return set;
}

std::vector<std::uint8_t> highlight(std::string_view code, std::string_view language)
{
    std::vector<std::uint8_t> cls(code.size(), TokPlain);
    std::string lang;
    for (char c : language)
        lang += char(std::tolower(static_cast<unsigned char>(c)));
    if (lang.empty() || lang == "text" || lang == "txt" || lang == "plain" || lang == "md" || lang == "markdown")
        return cls;

    const bool hashComments = lang == "python" || lang == "py" || lang == "sh" || lang == "bash" || lang == "shell" ||
                              lang == "zsh" || lang == "ruby" || lang == "rb" || lang == "yaml" || lang == "yml" ||
                              lang == "toml" || lang == "cmake" || lang == "perl" || lang == "r" || lang == "ini" ||
                              lang == "dockerfile" || lang == "make" || lang == "makefile" || lang == "powershell" ||
                              lang == "ps1" || lang == "conf";
    const bool dashComments = lang == "sql" || lang == "lua" || lang == "haskell" || lang == "hs";
    const bool htmlComments = lang == "html" || lang == "xml" || lang == "svg";
    const bool slashComments = !hashComments && !dashComments && !htmlComments;

    size_t i = 0;
    const size_t n = code.size();
    auto mark = [&](size_t from, size_t to, std::uint8_t c) { std::fill(cls.begin() + from, cls.begin() + to, c); };
    while (i < n) {
        char c = code[i];
        std::string_view rest = code.substr(i);
        if ((slashComments && rest.rfind("//", 0) == 0) || (hashComments && c == '#') ||
            (dashComments && rest.rfind("--", 0) == 0)) {
            size_t end = code.find('\n', i);
            end = end == std::string_view::npos ? n : end;
            mark(i, end, TokComment);
            i = end;
        } else if ((slashComments && rest.rfind("/*", 0) == 0) || (htmlComments && rest.rfind("<!--", 0) == 0)) {
            std::string_view close = htmlComments ? "-->" : "*/";
            size_t end = code.find(close, i + 2);
            end = end == std::string_view::npos ? n : end + close.size();
            mark(i, end, TokComment);
            i = end;
        } else if (c == '"' || c == '\'' || c == '`') {
            size_t j = i + 1;
            while (j < n && code[j] != c && (code[j] != '\n' || c == '`')) {
                if (code[j] == '\\')
                    ++j;
                ++j;
            }
            j = std::min(n, j + 1);
            mark(i, j, TokString);
            i = j;
        } else if (std::isdigit(static_cast<unsigned char>(c)) && (i == 0 || !isIdentChar(code[i - 1]))) {
            size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(code[j])) || code[j] == '.' || code[j] == '_'))
                ++j;
            mark(i, j, TokNumber);
            i = j;
        } else if (isIdentStart(c)) {
            size_t j = i;
            while (j < n && isIdentChar(code[j]))
                ++j;
            if (keywords().count(code.substr(i, j - i)))
                mark(i, j, TokKeyword);
            i = j;
        } else {
            ++i;
        }
    }
    return cls;
}

const char* nextUtf8(const char* p, const char* end)
{
    ++p;
    while (p < end && (static_cast<unsigned char>(*p) & 0xC0) == 0x80)
        ++p;
    return p;
}

} // namespace

// ---- Public API -------------------------------------------------------------------------

std::optional<float> MarkdownRenderer::anchorY(const std::string& anchor) const
{
    auto it = anchors_.find(anchor);
    if (it == anchors_.end()) {
        std::string lower;
        for (char c : anchor)
            lower += char(std::tolower(static_cast<unsigned char>(c)));
        it = anchors_.find(lower);
    }
    if (it == anchors_.end())
        return std::nullopt;
    return it->second;
}

void MarkdownRenderer::render(const model::Document& doc, const Params& params)
{
    p_ = params;
    pal_ = params.palette;
    dl_ = ImGui::GetWindowDrawList();
    size_ = params.fontSize;
    textColor_ = pal_->text;
    tight_ = false;
    listDepth_ = 0;
    atTop_ = true;
    matchCounter_ = 0;
    currentMatchY_.reset();
    anchors_.clear();
    scratch_.clear();
    hoveredLinkPrev_ = hoveredLink_;
    hoveredLink_ = -1;
    clickedLink_ = -1;
    canHover_ = ImGui::IsWindowHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left);

    const float avail = ImGui::GetContentRegionAvail().x;
    const float width = std::min(avail, p_.maxWidth);
    const float indent = std::floor((avail - width) * 0.5f);

    if (cacheDoc_ != &doc || std::fabs(cacheWidth_ - width) > 0.5f || cacheSize_ != size_ || cacheSearch_ != p_.search) {
        cacheDoc_ = &doc;
        cacheWidth_ = width;
        cacheSize_ = size_;
        cacheSearch_ = std::string(p_.search);
        heights_.assign(doc.blocks.size(), -1.0f);
        blockMatches_.assign(doc.blocks.size(), 0);
        if (!p_.search.empty())
            for (size_t i = 0; i < doc.blocks.size(); ++i)
                blockMatches_[i] = model::countMatches(doc.blocks[i], p_.search);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    if (indent > 0.0f)
        ImGui::Indent(indent);
    rightEdge_ = ImGui::GetCursorScreenPos().x + width;
    space(size_);

    const float visTop = ImGui::GetScrollY();
    const float visBottom = visTop + ImGui::GetWindowHeight();
    for (size_t i = 0; i < doc.blocks.size(); ++i) {
        const Block& block = doc.blocks[i];
        const float y0 = ImGui::GetCursorPosY();
        const int matches = blockMatches_[i];
        const bool holdsCurrent = p_.currentMatch >= matchCounter_ && p_.currentMatch < matchCounter_ + matches;
        const float h = heights_[i];
        if (!p_.fullLayout && h >= 0.0f && !holdsCurrent && (y0 + h < visTop || y0 > visBottom)) {
            if (block.type == BlockType::Heading)
                anchors_[block.anchor] = y0;
            ImGui::Dummy(ImVec2(width, h));
            matchCounter_ += matches;
            atTop_ = false;
            continue;
        }
        ImGui::PushID(int(i));
        renderBlock(block);
        ImGui::PopID();
        heights_[i] = ImGui::GetCursorPosY() - y0;
    }
    space(size_ * 3.0f);

    if (indent > 0.0f)
        ImGui::Unindent(indent);
    ImGui::PopStyleVar();

    if (hoveredLink_ >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const std::string& url = doc.links[size_t(hoveredLink_)].url;
        if (p_.showTooltips && !url.empty() && url[0] != '#')
            ImGui::SetTooltip("%s", url.c_str());
        const ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            io.MouseDragMaxDistanceSqr[0] < io.MouseDragThreshold * io.MouseDragThreshold)
            clickedLink_ = hoveredLink_;
    }
}

// ---- Blocks -------------------------------------------------------------------------------

void MarkdownRenderer::renderBlock(const Block& block)
{
    switch (block.type) {
    case BlockType::Paragraph:
        renderInlines(block.inlines, size_, textColor_, Align::None, 0, availableWidth());
        space(tight_ ? size_ * 0.3f : size_ * 0.85f);
        break;
    case BlockType::Heading:
        renderHeading(block);
        break;
    case BlockType::CodeBlock:
        renderCode(block);
        break;
    case BlockType::BlockQuote:
        renderQuote(block);
        break;
    case BlockType::List:
        renderList(block);
        break;
    case BlockType::ThematicBreak:
        renderRule();
        break;
    case BlockType::Table:
        renderTable(block);
        break;
    case BlockType::ListItem: // only rendered through renderList()
        break;
    }
    atTop_ = false;
}

void MarkdownRenderer::renderHeading(const Block& block)
{
    const int level = std::clamp(block.level, 1, 6);
    const float hs = size_ * kHeadingScale[level - 1];
    anchors_[block.anchor] = ImGui::GetCursorPosY();
    if (!atTop_)
        space(size_ * (level <= 2 ? 0.9f : 0.6f));
    const ImU32 color = level == 6 ? pal_->textDim : pal_->heading;
    renderInlines(block.inlines, hs, color, Align::None, level <= 4 ? model::StyleBold : 0, availableWidth());
    if (level <= 2) {
        space(size_ * 0.3f);
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl_->AddLine(ImVec2(p.x, p.y), ImVec2(rightEdge_, p.y), pal_->rule, std::max(1.0f, size_ * 0.07f));
        space(size_ * 0.7f);
    } else {
        space(size_ * 0.5f);
    }
}

void MarkdownRenderer::renderRule()
{
    space(size_ * 0.6f);
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl_->AddRectFilled(ImVec2(p.x, p.y), ImVec2(rightEdge_, p.y + std::max(2.0f, size_ * 0.18f)), pal_->rule);
    space(size_ * 1.1f);
}

void MarkdownRenderer::renderQuote(const Block& block)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float indent = size_ * 1.1f;
    ImGui::Indent(indent);
    const ImU32 oldColor = textColor_;
    textColor_ = pal_->quoteText;
    for (size_t i = 0; i < block.children.size(); ++i) {
        ImGui::PushID(int(i));
        renderBlock(block.children[i]);
        ImGui::PopID();
    }
    textColor_ = oldColor;
    ImGui::Unindent(indent);
    const float end = ImGui::GetCursorScreenPos().y - size_ * 0.85f; // exclude trailing paragraph spacing
    dl_->AddRectFilled(ImVec2(start.x, start.y), ImVec2(start.x + std::max(3.0f, size_ * 0.22f), std::max(end, start.y + size_)),
                       pal_->quoteBar);
}

void MarkdownRenderer::renderList(const Block& list)
{
    const bool outerTight = tight_;
    tight_ = list.tight;
    ++listDepth_;

    ImFont* font = fonts_.body;
    std::string widest = std::to_string(list.start + int(list.children.size()) - 1) + ".";
    const float markerWidth = std::max(size_ * 1.6f, list.ordered ? measure(font, size_, widest.c_str(), nullptr, 0) + size_ * 0.7f : 0.0f);
    const float lineH = size_ * kLineSpacing;

    int number = list.start;
    for (size_t idx = 0; idx < list.children.size(); ++idx, ++number) {
        const Block& item = list.children[idx];
        ImGui::PushID(int(idx));
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float midY = pos.y + lineH * 0.5f;

        if (item.task >= 0) {
            const float box = size_ * 0.85f;
            ImVec2 a(pos.x + markerWidth - box - size_ * 0.45f, midY - box * 0.5f);
            ImVec2 b(a.x + box, a.y + box);
            if (item.task == 1) {
                dl_->AddRectFilled(a, b, pal_->checkbox, size_ * 0.15f);
                const float t = std::max(1.5f, size_ * 0.12f);
                dl_->AddLine(ImVec2(a.x + box * 0.22f, a.y + box * 0.52f), ImVec2(a.x + box * 0.42f, a.y + box * 0.72f),
                             IM_COL32_WHITE, t);
                dl_->AddLine(ImVec2(a.x + box * 0.42f, a.y + box * 0.72f), ImVec2(a.x + box * 0.78f, a.y + box * 0.3f),
                             IM_COL32_WHITE, t);
            } else {
                dl_->AddRect(a, b, pal_->textDim, size_ * 0.15f, std::max(1.0f, size_ * 0.08f));
            }
        } else if (list.ordered) {
            std::string label = std::to_string(number) + ".";
            float w = measure(font, size_, label.c_str(), label.c_str() + label.size(), 0);
            dl_->AddText(font, size_, ImVec2(pos.x + markerWidth - w - size_ * 0.4f, midY - size_ * 0.5f), textColor_,
                         label.c_str(), label.c_str() + label.size());
        } else {
            const ImVec2 c(pos.x + markerWidth - size_ * 0.8f, midY);
            const float r = size_ * 0.17f;
            switch ((listDepth_ - 1) % 3) {
            case 0: dl_->AddCircleFilled(c, r, textColor_); break;
            case 1: dl_->AddCircle(c, r, textColor_, 0, std::max(1.0f, size_ * 0.07f)); break;
            default: dl_->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), textColor_); break;
            }
        }

        ImGui::Indent(markerWidth);
        if (item.children.empty())
            space(lineH);
        for (size_t i = 0; i < item.children.size(); ++i) {
            ImGui::PushID(int(i));
            renderBlock(item.children[i]);
            ImGui::PopID();
        }
        ImGui::Unindent(markerWidth);
        ImGui::PopID();
    }

    --listDepth_;
    tight_ = outerTight;
    if (list.tight && !tight_)
        space(size_ * 0.55f); // tight items only add small gaps; restore normal block spacing
}

void MarkdownRenderer::renderCode(const Block& block)
{
    ImFont* font = fonts_.mono;
    const float fs = size_ * kCodeScale;
    const float lineH = fs * 1.45f;
    const float pad = size_ * 0.8f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float width = availableWidth();
    const float innerW = std::max(fs * 4.0f, width - pad * 2.0f);
    const std::string& code = block.code;

    // Soft-wrap long lines.
    struct Line {
        size_t begin, end;
    };
    std::vector<Line> lines;
    const char* base = code.c_str();
    size_t start = 0;
    while (start <= code.size()) {
        size_t nl = code.find('\n', start);
        size_t end = nl == std::string::npos ? code.size() : nl;
        if (end == start) {
            lines.push_back({start, end});
        } else {
            const char* s = base + start;
            const char* e = base + end;
            while (s < e) {
                const char* cut = font->CalcWordWrapPosition(fs, s, e, innerW);
                if (cut <= s)
                    cut = nextUtf8(s, e);
                lines.push_back({size_t(s - base), size_t(cut - base)});
                s = cut;
            }
        }
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }

    const float height = float(lines.size()) * lineH + pad * 2.0f;
    const ImVec2 max(pos.x + width, pos.y + height);
    dl_->AddRectFilled(pos, max, pal_->codeBg, size_ * 0.35f);

    const int hitsStart = collectHits(code);
    const int hitsCount = int(hits_.size()) - hitsStart;
    const std::vector<std::uint8_t> cls = highlight(code, block.language);
    const ImU32 colors[] = {pal_->codeText, pal_->syntaxKeyword, pal_->syntaxString, pal_->syntaxComment, pal_->syntaxNumber};

    const ImVec2 clipMin = ImGui::GetWindowDrawList()->GetClipRectMin();
    const ImVec2 clipMax = ImGui::GetWindowDrawList()->GetClipRectMax();
    for (size_t li = 0; li < lines.size(); ++li) {
        const float y = pos.y + pad + float(li) * lineH;
        const bool visible = y + lineH >= clipMin.y && y <= clipMax.y;
        const Line& line = lines[li];
        const float textY = y + (lineH - fs) * 0.5f;
        drawHighlights(font, fs, ImVec2(pos.x + pad, textY), y, y + lineH, base + line.begin, line.begin,
                       line.end - line.begin, hits_.data() + hitsStart, hitsCount);
        if (!visible)
            continue;
        float x = pos.x + pad;
        size_t s = line.begin;
        while (s < line.end) {
            size_t e = s + 1;
            while (e < line.end && cls[e] == cls[s])
                ++e;
            dl_->AddText(font, fs, ImVec2(x, textY), colors[cls[s]], base + s, base + e);
            x += font->CalcTextSizeA(fs, FLT_MAX, 0.0f, base + s, base + e).x;
            s = e;
        }
    }

    // Copy button (and language tag) in the top-right corner.
    const bool hovered = ImGui::IsMouseHoveringRect(pos, max) && ImGui::IsWindowHovered();
    const char* copyLabel = fonts_.hasIcons ? ICON_FA_COPY : "Copy";
    const float buttonW = ImGui::CalcTextSize(copyLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    if (hovered || !p_.showTooltips) {
        ImGui::SetCursorScreenPos(ImVec2(max.x - buttonW - pad * 0.4f, pos.y + pad * 0.3f));
        if (ImGui::SmallButton(copyLabel))
            ImGui::SetClipboardText(code.c_str());
        if (p_.showTooltips)
            ImGui::SetItemTooltip("Copy code");
    } else if (!block.language.empty()) {
        float w = measure(fonts_.body, size_ * 0.75f, block.language.c_str(), nullptr, 0);
        dl_->AddText(fonts_.body, size_ * 0.75f, ImVec2(max.x - w - pad * 0.6f, pos.y + pad * 0.3f), pal_->textDim,
                     block.language.c_str());
    }

    ImGui::SetCursorScreenPos(pos);
    ImGui::Dummy(ImVec2(width, height));
    space(size_ * 0.85f);
}

void MarkdownRenderer::renderTable(const Block& block)
{
    const int columns = int(block.aligns.size());
    if (columns == 0 || block.rows.empty())
        return;
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(size_ * 0.6f, size_ * 0.3f));
    if (ImGui::BeginTable("##table", columns, flags, ImVec2(availableWidth(), 0.0f))) {
        for (size_t r = 0; r < block.rows.size(); ++r) {
            ImGui::TableNextRow(r == 0 ? ImGuiTableRowFlags_Headers : ImGuiTableRowFlags_None);
            if (r == 0)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, pal_->tableHeaderBg);
            const auto& row = block.rows[r];
            for (int c = 0; c < columns && size_t(c) < row.size(); ++c) {
                ImGui::TableSetColumnIndex(c);
                ImGui::PushID(int(r) * columns + c);
                renderInlines(row[size_t(c)], size_, textColor_, block.aligns[size_t(c)],
                              r == 0 ? model::StyleBold : 0, ImGui::GetContentRegionAvail().x);
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    space(size_ * 0.85f);
}

// ---- Inline layout --------------------------------------------------------------------------

float MarkdownRenderer::availableWidth() const
{
    return std::max(size_ * 4.0f, rightEdge_ - ImGui::GetCursorScreenPos().x);
}

float MarkdownRenderer::localY(float screenY) const
{
    return screenY - ImGui::GetWindowPos().y + ImGui::GetScrollY();
}

void MarkdownRenderer::space(float h)
{
    ImGui::Dummy(ImVec2(0.0f, h));
}

float MarkdownRenderer::measure(ImFont* font, float size, const char* b, const char* e, std::uint8_t style) const
{
    float w = font->CalcTextSizeA(size, FLT_MAX, 0.0f, b, e).x;
    if (style & model::StyleBold)
        w += std::max(1.0f, size / 24.0f);
    return w;
}

void MarkdownRenderer::drawText(ImFont* font, float size, ImVec2 pos, ImU32 color, const char* b, const char* e,
                                std::uint8_t style)
{
    const int firstVertex = dl_->VtxBuffer.Size;
    dl_->AddText(font, size, pos, color, b, e);
    if (style & model::StyleBold) // faux bold: overdraw with a small horizontal offset
        dl_->AddText(font, size, ImVec2(pos.x + std::max(1.0f, size / 24.0f), pos.y), color, b, e);
    if (style & model::StyleItalic) { // faux italic: shear the glyph quads around the baseline
        const float baseline = pos.y + size * 0.8f;
        for (int i = firstVertex; i < dl_->VtxBuffer.Size; ++i) {
            ImDrawVert& v = dl_->VtxBuffer[i];
            v.pos.x += (baseline - v.pos.y) * kItalicShear;
        }
    }
}

int MarkdownRenderer::collectHits(std::string_view text)
{
    const int first = int(hits_.size());
    if (p_.search.empty())
        return first;
    for (size_t offset : model::findAll(text, p_.search))
        hits_.push_back({offset, offset + p_.search.size(), matchCounter_++});
    return first;
}

void MarkdownRenderer::drawHighlights(ImFont* font, float size, ImVec2 pos, float top, float bottom, const char* lineBegin,
                                      size_t lineOffset, size_t lineLength, const Hit* hits, int hitCount)
{
    for (int h = 0; h < hitCount; ++h) {
        const Hit& hit = hits[h];
        size_t b = std::max(hit.begin, lineOffset);
        size_t e = std::min(hit.end, lineOffset + lineLength);
        if (b >= e)
            continue;
        float x0 = pos.x + font->CalcTextSizeA(size, FLT_MAX, 0.0f, lineBegin, lineBegin + (b - lineOffset)).x;
        float x1 = pos.x + font->CalcTextSizeA(size, FLT_MAX, 0.0f, lineBegin, lineBegin + (e - lineOffset)).x;
        const bool current = hit.index == p_.currentMatch;
        dl_->AddRectFilled(ImVec2(x0, top), ImVec2(x1, bottom), current ? pal_->highlightCurrent : pal_->highlight,
                           size * 0.12f);
        if (current)
            currentMatchY_ = localY(top);
    }
}

void MarkdownRenderer::layoutInlines(const Inlines& inlines, float size, std::uint8_t extraStyle, float width)
{
    frags_.clear();
    lineWidths_.clear();
    float x = 0.0f;
    int line = 0;
    bool pendingSpace = false;
    const float spaceW = measure(fonts_.body, size, " ", nullptr, 0);

    auto newLine = [&]() {
        lineWidths_.push_back(x);
        x = 0.0f;
        ++line;
        pendingSpace = false;
    };
    auto place = [&](const Span* span, const char* b, const char* e, float w, size_t offset, int hb, int he) {
        if (pendingSpace && x > 0.0f) {
            if (x + spaceW + w > width)
                newLine();
            else
                x += spaceW;
        } else if (x > 0.0f && x + w > width) {
            newLine();
        }
        pendingSpace = false;
        frags_.push_back({x, w, line, b, e, span, offset, hb, he});
        x += w;
    };

    for (const Span& span : inlines) {
        if (span.kind == SpanKind::SoftBreak) {
            pendingSpace = true;
            continue;
        }
        if (span.kind == SpanKind::HardBreak) {
            newLine();
            continue;
        }
        if (span.kind == SpanKind::Image) {
            const std::string& label = scratch_.emplace_back(
                std::string(fonts_.hasIcons ? ICON_FA_IMAGE " " : "[image] ") + (span.text.empty() ? "image" : span.text));
            float w = measure(fonts_.body, size, label.c_str(), label.c_str() + label.size(), 0);
            place(&span, label.c_str(), label.c_str() + label.size(), std::min(w, width), 0, 0, 0);
            continue;
        }

        const std::uint8_t style = std::uint8_t(span.style | extraStyle);
        const bool code = (style & model::StyleCode) != 0;
        ImFont* font = code ? fonts_.mono : fonts_.body;
        const float fs = code ? size * kCodeScale : size;
        const float codePad = code ? size * 0.2f : 0.0f;
        const int hb = collectHits(span.text);
        const int he = int(hits_.size());

        const std::string& t = span.text;
        size_t pos = 0;
        while (pos < t.size()) {
            if (t[pos] == ' ' && !code) {
                pendingSpace = true;
                ++pos;
                continue;
            }
            size_t end = pos;
            // Keep the rest of a code span in one piece when it fits, so its background is continuous.
            const bool codeFits =
                code && x + (pendingSpace && x > 0.0f ? spaceW : 0.0f) +
                                measure(font, fs, t.c_str() + pos, t.c_str() + t.size(), style) + codePad * 2.0f <= width;
            if (codeFits) {
                end = t.size();
            } else if (code) { // otherwise break between words, keeping spaces attached to them
                while (end < t.size() && t[end] == ' ')
                    ++end;
                while (end < t.size() && t[end] != ' ')
                    ++end;
                while (end < t.size() && t[end] == ' ')
                    ++end;
            } else {
                end = t.find(' ', pos);
                if (end == std::string::npos)
                    end = t.size();
            }
            const char* b = t.c_str() + pos;
            const char* e = t.c_str() + end;
            float w = measure(font, fs, b, e, style) + codePad * 2.0f;
            if (w <= width) {
                place(&span, b, e, w, pos, hb, he);
            } else {
                // Token wider than the line: break at character boundaries.
                if (x > 0.0f)
                    newLine();
                while (b < e) {
                    const char* cut = nullptr;
                    font->CalcTextSizeA(fs, width - x - codePad * 2.0f, 0.0f, b, e, &cut);
                    if (cut <= b)
                        cut = nextUtf8(b, e);
                    float cw = measure(font, fs, b, cut, style) + codePad * 2.0f;
                    frags_.push_back({x, cw, line, b, cut, &span, size_t(b - t.c_str()), hb, he});
                    x += cw;
                    b = cut;
                    if (b < e)
                        newLine();
                }
            }
            pos = end;
        }
    }
    lineWidths_.push_back(x);
}

void MarkdownRenderer::renderInlines(const Inlines& inlines, float size, ImU32 color, Align align,
                                     std::uint8_t extraStyle, float width)
{
    hits_.clear();
    layoutInlines(inlines, size, extraStyle, width);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float lineH = size * kLineSpacing;
    const int lineCount = int(lineWidths_.size());
    const float height = float(std::max(1, lineCount)) * lineH;
    const ImVec2 clipMin = dl_->GetClipRectMin();
    const ImVec2 clipMax = dl_->GetClipRectMax();

    for (const Fragment& f : frags_) {
        float offset = 0.0f;
        const float lw = lineWidths_[size_t(f.line)];
        if (align == Align::Center)
            offset = std::floor((width - lw) * 0.5f);
        else if (align == Align::Right)
            offset = width - lw;
        const float x = origin.x + f.x + offset;
        const float top = origin.y + float(f.line) * lineH;
        const Span& span = *f.span;
        const std::uint8_t style = std::uint8_t(span.style | extraStyle);
        const bool code = (style & model::StyleCode) != 0 && span.kind == SpanKind::Text;
        ImFont* font = code ? fonts_.mono : fonts_.body;
        const float fs = code ? size * kCodeScale : size;
        const float codePad = code ? size * 0.2f : 0.0f;
        const float textY = top + (lineH - fs) * 0.5f;

        const int link = span.link;
        if (link >= 0 && canHover_ && ImGui::IsMouseHoveringRect(ImVec2(x, top), ImVec2(x + f.w, top + lineH)))
            hoveredLink_ = link;
        const bool hot = link >= 0 && link == hoveredLinkPrev_;

        if (code)
            dl_->AddRectFilled(ImVec2(x, textY - size * 0.1f), ImVec2(x + f.w, textY + fs + size * 0.1f), pal_->codeBg,
                               size * 0.2f);
        if (f.hitsEnd > f.hitsBegin)
            drawHighlights(font, fs, ImVec2(x + codePad, textY), top + size * 0.1f, top + lineH - size * 0.1f, f.begin,
                           f.spanOffset, size_t(f.end - f.begin), hits_.data() + f.hitsBegin, f.hitsEnd - f.hitsBegin);

        if (top > clipMax.y || top + lineH < clipMin.y)
            continue;
        const ImU32 col = link >= 0 ? (hot ? pal_->linkHover : pal_->link) : code ? pal_->codeText : color;
        drawText(font, fs, ImVec2(x + codePad, textY), col, f.begin, f.end, span.kind == SpanKind::Image ? 0 : style);

        const float thickness = std::max(1.0f, size * 0.07f);
        if (hot) {
            float y = textY + fs * 1.02f;
            dl_->AddLine(ImVec2(x, y), ImVec2(x + f.w, y), col, thickness);
        }
        if (style & model::StyleStrike) {
            float y = textY + fs * 0.55f;
            dl_->AddLine(ImVec2(x, y), ImVec2(x + f.w, y), col, thickness);
        }
    }

    float used = 0.0f;
    for (float w : lineWidths_)
        used = std::max(used, w);
    ImGui::Dummy(ImVec2(std::min(width, used), height));
}

} // namespace mdv::view
