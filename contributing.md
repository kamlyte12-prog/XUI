## 🤝 Contributing

Thanks for your interest in Xui! Xui is experimental, so the design is still evolving. Small, focused contributions are easiest to review. For anything large (a new widget, a change to the `Platform` / `Renderer` contracts, new `.xui` / `.xss` syntax), please open an issue to discuss it first.

> 💡 Before you start, skim [`tracking.md`](tracking.md) for audit findings, design choices, status, and next steps. It shows what is already planned or known to be broken. The [`architecture.md`](architecture.md) notes explain how the pieces fit together.

### Reporting issues

Open an issue at [TODO: issue tracker URL]. Please include:

- **What you expected** and **what happened** (copy the text of `ui.lastError()` if you have one).
- **Environment:** OS, compiler and version, CMake version, GPU/driver (for OpenGL problems).
- **How you built it:** the exact CMake configure and build commands.
- **A minimal repro:** the smallest `.xui` / `.xss` snippet that shows the problem, or the code that implements your custom `Platform` / `Renderer`.
- **Screenshots or a short GIF** for visual or input bugs.

> **Note:** Some "bugs" are known limitations: the runtime does not yet evaluate bindings, conditions, loops, component expansion, or transitions, and unsupported nodes intentionally return an error. Check [Features](#-features) and [`tracking.md`](tracking.md) first.

### Setting up a development build

Use the same commands as [Build and test](#-build-and-test):

```sh
cmake -S . -B build/debug -DXUI_BUILD_TESTS=ON -DXUI_BUILD_BENCHMARKS=ON
cmake --build build/debug --config Debug
ctest --test-dir build/debug -C Debug --output-on-failure
```

To try your change in a real window, build and run the demo:

```sh
cmake -S . -B build/glfw -DXUI_BUILD_GLFW_EXAMPLE=ON -DXUI_BUILD_TESTS=OFF -DXUI_BUILD_BENCHMARKS=OFF
cmake --build build/glfw --target xui_glfw_demo
```

### Pull request process

1. **Fork** the repository and create a branch from the default branch ([TODO: confirm branch name and any naming convention]).
2. **Make one logical change per PR.** Keep unrelated refactors and formatting changes out of it.
3. **Add or update tests** for new behavior and bug fixes.
4. **Run the full test suite** (`ctest` command above) and confirm it passes in a Debug build.
5. **Update documentation** if behavior changes (see [Documentation changes](#documentation-changes)).
6. **Open the PR** with a clear description: what changed, why, how you tested it, and which platforms you tried. Link the related issue.
7. **Respond to review feedback** by pushing follow-up commits to the same branch.

[TODO: Document commit message conventions, required CI checks, and review/merge policy, if any.]

### Code style

The existing sources are the reference. Please match them. Observed conventions:

- **Language:** C++23.
- **Namespaces:** library code lives in `namespace xui`; file-local helpers go in an anonymous namespace.
- **Formatting:** 4-space indentation, braces on the same line as the statement.
- **Types:** use the Xui fixed-width aliases (for example `xui::u32`, `xui::f64`) and `std::string_view` for non-owning string parameters.
- **Error handling:** report failures through return values (`bool` / `std::optional`) and expose messages via `lastError()`. Do not fail silently.
- **Includes:** public headers are included as `<xui/xui.hpp>`; standard headers are listed alphabetically.

[TODO: Add the formatter/linter configuration (for example `.clang-format`, `.clang-tidy`) and the command to run it, if the project uses one.]

### Architecture rules to respect

These boundaries are core to the design. PRs that break them are likely to be sent back:

| Rule | Why |
| --- | --- |
| `Platform` and `Renderer` never parse `.xui` or `.xss`. | Backends stay replaceable and small. |
| Shared application code makes no OS or graphics-API calls. | The same `.xui` / `.xss` runs on any backend. |
| GLFW-specific calls stay in the GLFW platform adapter. UI input and layout use Xui interfaces. | Keeps the framework backend-neutral. |
| `Platform` reports top-left-origin logical coordinates, logical client size, physical framebuffer size, and DPI, and waits instead of busy-polling. | Consistent input and DPI handling across backends. |
| `Renderer` executes the backend-neutral `DrawList` and reports truthful capabilities. | Callers can rely on what a renderer says it supports. |
| Unsupported `.xui` nodes return an error instead of silently becoming native controls. | Authors get clear feedback. |

### Documentation changes

Update docs in the same PR as the code change:

- **New or changed widget, option, or style property:** update the [widget guide](doc/widgets.md).
- **Changed backend contracts or data flow:** update [`doc/architecture.md`](doc/architecture.md) and, if relevant, the root [`architecture.md`](architecture.md).
- **Changed status, limitations, or known issues:** update [`tracking.md`](tracking.md).
- **New CMake option, environment variable, or command-line flag:** update the [Configuration](#-configuration) tables in this README.

### Adding or changing a widget

As a checklist (adapt to the actual code layout: [TODO: point to the runtime/widget source directory]):

- [ ] Support the widget in the runtime, with its layout behavior and `.xss` states (`resting`, `hover`, `focus`, `checked`, `disabled`, `pressed`) where applicable.
- [ ] If it holds state, make it readable and writable by authored id through `runtime().value(id)` / `setValue(id, value)`.
- [ ] If it changes in response to input, make sure it can notify the application through the event handler.
- [ ] Add it to the showcase (`showcase.xui` / `showcase.xss`) if it helps demonstrate the feature.
- [ ] Add tests and update [`doc/widgets.md`](doc/widgets.md).

### Performance-sensitive changes

If your change touches layout, painting, text, or rendering, build with `XUI_BUILD_BENCHMARKS=ON` and compare before and after. You can also use `xui_glfw_demo --profile` (or `xui_hello --profile`) for a quick loop-throughput check.

> **Note:** `--profile` measures loop throughput, not visible presentation or input latency, so don't use it as the only evidence for latency claims.

### Code of conduct

[TODO: Add a Code of Conduct or link to one. Be respectful and constructive in issues and reviews.]

### Licensing of contributions

[TODO: State the license that applies to contributions, and whether a CLA or sign-off is required. See [License](#-license).]