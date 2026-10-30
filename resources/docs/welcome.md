# Welcome to MDViewer

**MDViewer** is a fast, cross-platform Markdown viewer built with
[Dear ImGui](https://github.com/ocornut/imgui), OpenGL and GLFW. It runs on
Windows, Linux, macOS, Android and in the browser (WebAssembly), on both
*x86_64* and *aarch64*.

> Open a document with **File › Open** (`Ctrl+O`), load one from the web with
> **File › Open URL** (`Ctrl+L`), or drag & drop a `.md` file onto the window.

## Contents

- [Text formatting](#text-formatting)
- [Lists](#lists)
- [Code](#code)
- [Tables](#tables)
- [Links and navigation](#links-and-navigation)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Architecture](architecture.md)

---

## Text formatting

You can write *italic*, **bold**, ***bold italic***, ~~strikethrough~~ and
`inline code`. Escaped characters such as \*literal asterisks\* stay as they
are, and HTML entities like &copy; &mdash; &rarr; are decoded.

A line ending with two spaces  
forces a hard line break.

Bare URLs such as https://github.com/ocornut/imgui become links automatically.

### Block quotes

> Markdown is intended to be as easy-to-read and easy-to-write as is feasible.
>
> > Quotes can be nested, and can contain **formatting** and `code`.

## Lists

1. Ordered lists
2. With *inline* formatting
   - and nested
   - bullet lists
     - several levels deep
3. Continue numbering

- [x] Task lists are supported
- [x] Checked items
- [ ] Unchecked items

## Code

Fenced code blocks get lightweight syntax highlighting and a copy button:

```cpp
#include <cstdio>

// Entry point
int main(int argc, char** argv)
{
    const char* name = argc > 1 ? argv[1] : "world";
    std::printf("Hello, %s! (%d)\n", name, 42);
    return 0;
}
```

```python
def fibonacci(n: int) -> list[int]:
    """Return the first n Fibonacci numbers."""
    seq = [0, 1]
    while len(seq) < n:
        seq.append(seq[-1] + seq[-2])  # next term
    return seq[:n]
```

    Indented code blocks work too.

## Tables

| Platform | Architectures      | Backend              |
|:---------|:------------------:|---------------------:|
| Windows  | x86_64, aarch64    | GLFW + OpenGL 3      |
| Linux    | x86_64, aarch64    | GLFW + OpenGL 3      |
| macOS    | x86_64, arm64      | GLFW + OpenGL 3.2    |
| Android  | arm64-v8a, x86_64  | EGL + OpenGL ES 3    |
| Web      | wasm32             | GLFW + WebGL 2       |

## Links and navigation

- Links to other Markdown files open inside the viewer — try the
  [architecture overview](architecture.md).
- Links to headings scroll to them: [back to the top](#welcome-to-mdviewer).
- Other links open in your browser.
- Use **Back** / **Forward** (`Alt+Left` / `Alt+Right`) to move through history.
- Local files are reloaded automatically when they change on disk.

## Keyboard shortcuts

| Shortcut              | Action                 |
|-----------------------|------------------------|
| `Ctrl+O`              | Open file              |
| `Ctrl+L`              | Open URL               |
| `Ctrl+F`              | Find                   |
| `F3` / `Shift+F3`     | Next / previous match  |
| `F5`                  | Reload                 |
| `Alt+Left/Right`      | Back / forward         |
| `Ctrl+=` / `Ctrl+-`   | Zoom in / out          |
| `Ctrl+0`              | Reset zoom             |
| `Ctrl+Wheel`          | Zoom                   |
| `PgUp/PgDn/Home/End`  | Scroll                 |
