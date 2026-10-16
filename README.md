# Xui

> An experimental C++23 GUI framework: author structure in `.xui`, style it in `.xss`, and supply replaceable platform and rendering backends.

[![CI](https://github.com/kamlyte12-prog/XUI/actions/workflows/ci.yml/badge.svg)](https://github.com/kamlyte12-prog/XUI/actions/workflows/ci.yml)
![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C)
![CMake](https://img.shields.io/badge/CMake-3.25%2B-064F8C)

Xui keeps layout, styling, and platform rendering separate. The recommended `xui::Xui` facade loads `.xui`, `.xss`, and fonts; `xui::Platform` handles the native window and normalized input; `xui::Renderer` presents the backend-neutral draw list. The current interactive example uses GLFW and OpenGL. See the [architecture guide](doc/architecture.md) for the actual backend support and limitations.

## Start here

- New users: [Getting started](doc/getting-started.md)
- `.xui` layout and structure: [Xui language guide](doc/xui-guide.md)
- `.xss` styling: [Xss styling guide](doc/xss-guide.md) and [property reference](doc/styling-reference.md)
- Controls and supported features: [UI object reference](doc/widgets.md)
- Documentation index: [`doc/README.md`](doc/README.md)
- Agent instructions: [`AGENTS.md`](AGENTS.md) → [`plan/agents.md`](plan/agents.md)
- Project roadmap and status: [`plan/plan.md`](plan/plan.md)
- Design and change history: [`plan/architecture.md`](plan/architecture.md) and [`plan/tracking.md`](plan/tracking.md)

## 📖 Table of Contents

- [What is this?](#-what-is-this)
- [Features](#-features)
- [Demo](#-demo)
- [Prerequisites](#-prerequisites)
- [Installation](#-installation)
- [Quick start](#-quick-start)
- [Usage](#-usage)
- [Configuration](#-configuration)
- [Project structure](#-project-structure)
- [Build and test](#-build-and-test)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [License](#-license)
- [FAQ / Troubleshooting](#faq)

## 💡 What is this?

A declarative C++ UI framework with a retained runtime. `.xui` describes widget trees and layout intent; `.xss` describes appearance and interaction states. Backends implement `Renderer` and `Platform`; shared app code does not call OS or graphics APIs directly.

## ✨ Features

The runtime-supported widget list, styling capabilities, and explicit limitations are maintained in the [widget reference](doc/widgets.md) and [styling reference](doc/styling-reference.md). Parser/schema recognition alone does not mean a widget is implemented.

## 🖼️ Demo

The GLFW/OpenGL showcase is defined in [`examples/showcase.xui`](examples/showcase.xui) and [`examples/showcase.xss`](examples/showcase.xss). Its screenshot is [here](doc/showcase.png). Build and launch instructions, including the Windows `xui_hello` target and profile mode, are in [Getting started](doc/getting-started.md#build-xui).

## 📋 Prerequisites

CMake 3.25+, a C++23 compiler, and OpenGL 2.1 for the bundled GLFW example. See the [full platform dependency list](doc/getting-started.md#build-xui).

## 📦 Installation

Xui currently builds from source and does not provide an installed CMake package. See [Getting started](doc/getting-started.md#build-xui) for CMake integration and build commands.

## 🚀 Quick start

The recommended path is `xui::Xui`: implement or choose a `Renderer` and `Platform`, load the font and `.xui`/`.xss` files, call `attach()`, then `run()`. Follow the [minimal application guide](doc/getting-started.md#minimal-application).

## 🧰 Usage

### Authoring `.xui` and `.xss`

Use the [`.xui` guide](doc/xui-guide.md) for structure/layout and the [`.xss` guide](doc/xss-guide.md) for selectors, states, themes, and cascading.

### Using Xui in your C++ app

The [getting-started guide](doc/getting-started.md#minimal-application) has the facade example and asset/font loading rules.

### Custom backends

Implement [`xui::Renderer`](doc/architecture.md) and [`xui::Platform`](doc/architecture.md). The [backend guide](doc/architecture.md) documents their contracts and lifecycle.

### Before and after the facade

The facade and lower-level lifecycle are described in the [Getting started guide](doc/getting-started.md) and [Architecture guide](doc/architecture.md).

## ⚙️ Configuration

`XuiOptions`, `XuiRunOptions`, CMake switches, demo flags, and environment variables are listed in [Getting started: Configuration](doc/getting-started.md#configuration).

## 🗂️ Project structure

The [architecture source map](plan/architecture.md) describes module responsibilities. User documentation is indexed in [`doc/`](doc/README.md); agent context is indexed in [`plan/`](plan/README.md).

## 🧪 Build and test

Use the [build/test commands](doc/getting-started.md#build-xui) and [`CONTRIBUTING.md`](CONTRIBUTING.md). CI configuration is in [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## 📚 Documentation

Start at [`doc/README.md`](doc/README.md) for user guides or [`plan/README.md`](plan/README.md) for agent-facing project context.

## 🤝 Contributing

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) for issue, development, and pull-request workflow. Agents should begin with [`AGENTS.md`](AGENTS.md).

## 📄 License

Xui is distributed under the [MIT License](LICENSE.md). Bundled GLFW keeps its own [license](examples/lib/glfw-3.5.1/LICENSE.md).

<a id="faq"></a>
## ❓ FAQ / Troubleshooting

Check the [troubleshooting guide](doc/troubleshooting.md) for setup, text, styling, layout, and responsiveness problems.
