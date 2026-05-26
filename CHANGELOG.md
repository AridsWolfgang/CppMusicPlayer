# Changelog

All notable changes to VibeStream will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial project scaffolding
- TUI with three‑pane layout (FTXUI)
- Audio playback engine (miniaudio)
- SQLite library and playlist management
- Downloader integration with yt‑dlp
- Vim‑style key bindings
- Configuration file support (JSON)

### Security
- Command injection protection for yt‑dlp invocations
- Path traversal checks in library scanner

## [1.0.0] - YYYY-MM-DD (planned)

### Added
- First stable release
- Full TUI with help screen
- Download queue with progress bars
- MPRIS support on Linux
- Windows MSYS2 build support
