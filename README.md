# VibeStream

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![TUI](https://img.shields.io/badge/UI-TUI-brightgreen)](https://github.com/ArthurSonzogni/FTXUI)

**VibeStream** is a feature‑rich terminal‑based music player and downloader.  
Play local audio, manage playlists, and download tracks directly from YouTube and other platforms – all from the comfort of your terminal.

## Features

- 🎵 **Play local music** (MP3, FLAC, WAV, OGG)
- 📥 **Download audio** from YouTube, SoundCloud, etc. (via yt‑dlp)
- 🖥️ **Modern TUI** with three‑pane layout, progress bars, and cover art preview
- ⌨️ **Vim‑inspired key bindings** – fast, keyboard‑driven workflow
- 📚 **Library management** with SQLite database
- 🔁 **Playlists**, shuffle, repeat, volume control, seeking
- ⚙️ **Highly configurable** via JSON file
- 🐧 **Cross‑platform** – Linux, macOS, Windows (WSL/MSYS2)

## Quick Start

### Prerequisites

- C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022)
- CMake 3.15+
- [yt‑dlp](https://github.com/yt-dlp/yt-dlp) (for downloads)
- [ffmpeg](https://ffmpeg.org/) (for audio conversion)

### Build from Source

```bash
git clone https://github.com/AridsWolfgang/VibeStream.git
cd VibeStream
mkdir build && cd build
cmake ..
make
sudo make install   # optional
