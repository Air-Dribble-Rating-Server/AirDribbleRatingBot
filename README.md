# AirDribbleRatingBot

A modern Discord bot written in **C++23** using [D++](https://github.com/brainboxdotcc/DPP) (v10.1.5).

It is designed for the Air Dribble Rating discord server and implements a structured ranking and challenge system.

## 🚀 Features

- Challenge-based ranking system
- Role management based on rating
- Moderation workflow via submissions
- Modular command architecture
- Linux, Debian (Makefile) + Windows (CMake) support

## Prerequisites

- Git – to clone the repository.
- A C++23‑compatible compiler:
  - Linux: GCC 14 or newer.
  - Windows: Visual Studio 2022/2026 (MSVC) with C++23 support.
- CMake (version ≥ 3.20) – only required on Windows.
- pkg-config – usually already present on Linux.

## ⚙️ Setup

```bash
git clone https://github.com/yourusername/AirDribbleRatingBot.git
cd AirDribbleRatingBot
```

## Create config

```bash
cp config.example.json config.json
```

Edit token.

## Build and run

### On Linux, Debian (GCC)

No needed instalations. The Makefile provides four build modes. All binaries are placed inside build/.


| Command                   | What it does                                                             |
| ------------------------- | ------------------------------------------------------------------------ |
| `make` / `make debug-run` | Compile in debug mode (-g -O0 -march=native + warnings) and run the bot. |
| `make compile`            | Only compile in debug mode (no execution).                               |
| `make -j$(nproc) release` | Compile in release mode (-O3 -flto=jobserver, portable) and run the bot. |
| `make compile-release`    | Only compile in release mode (no execution).                             |
| `make clean`              | Remove build/ directory.                                                 |

#### Examples:

```bash
# Quick test (debug)
make # Compiles and runs in debug mode

# Full optimization, ready to distribute (static binary)
make -j$(nproc) release # Compiles and runs in release mode using parallel compilation
```

### On Windows (MSVC)

No needed instalations.

Open the project folder on Visual Studio 2022/2026 and create a `launch.vs.json` file inside of the folder `.vs`. Copy this inside the file:

````json
{
  "version": "0.2.1",
  "configurations": [
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "AirDribbleRatingBot.exe (debug\\AirDribbleRatingBot.exe)",
      "name": "AirDribbleRatingBot.exe (debug\\AirDribbleRatingBot.exe)",
      "currentDir": "${workspaceRoot}"
    },
    {
      "type": "default",
      "project": "CMakeLists.txt",
      "projectTarget": "AirDribbleRatingBot.exe (release\\AirDribbleRatingBot.exe)",
      "name": "AirDribbleRatingBot.exe (release\\AirDribbleRatingBot.exe)",
      "currentDir": "${workspaceRoot}"
    }
  ]
}
````

Compile the CMakeLists.txt and you will be ready to 

## Documentation

- Commands: docs/commands.md
- Contributing: docs/contributing.md
- Roadmap: docs/roadmap.md
- Architecture: docs/architecture.md

## ⚠️ Note

This bot is tailored for a specific Discord server.
Commands and behavior are not generic.

## 📄 License

This project is licensed under the MIT License – see the LICENSE file for details.
