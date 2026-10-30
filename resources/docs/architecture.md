# Architecture

MDViewer follows the **Model–View–ViewModel** pattern, with a platform
abstraction layer underneath.

```text
 +---------------------------- app ----------------------------+
 |  main_glfw.cpp (desktop, web)   main_android.cpp (Android)  |
 |  Application: composition root, frame loop                  |
 +--------------+----------------------------------------------+
                |
 +---- view ----v---+  binds   +---- viewmodel ----+
 | MainView         | -------> | ViewerViewModel   |
 | MarkdownRenderer | <------- | FileBrowserVM     |
 | Theme, Fonts     |  events  | Property/Command  |
 +------------------+          +---------+---------+
                                         | uses
                               +---------v---------+
                               |    interfaces     |
                               | IDocumentService  |
                               | ISettingsService  |
                               | IFileSystemService|
                               | IPlatformService  |
                               +---------^---------+
                                         | implements
 +---- model -------+          +---------+---------+
 | Document (AST)   | <------- |     services      |
 | Markdown parser  |          +---------+---------+
 | Search, Settings |                    |
 +------------------+          +---------v---------+
                               |     OS layer      |
                               | fs, net, dialog,  |
                               | shell, dispatch,  |
                               | log, platform     |
                               +-------------------+
```

## Layers

| Layer          | Depends on                 | Responsibility                                   |
|----------------|----------------------------|--------------------------------------------------|
| **os**         | the operating system       | Files, HTTP, dialogs, shell, threads, logging    |
| **model**      | nothing                    | Document tree, Markdown parser, search, settings |
| **interfaces** | model                      | Abstract service contracts                       |
| **services**   | interfaces, model, os      | Concrete services backed by the OS layer         |
| **viewmodel**  | interfaces, model          | Presentation state, commands, navigation         |
| **view**       | viewmodel, Dear ImGui      | Immediate-mode rendering and input               |

The view-models never touch the OS layer or ImGui, so they can be unit tested
with fake services.

[Back to the welcome page](welcome.md)
