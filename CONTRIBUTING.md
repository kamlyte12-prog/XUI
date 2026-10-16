# Contributing

Thanks for your interest in Xui! Xui is experimental, so the design is still evolving. Small, focused contributions are easiest to review. For anything large (a new widget, a change to the `Platform` / `Renderer` contracts, new `.xui` / `.xss` syntax), please open an issue to discuss it first.

> 💡 Agents: read [`AGENTS.md`](AGENTS.md) and [`plan/agents.md`](plan/agents.md) first. Check the [current roadmap](plan/plan.md) and [change history](plan/tracking.md) before changing code. The [architecture and backends guide](doc/architecture.md) explains the current public contracts.

## Ways to contribute

- **Report bugs** — see [Reporting issues](#reporting-issues).
- **Improve documentation** — the guides in [`doc/`](doc/README.md) are as important as the code.
- **Add or improve examples** — demo apps and `.xui` / `.xss` assets live in [`examples/`](examples).
- **Implement widgets or runtime features** — follow [the widget implementation checklist](plan/conventions.md#adding-a-widget-end-to-end).
- **Improve performance or harden parsers** — read the [performance guide](doc/performance.md) and use the fuzz harnesses in [`benchmarks/`](benchmarks).

## Code of conduct

[TODO: Add a Code of Conduct or link to one.] In the meantime, be respectful and constructive in issues and reviews.

## Reporting issues

Open an issue at <https://github.com/kamlyte12-prog/XUI/issues>. Please include:

- **What you expected** and **what happened** (copy the text of `ui.lastError()` if you have one).
- **Environment:** OS, compiler and version, CMake version, GPU/driver (for OpenGL problems).
- **How you built it:** the exact CMake configure and build commands.
- **A minimal repro:** the smallest `.xui` / `.xss` snippet that shows the problem, or the code that implements your custom `Platform` / `Renderer`.
- **Screenshots or a short GIF** for visual or input bugs.

> **Note:** Some apparent bugs are known limitations. The runtime does not evaluate bindings, conditions, loops, component expansion, or transitions, and unsupported nodes intentionally return an error. Check the [widget reference](doc/widgets.md) and [current roadmap](plan/plan.md) first.

## Setting up a development environment

Prerequisites and platform dependencies are listed in the [getting-started guide](doc/getting-started.md#build-xui). Xui uses CMake **3.25+** and C++23; GLFW 3.5.1 is bundled.

### Build, test, and run

Use the canonical [build and test instructions](doc/getting-started.md#build-xui). The `xui_tests` suite covers core, layout, render, text, `.xui`/`.xss` parsing, runtime behavior, and golden-image comparisons in [`tests/golden/`](tests/golden). If a change intentionally alters rendered output, update the affected baselines and mention it in the PR description. The getting-started guide also documents the GLFW target and its platform dependencies; CI includes a GCC ASan/UBSan configuration.

The agent [architecture map](plan/architecture.md) maps source modules to responsibilities. Public headers are in [`include/xui/`](include/xui); start from [`include/xui/xui.hpp`](include/xui/xui.hpp).

## Pull request process

1. **Fork** the repository and create a branch from the default branch (`master`). Use a short, descriptive branch name.
2. **Make one logical change per PR.** Keep unrelated refactors and formatting changes out of it.
3. **Add or update tests** for new behavior and bug fixes.
4. **Run the full test suite** (`ctest` command above) and confirm it passes in a Debug build.
5. **Update documentation** if behavior changes (see [the documentation update rules](plan/agents.md#update-after-each-change)).
6. **Open the PR** with a clear description: what changed, why, how you tested it, and which platforms you tried. Link the related issue.
7. **Respond to review feedback** by pushing follow-up commits to the same branch.

### Continuous integration

Every push and pull request runs [`.github/workflows/ci.yml`](.github/workflows/ci.yml): a Debug build plus `ctest` on Ubuntu (GCC 13, GCC 13 with ASan + UBSan, Clang 18), macOS 14, and Windows (MSVC with `/W4 /WX`). Because `XUI_WERROR` defaults to `ON`, compiler warnings fail the build — keep your changes warning-clean.

## Technical contribution references

- Follow the project invariants and regression checklist in [`plan/agents.md`](plan/agents.md), and code/widget/style conventions in [`plan/conventions.md`](plan/conventions.md).
- Update user behavior documentation in [`doc/`](doc/README.md), especially the [widget guide](doc/widgets.md), [style reference](doc/styling-reference.md), and [architecture guide](doc/architecture.md).
- Record current status and decisions in [`plan/plan.md`](plan/plan.md) and [`plan/tracking.md`](plan/tracking.md). CMake options, demo flags, and setup belong in the [getting-started guide](doc/getting-started.md#configuration).
- For layout, paint, text, or rendering changes, compare the relevant benchmarks and use `xui_glfw_demo --profile` only as a loop-throughput measurement; it does not prove visible presentation or input latency. See the [performance guide](doc/performance.md).

## Getting help

- **Usage and build problems:** check the [troubleshooting guide](doc/troubleshooting.md) and the guides in [`doc/`](doc/README.md).
- **Design questions and bugs:** [open an issue](https://github.com/kamlyte12-prog/XUI/issues) describing your use case.

## Licensing of contributions

The repository is licensed under the [MIT License](LICENSE.md). This repository does not currently document a separate CLA or sign-off requirement. The bundled GLFW dependency retains its own license at [`examples/lib/glfw-3.5.1/LICENSE.md`](examples/lib/glfw-3.5.1/LICENSE.md).
