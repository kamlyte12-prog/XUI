# Contributing

Thanks for your interest in Xui! Xui is experimental, so the design is still evolving. Small, focused contributions are easiest to review. For anything large (a new widget, a change to the `Platform` / `Renderer` contracts, new `.xui` / `.xss` syntax), please open an issue to discuss it first.

> 💡 Before you start, skim [`tracking.md`](tracking.md) for audit findings, design choices, status, and next steps. It shows what is already planned or known to be broken. The [architecture and backends guide](doc/architecture.md) explains how the pieces fit together.

## Ways to contribute

- **Report bugs** — see [Reporting issues](#reporting-issues).
- **Improve documentation** — the guides in [`doc/`](doc/README.md) are as important as the code.
- **Add or improve examples** — demo apps and `.xui` / `.xss` assets live in [`examples/`](examples).
- **Implement widgets or runtime features** — see [Adding or changing a widget](#adding-or-changing-a-widget).
- **Improve performance or harden parsers** — see [Performance-sensitive changes](#performance-sensitive-changes) and the fuzz harnesses in [`benchmarks/`](benchmarks).

## Code of conduct

[TODO: Add a Code of Conduct or link to one.] In the meantime, be respectful and constructive in issues and reviews.

## Reporting issues

Open an issue at <https://github.com/kamlyte12-prog/XUI/issues>. Please include:

- **What you expected** and **what happened** (copy the text of `ui.lastError()` if you have one).
- **Environment:** OS, compiler and version, CMake version, GPU/driver (for OpenGL problems).
- **How you built it:** the exact CMake configure and build commands.
- **A minimal repro:** the smallest `.xui` / `.xss` snippet that shows the problem, or the code that implements your custom `Platform` / `Renderer`.
- **Screenshots or a short GIF** for visual or input bugs.

> **Note:** Some "bugs" are known limitations: the runtime does not yet evaluate bindings, conditions, loops, component expansion, or transitions, and unsupported nodes intentionally return an error. Check [Features](README.md#-features) and [`tracking.md`](tracking.md) first.

## Setting up a development environment

Prerequisites (details in [Prerequisites](README.md#-prerequisites)): CMake **3.25+**, a **C++23** compiler (CI uses GCC 13, Clang 18, Apple Clang, and MSVC 2022), and on Linux/macOS the system OpenGL development package. GLFW 3.5.1 is bundled, so no windowing dependency to install.

### Build, test, and run

Use the same commands as [Build and test](README.md#-build-and-test):

```sh
cmake -S . -B build/debug -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=ON
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
```

The test suite is the `xui_tests` target: unit tests for core, layout, render, text, `.xui`/`.xss` parsing, and the runtime, plus golden-image comparisons against the baselines in [`tests/golden/`](tests/golden). If your change intentionally alters rendered output, update the affected baselines and say so in the PR description.

For a sanitizer build with GCC/Clang (the CI runs one on Ubuntu):

```sh
cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DXUI_SANITIZE=ON
cmake --build build/sanitize --config Debug
```

To try your change in a real window, build and run the demo:

```sh
cmake -S . -B build/glfw -DXUI_BUILD_GLFW_EXAMPLE=ON -DXUI_BUILD_TESTS=OFF -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/glfw --target xui_glfw_demo
```

### Where things live

The CMake build produces small static libraries, one per module, mirroring the source layout:

| Library | Sources | Responsibility |
| --- | --- | --- |
| `xui_core` | `src/core/`, `include/xui/core/` | Handles, arenas, interner, math, color, diagnostics |
| `xui_assets` | `src/lang/`, `src/style/` | `.xui` and `.xss` parsers/compilers |
| `xui_layout` | `src/layout/` | Layout engine |
| `xui_renderer` | `src/render/` | `Renderer` interface, `DrawList`, `Painter`, `NullRenderer`, `SoftRenderer` |
| `xui_platform` | `src/platform/` | `Platform` interface and headless null backend |
| `xui_text` | `src/text/` | TTF parsing, glyph rasterizer, atlas, wrap cache |
| `xui_widgets` | `src/ui/` | Retained runtime; also the application facade target |

Public headers are in [`include/xui/`](include/xui); start from [`include/xui/xui.hpp`](include/xui/xui.hpp).

## Pull request process

1. **Fork** the repository and create a branch from the default branch (`master`). Use a short, descriptive branch name.
2. **Make one logical change per PR.** Keep unrelated refactors and formatting changes out of it.
3. **Add or update tests** for new behavior and bug fixes.
4. **Run the full test suite** (`ctest` command above) and confirm it passes in a Debug build.
5. **Update documentation** if behavior changes (see [Documentation changes](#documentation-changes)).
6. **Open the PR** with a clear description: what changed, why, how you tested it, and which platforms you tried. Link the related issue.
7. **Respond to review feedback** by pushing follow-up commits to the same branch.

[TODO: Document commit message conventions if any — the existing history has none.]

### Continuous integration

Every push and pull request runs [`.github/workflows/ci.yml`](.github/workflows/ci.yml): a Debug build plus `ctest` on Ubuntu (GCC 13, GCC 13 with ASan + UBSan, Clang 18), macOS 14, and Windows (MSVC with `/W4 /WX`). Because `XUI_WERROR` defaults to `ON`, compiler warnings fail the build — keep your changes warning-clean.

## Code style

The existing sources are the reference. Please match them. Observed conventions:

- **Language:** C++23.
- **Namespaces:** library code lives in `namespace xui`; file-local helpers go in an anonymous namespace.
- **Formatting:** 4-space indentation, braces on the same line as the statement.
- **Types:** use the Xui fixed-width aliases (for example `xui::u32`, `xui::f64`) and `std::string_view` for non-owning string parameters.
- **Error handling:** report failures through return values (`bool` / `std::optional`) and expose messages via `lastError()`. Do not fail silently.
- **Includes:** public headers are included as `<xui/xui.hpp>`; standard headers are listed alphabetically.

[TODO: Add the formatter/linter configuration (for example `.clang-format`, `.clang-tidy`) and the command to run it, if the project adopts one.]

## Architecture rules to respect

These boundaries are core to the design. PRs that break them are likely to be sent back:

| Rule | Why |
| --- | --- |
| `Platform` and `Renderer` never parse `.xui` or `.xss`. | Backends stay replaceable and small. |
| Shared application code makes no OS or graphics-API calls. | The same `.xui` / `.xss` runs on any backend. |
| GLFW-specific calls stay in the GLFW platform adapter. UI input and layout use Xui interfaces. | Keeps the framework backend-neutral. |
| `Platform` reports top-left-origin logical coordinates, logical client size, physical framebuffer size, and DPI, and waits instead of busy-polling. | Consistent input and DPI handling across backends. |
| `Renderer` executes the backend-neutral `DrawList` and reports truthful capabilities. | Callers can rely on what a renderer says it supports. |
| Unsupported `.xui` nodes return an error instead of silently becoming native controls. | Authors get clear feedback. |

## Documentation changes

Update docs in the same PR as the code change:

- **New or changed widget, option, or style property:** update the [widget guide](doc/widgets.md).
- **Changed backend contracts or data flow:** update [`doc/architecture.md`](doc/architecture.md).
- **Changed status, limitations, or known issues:** update [`tracking.md`](tracking.md).
- **New CMake option, environment variable, or command-line flag:** update the [Configuration](README.md#-configuration) tables in the README.

## Adding or changing a widget

The runtime lives in [`src/ui/runtime.cpp`](src/ui/runtime.cpp) with its public header at [`include/xui/ui/runtime.hpp`](include/xui/ui/runtime.hpp). As a checklist:

- [ ] Support the widget in the runtime, with its layout behavior and `.xss` states (`resting`, `hover`, `focus`, `checked`, `disabled`, `pressed`) where applicable.
- [ ] If it holds state, make it readable and writable by authored id through `runtime().value(id)` / `setValue(id, value)`.
- [ ] If it changes in response to input, make sure it can notify the application through the event handler.
- [ ] Add it to the showcase (`showcase.xui` / `showcase.xss`) if it helps demonstrate the feature.
- [ ] Add tests and update [`doc/widgets.md`](doc/widgets.md).

## Performance-sensitive changes

If your change touches layout, painting, text, or rendering, build with `XUI_BUILD_BENCHMARKS=ON` and compare before and after using the `bench_style`, `bench_xui`, `bench_layout`, and `bench_paint` targets. You can also use `xui_glfw_demo --profile` (or `xui_hello --profile`) for a quick loop-throughput check.

> **Note:** `--profile` measures loop throughput, not visible presentation or input latency, so don't use it as the only evidence for latency claims.

## Getting help

- **Usage and build problems:** check the [troubleshooting guide](doc/troubleshooting.md) and the guides in [`doc/`](doc/README.md).
- **Design questions and bugs:** [open an issue](https://github.com/kamlyte12-prog/XUI/issues) describing your use case.

## Licensing of contributions

[TODO: State the license that applies to contributions, and whether a CLA or sign-off is required. The project has no LICENSE file yet — see [License](README.md#-license).]
