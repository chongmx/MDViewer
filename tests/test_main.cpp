// Unit tests for the model, OS-layer helpers and view-model (no framework dependency).
#include "mdv/model/markdown_parser.h"
#include "mdv/model/search.h"
#include "mdv/model/settings.h"
#include "mdv/os/fs.h"
#include "mdv/os/net.h"
#include "mdv/viewmodel/viewer_view_model.h"

#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                           \
    } while (0)

#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        ++g_checks;                                                                                 \
        auto va = (a);                                                                              \
        auto vb = (b);                                                                              \
        if (!(va == vb)) {                                                                          \
            ++g_failures;                                                                           \
            std::printf("  FAILED %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);                 \
        }                                                                                           \
    } while (0)

using namespace mdv;
using model::BlockType;

std::vector<std::pair<const char*, std::function<void()>>>& registry()
{
    static std::vector<std::pair<const char*, std::function<void()>>> tests;
    return tests;
}

struct Register {
    Register(const char* name, std::function<void()> fn) { registry().emplace_back(name, std::move(fn)); }
};

#define TEST(name)                                       \
    static void name();                                  \
    static Register register_##name(#name, &name);       \
    static void name()

// ---- Parser ------------------------------------------------------------------------------------

TEST(parses_headings_and_paragraphs)
{
    model::Document doc = model::parseMarkdown("# Title\n\nHello *world*\nnext line\n\nSetext\n---\n");
    CHECK_EQ(doc.blocks.size(), size_t(3));
    CHECK(doc.blocks[0].type == BlockType::Heading);
    CHECK_EQ(doc.blocks[0].level, 1);
    CHECK_EQ(doc.title, std::string("Title"));
    CHECK(doc.blocks[1].type == BlockType::Paragraph);
    CHECK_EQ(model::plainText(doc.blocks[1].inlines), std::string("Hello world next line"));
    CHECK(doc.blocks[2].type == BlockType::Heading);
    CHECK_EQ(doc.blocks[2].level, 2);
    CHECK_EQ(doc.headings.size(), size_t(2));
}

TEST(parses_emphasis)
{
    model::Document doc = model::parseMarkdown("a **bold** and *it* and ***both*** ~~gone~~ `co*de*` snake_case_word");
    const model::Inlines& in = doc.blocks[0].inlines;
    auto find = [&](const char* text) -> const model::Span* {
        for (const model::Span& s : in)
            if (s.text == text)
                return &s;
        return nullptr;
    };
    CHECK(find("bold") && find("bold")->style == model::StyleBold);
    CHECK(find("it") && find("it")->style == model::StyleItalic);
    CHECK(find("both") && find("both")->style == (model::StyleBold | model::StyleItalic));
    CHECK(find("gone") && find("gone")->style == model::StyleStrike);
    CHECK(find("co*de*") && find("co*de*")->style == model::StyleCode);
    CHECK(model::plainText(in).find("snake_case_word") != std::string::npos);
}

TEST(parses_nested_emphasis)
{
    model::Document doc = model::parseMarkdown("*foo **bar** baz*");
    const model::Inlines& in = doc.blocks[0].inlines;
    CHECK_EQ(model::plainText(in), std::string("foo bar baz"));
    bool sawBoldItalic = false;
    for (const model::Span& s : in)
        if (s.text == "bar")
            sawBoldItalic = s.style == (model::StyleBold | model::StyleItalic);
    CHECK(sawBoldItalic);
}

TEST(parses_links_and_images)
{
    model::Document doc = model::parseMarkdown(
        "See [the docs](docs/a.md \"Title\") and [ref][r] and <https://x.org> and https://y.org/path.\n"
        "![alt text](img.png)\n\n[r]: https://ref.example\n");
    CHECK_EQ(doc.blocks.size(), size_t(1));
    std::vector<std::string> urls;
    for (const model::Link& l : doc.links)
        urls.push_back(l.url);
    CHECK_EQ(urls.size(), size_t(5));
    CHECK_EQ(urls[0], std::string("docs/a.md"));
    CHECK_EQ(doc.links[0].title, std::string("Title"));
    CHECK_EQ(urls[1], std::string("https://ref.example"));
    CHECK_EQ(urls[2], std::string("https://x.org"));
    CHECK_EQ(urls[3], std::string("https://y.org/path"));
    CHECK_EQ(urls[4], std::string("img.png"));
    bool image = false;
    for (const model::Span& s : doc.blocks[0].inlines)
        if (s.kind == model::SpanKind::Image)
            image = s.text == "alt text";
    CHECK(image);
}

TEST(parses_lists)
{
    model::Document doc = model::parseMarkdown("1. one\n2. two\n   - nested\n   - more\n3. three\n\n- [x] done\n- [ ] todo\n");
    CHECK_EQ(doc.blocks.size(), size_t(2));
    const model::Block& ol = doc.blocks[0];
    CHECK(ol.type == BlockType::List && ol.ordered);
    CHECK(ol.tight);
    CHECK_EQ(ol.children.size(), size_t(3));
    CHECK_EQ(ol.children[1].children.size(), size_t(2));
    CHECK(ol.children[1].children[1].type == BlockType::List);
    const model::Block& tasks = doc.blocks[1];
    CHECK_EQ(tasks.children[0].task, 1);
    CHECK_EQ(tasks.children[1].task, 0);
}

TEST(loose_list)
{
    model::Document doc = model::parseMarkdown("- a\n\n- b\n");
    CHECK_EQ(doc.blocks.size(), size_t(1));
    CHECK(!doc.blocks[0].tight);
}

TEST(parses_code_blocks)
{
    model::Document doc = model::parseMarkdown("```cpp\nint x;\n\n# not heading\n```\n\n    indented\n    code\n");
    CHECK_EQ(doc.blocks.size(), size_t(2));
    CHECK(doc.blocks[0].type == BlockType::CodeBlock);
    CHECK_EQ(doc.blocks[0].language, std::string("cpp"));
    CHECK_EQ(doc.blocks[0].code, std::string("int x;\n\n# not heading"));
    CHECK_EQ(doc.blocks[1].code, std::string("indented\ncode"));
}

TEST(parses_quotes_rules_tables)
{
    model::Document doc = model::parseMarkdown("> quote\nlazy\n\n***\n\n| A | B |\n|:--|--:|\n| 1 | 2 |\n| x \\| y |\n");
    CHECK_EQ(doc.blocks.size(), size_t(3));
    CHECK(doc.blocks[0].type == BlockType::BlockQuote);
    CHECK_EQ(model::plainText(doc.blocks[0].children[0].inlines), std::string("quote lazy"));
    CHECK(doc.blocks[1].type == BlockType::ThematicBreak);
    const model::Block& t = doc.blocks[2];
    CHECK(t.type == BlockType::Table);
    CHECK_EQ(t.rows.size(), size_t(3));
    CHECK(t.aligns[0] == model::Align::Left && t.aligns[1] == model::Align::Right);
    CHECK_EQ(model::plainText(t.rows[2][0]), std::string("x | y"));
}

TEST(html_is_reduced)
{
    model::Document doc = model::parseMarkdown("<p align=\"center\">\n<img src=\"logo.png\" alt=\"Logo\">\n</p>\n\n<!-- comment -->\n\nText<br>more &amp; &#65;");
    CHECK_EQ(doc.blocks.size(), size_t(2));
    CHECK(doc.blocks[0].inlines.size() >= 1);
    CHECK(doc.blocks[0].inlines[0].kind == model::SpanKind::Image);
    CHECK_EQ(model::plainText(doc.blocks[1].inlines), std::string("Text more & A"));
}

TEST(heading_anchors_are_unique)
{
    model::Document doc = model::parseMarkdown("# Hello, World!\n## Hello, World!\n");
    CHECK_EQ(doc.headings[0].anchor, std::string("hello-world"));
    CHECK_EQ(doc.headings[1].anchor, std::string("hello-world-1"));
}

// ---- Search ------------------------------------------------------------------------------------

TEST(search_counts_matches)
{
    model::Document doc = model::parseMarkdown("# Foo\nfoo FOO\n\n```\nfoofoo\n```\n\n| foo |\n|---|\n| x |\n");
    CHECK_EQ(model::countMatches(doc, "foo"), 6);
    CHECK_EQ(model::countMatches(doc, ""), 0);
    CHECK_EQ(model::findAll("aaaa", "aa").size(), size_t(2));
}

// ---- Settings ----------------------------------------------------------------------------------

TEST(settings_round_trip)
{
    model::Settings s;
    s.darkTheme = false;
    s.zoom = 1.5f;
    s.addRecent("/a.md");
    s.addRecent("/b.md");
    s.addRecent("/a.md");
    s.addRecent("asset:docs/welcome.md");
    model::Settings r = model::Settings::deserialize(s.serialize());
    CHECK(!r.darkTheme);
    CHECK(r.zoom > 1.49f && r.zoom < 1.51f);
    CHECK_EQ(r.recentDocuments.size(), size_t(2));
    CHECK_EQ(r.recentDocuments[0], std::string("/a.md"));
}

// ---- OS helpers --------------------------------------------------------------------------------

TEST(path_helpers)
{
    using namespace os::fs::path;
    CHECK_EQ(normalize("a/b/../c/./d"), std::string("a/c/d"));
    CHECK_EQ(normalize("C:\\x\\..\\y\\z.md"), std::string("C:/y/z.md"));
    CHECK_EQ(normalize("/a/../../b"), std::string("/b"));
    CHECK_EQ(join("/docs/sub", "../readme.md"), std::string("/docs/readme.md"));
    CHECK_EQ(parent("/docs/readme.md"), std::string("/docs"));
    CHECK_EQ(parent("C:/readme.md"), std::string("C:/"));
    CHECK_EQ(filename("/docs/readme.md"), std::string("readme.md"));
    CHECK_EQ(extension("/docs/README.MD"), std::string(".md"));
    CHECK(isAbsolute("/x") && isAbsolute("D:/x") && !isAbsolute("x/y"));
}

TEST(url_helpers)
{
    auto u = os::net::parseUrl("https://user@example.com:8443/a/b?q=1#frag");
    CHECK(u.has_value());
    CHECK_EQ(u->scheme, std::string("https"));
    CHECK_EQ(u->host, std::string("example.com"));
    CHECK_EQ(u->port, 8443);
    CHECK_EQ(u->path, std::string("/a/b?q=1"));
    CHECK_EQ(u->fragment, std::string("frag"));
    CHECK_EQ(os::net::parseUrl("http://x.org")->path, std::string("/"));
    CHECK_EQ(os::net::resolveUrl("https://h.org/docs/a/readme.md", "../b.md#s"), std::string("https://h.org/docs/b.md#s"));
    CHECK_EQ(os::net::resolveUrl("https://h.org/docs/readme.md", "/root.md"), std::string("https://h.org/root.md"));
    CHECK_EQ(os::net::resolveUrl("https://h.org/docs/readme.md", "//cdn.org/x"), std::string("https://cdn.org/x"));
    CHECK(os::net::isUrl("mailto:a@b.c") && os::net::isUrl("http://x") && !os::net::isUrl("C:/x"));
    CHECK_EQ(os::net::percentDecode("a%20b%2"), std::string("a b%2"));
}

// ---- View-model with fake services ---------------------------------------------------------------

struct FakeDocuments : interfaces::IDocumentService {
    std::map<std::string, std::string> files;
    std::vector<std::string> loaded;

    void load(const std::string& location, interfaces::LoadCallback callback) override
    {
        loaded.push_back(location);
        auto it = files.find(location);
        if (it == files.end()) {
            callback({nullptr, "missing " + location});
            return;
        }
        auto doc = std::make_shared<model::Document>(model::parseMarkdown(it->second));
        doc->location = location;
        callback({doc, {}});
    }
    std::string normalizeLocation(const std::string& input) const override { return input; }
    std::string resolveLink(const std::string& base, const std::string& href) const override
    {
        return os::fs::path::join(os::fs::path::parent(base), href);
    }
    bool isLocalFile(const std::string& l) const override { return !l.empty() && l[0] == '/'; }
    bool isRemote(const std::string&) const override { return false; }
    bool isAsset(const std::string& l) const override { return l.rfind("asset:", 0) == 0; }
    bool isExternalScheme(const std::string& l) const override { return l.rfind("mailto:", 0) == 0 || l.rfind("https:", 0) == 0; }
};

struct FakeSettings : interfaces::ISettingsService {
    model::Settings stored;
    int saves = 0;
    model::Settings load() override { return stored; }
    bool save(const model::Settings& s) override
    {
        stored = s;
        ++saves;
        return true;
    }
};

struct FakeFileSystem : interfaces::IFileSystemService {
    std::vector<model::FileEntry> listDirectory(const std::string&, std::string*) override
    {
        return {{"sub", true, 0}, {"a.md", false, 1}, {"b.png", false, 1}};
    }
    std::vector<std::string> roots() override { return {"/"}; }
    std::string join(const std::string& d, const std::string& n) const override { return os::fs::path::join(d, n); }
    std::string parent(const std::string& p) const override { return os::fs::path::parent(p); }
    std::string normalize(const std::string& p) const override { return os::fs::path::normalize(p); }
    std::optional<std::int64_t> modifiedTime(const std::string&) override { return mtime; }
    std::int64_t mtime = 100;
};

struct FakePlatform : interfaces::IPlatformService {
    std::vector<std::string> opened;
    double time = 0.0;
    bool hasNativeFilePicker() const override { return false; }
    void pickMarkdownFile(interfaces::PickFileCallback cb) override { cb(std::nullopt); }
    bool openExternal(const std::string& u) override
    {
        opened.push_back(u);
        return true;
    }
    double now() const override { return time; }
    std::string platformName() const override { return "Test"; }
    std::string architectureName() const override { return "test"; }
    bool supportsHttps() const override { return true; }
    bool isDesktop() const override { return true; }
    bool isMobile() const override { return false; }
};

struct Fixture {
    FakeDocuments documents;
    FakeSettings settings;
    FakeFileSystem fileSystem;
    FakePlatform platform;
    std::unique_ptr<viewmodel::ViewerViewModel> vm;

    Fixture()
    {
        documents.files["/docs/a.md"] = "# A\n\n[to b](b.md#part) [mail](mailto:x@y.z) [web](https://example.com) [anchor](#a)";
        documents.files["/docs/b.md"] = "# B\n\n## Part\n\nfoo foo foo";
        vm = std::make_unique<viewmodel::ViewerViewModel>(documents, settings, fileSystem, platform);
    }
};

TEST(viewmodel_navigation_and_history)
{
    Fixture f;
    std::vector<std::string> anchors;
    auto sub = f.vm->scrollToAnchorRequested.subscribe([&](const std::string& a) { anchors.push_back(a); });

    f.vm->open("/docs/a.md");
    CHECK(f.vm->document.get() != nullptr);
    CHECK_EQ(f.vm->document.get()->title, std::string("A"));
    CHECK(!f.vm->backCommand.canExecute());

    f.vm->followLink("b.md#part");
    CHECK_EQ(f.vm->currentLocation(), std::string("/docs/b.md"));
    CHECK_EQ(anchors.size(), size_t(1));
    CHECK_EQ(anchors.back(), std::string("part"));
    CHECK(f.vm->backCommand.canExecute());

    f.vm->backCommand.execute();
    CHECK_EQ(f.vm->currentLocation(), std::string("/docs/a.md"));
    CHECK(f.vm->forwardCommand.canExecute());

    f.vm->followLink("#a");
    CHECK_EQ(anchors.back(), std::string("a"));

    f.vm->followLink("mailto:x@y.z");
    f.vm->followLink("https://example.com");
    CHECK_EQ(f.platform.opened.size(), size_t(2));

    f.vm->followLink("missing.md");
    CHECK(!f.vm->errorText.get().empty());
    CHECK_EQ(f.settings.stored.recentDocuments.front(), std::string("/docs/a.md"));
}

TEST(viewmodel_search)
{
    Fixture f;
    f.vm->open("/docs/b.md");
    int scrolls = 0;
    auto sub = f.vm->scrollToMatchRequested.subscribe([&]() { ++scrolls; });
    f.vm->setSearchQuery("FOO");
    CHECK_EQ(f.vm->matchCount.get(), 3);
    CHECK_EQ(f.vm->currentMatch.get(), 0);
    f.vm->nextMatchCommand.execute();
    f.vm->nextMatchCommand.execute();
    f.vm->nextMatchCommand.execute();
    CHECK_EQ(f.vm->currentMatch.get(), 0);
    f.vm->previousMatchCommand.execute();
    CHECK_EQ(f.vm->currentMatch.get(), 2);
    CHECK(scrolls >= 5);
    f.vm->setSearchQuery("");
    CHECK_EQ(f.vm->matchCount.get(), 0);
    CHECK(!f.vm->nextMatchCommand.canExecute());
}

TEST(viewmodel_zoom_settings_and_watch)
{
    Fixture f;
    f.vm->zoomInCommand.execute();
    CHECK(f.vm->zoom.get() > 1.05f);
    f.vm->darkTheme.set(false);
    CHECK(!f.settings.stored.darkTheme);

    f.vm->open("/docs/b.md");
    size_t loads = f.documents.loaded.size();
    f.platform.time = 10.0;
    f.vm->tick();
    CHECK_EQ(f.documents.loaded.size(), loads); // unchanged
    f.fileSystem.mtime = 200;
    f.platform.time = 20.0;
    f.vm->tick();
    CHECK_EQ(f.documents.loaded.size(), loads + 1); // reloaded after change
}

TEST(file_browser_filters)
{
    Fixture f;
    bool requested = false;
    auto sub = f.vm->fileBrowserRequested.subscribe([&]() { requested = true; });
    f.vm->openFileCommand.execute(); // fake platform has no native picker
    CHECK(requested);
    auto& browser = f.vm->fileBrowser();
    CHECK_EQ(browser.entries().size(), size_t(2)); // dir + markdown only
    browser.showAllFiles.set(true);
    CHECK_EQ(browser.entries().size(), size_t(3));
    auto path = browser.activate(browser.entries()[1]);
    CHECK(path.has_value() && *path == "/a.md");
}

} // namespace

int main()
{
    for (auto& [name, fn] : registry()) {
        int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[FAIL]", name);
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
