# Third‑Party Licenses

VibeStream depends on the following open‑source libraries and tools:

| Library | Version | License | Copyright |
|---------|---------|---------|-----------|
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | latest | MIT | Arthur Sonzogni |
| [miniaudio](https://miniaud.io/) | 0.11+ | Public Domain (Unlicense) | David Reid |
| [TagLib](https://taglib.org/) | 1.13+ | LGPL 2.1 / MPL 1.1 | The TagLib authors |
| [SQLite3](https://sqlite.org/) | 3.x | Public Domain | D. Richard Hipp |
| [yt‑dlp](https://github.com/yt-dlp/yt-dlp) | latest | Unlicense | yt‑dlp contributors |
| [ffmpeg](https://ffmpeg.org/) | 5.x+ | LGPL 2.1 (or GPL, if configured) | FFmpeg team |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11+ | MIT | Niels Lohmann |
| [spdlog](https://github.com/gabime/spdlog) | 1.12+ | MIT | Gabi Melman |

Full license texts can be found in the `LICENSES/` directory of this repository.

## License Compatibility

All dependencies are compatible with the MIT license of VibeStream.  
The LGPL dependencies are used via dynamic linking and do not affect the overall project license.

## How to Update

If you add a new dependency, please append a row to this table and include the
license text in `LICENSES/<spdx‑id>.txt`.
