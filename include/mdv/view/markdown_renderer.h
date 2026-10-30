// Draws a model::Document with ImGui: custom word-wrapped inline layout, links, search
// highlighting, syntax-coloured code, tables and off-screen block culling.
#pragma once

#include "mdv/model/document.h"
#include "mdv/view/fonts.h"
#include "mdv/view/theme.h"

#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mdv::view {

class MarkdownRenderer {
public:
    struct Params {
        const Palette* palette = nullptr;
        float fontSize = 16.0f;   // body text size in pixels (zoom and DPI applied)
        float maxWidth = 900.0f;  // content column width limit in pixels
        std::string_view search;
        int currentMatch = -1;
        bool fullLayout = false;  // lay out every block (needed before scrolling to anchors/matches)
        bool showTooltips = true;
    };

    explicit MarkdownRenderer(const Fonts& fonts) : fonts_(fonts) {}

    // Renders inside the current scrolling window.
    void render(const model::Document& doc, const Params& params);
    // Drops cached block heights (call when the document object changes).
    void invalidate() { cacheDoc_ = nullptr; }

    // Results of the last render(), in the window's scroll coordinates.
    int clickedLink() const { return clickedLink_; }
    std::optional<float> anchorY(const std::string& anchor) const;
    std::optional<float> currentMatchY() const { return currentMatchY_; }

private:
    struct Fragment {
        float x = 0, w = 0;
        int line = 0;
        const char* begin = nullptr;
        const char* end = nullptr;
        const model::Span* span = nullptr;
        size_t spanOffset = 0; // offset of `begin` within span->text
        int hitsBegin = 0, hitsEnd = 0; // range in hits_ for the span
    };
    struct Hit {
        size_t begin, end;
        int index; // global match number
    };

    void renderBlock(const model::Block& block);
    void renderHeading(const model::Block& block);
    void renderCode(const model::Block& block);
    void renderQuote(const model::Block& block);
    void renderList(const model::Block& block);
    void renderTable(const model::Block& block);
    void renderRule();
    void renderInlines(const model::Inlines& inlines, float size, ImU32 color, model::Align align,
                       std::uint8_t extraStyle, float width);

    void layoutInlines(const model::Inlines& inlines, float size, std::uint8_t extraStyle, float width);
    float measure(ImFont* font, float size, const char* b, const char* e, std::uint8_t style) const;
    void drawText(ImFont* font, float size, ImVec2 pos, ImU32 color, const char* b, const char* e, std::uint8_t style);
    void drawHighlights(ImFont* font, float size, ImVec2 pos, float top, float bottom, const char* lineBegin,
                        size_t lineOffset, size_t lineLength, const Hit* hits, int hitCount);
    int collectHits(std::string_view text); // appends to hits_, returns first index
    float availableWidth() const;
    float localY(float screenY) const;
    void space(float h);

    const Fonts& fonts_;
    Params p_;
    const Palette* pal_ = nullptr;
    ImDrawList* dl_ = nullptr;
    float size_ = 16.0f;
    float rightEdge_ = 0.0f;
    ImU32 textColor_ = 0;
    bool tight_ = false;
    int listDepth_ = 0;
    bool atTop_ = true;
    bool canHover_ = false;

    int matchCounter_ = 0;
    std::optional<float> currentMatchY_;
    std::unordered_map<std::string, float> anchors_;
    int hoveredLink_ = -1;
    int hoveredLinkPrev_ = -1;
    int clickedLink_ = -1;

    std::vector<Fragment> frags_;
    std::vector<float> lineWidths_;
    std::vector<Hit> hits_;
    std::deque<std::string> scratch_; // stable storage for generated labels

    // Culling cache, keyed on what affects block heights.
    const model::Document* cacheDoc_ = nullptr;
    float cacheWidth_ = 0.0f;
    float cacheSize_ = 0.0f;
    std::string cacheSearch_;
    std::vector<float> heights_;
    std::vector<int> blockMatches_;
};

} // namespace mdv::view
