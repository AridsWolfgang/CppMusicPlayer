#include "vibestream/config.h"
#include "vibestream/player.h"
#include "vibestream/library.h"
#include "vibestream/downloader.h"
#include "vibestream/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

static void get_db_path(char *buf, size_t len) {
  const char *xdg = getenv("XDG_DATA_HOME");
  if (xdg && *xdg) {
    snprintf(buf, len, "%s/vibestream/library.db", xdg);
  } else {
    const char *home = getenv("HOME");
    if (!home) { struct passwd *pw = getpwuid(getuid()); home = pw ? pw->pw_dir : "."; }
    snprintf(buf, len, "%s/.local/share/vibestream/library.db", home);
  }
  char tmp[1024]; snprintf(tmp, sizeof(tmp), "%s", buf);
  char *slash = strrchr(tmp, '/');
  if (slash) { *slash = '\0'; char mk[1024]; snprintf(mk, sizeof(mk), "mkdir -p %s", tmp); system(mk); }
}

int main(int argc, char **argv) {
  vs_config cfg;
  config_load(&cfg);

  int rescan = 0;
  int update_meta = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--rescan") == 0 || strcmp(argv[i], "-r") == 0) {
      rescan = 1;
      if (i + 1 < argc && argv[i+1][0] != '-') {
        snprintf(cfg.music_dir, sizeof(cfg.music_dir), "%s", argv[++i]);
      }
    } else if (strcmp(argv[i], "--metadata") == 0 || strcmp(argv[i], "-m") == 0) {
      update_meta = 1;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("VibeStream v1.0 - Terminal Music Player\n");
      printf("Usage: vibestream [options]\n");
      printf("Options:\n");
      printf("  -r, --rescan [dir]  Scan music directory into library\n");
      printf("  -m, --metadata      Update metadata for all songs\n");
      printf("  -h, --help          Show this help\n");
      return 0;
    }
  }

  vs_player *player = player_create();
  if (!player) { fprintf(stderr, "Failed to init audio engine\n"); return 1; }

  char db_path[1024];
  get_db_path(db_path, sizeof(db_path));
  vs_library *lib = library_open(db_path);
  if (!lib) { fprintf(stderr, "Failed to open library\n"); player_destroy(player); return 1; }

  if (rescan) {
    printf("Scanning %s...\n", cfg.music_dir);
    library_scan(lib, cfg.music_dir, 1);
    printf("Scan complete.\n");
  }
  if (update_meta) {
    printf("Updating metadata...\n");
    library_update_metadata(lib);
    printf("Metadata update complete.\n");
  }
  if (rescan || update_meta) {
    library_close(lib);
    player_destroy(player);
    return 0;
  }

  vs_downloader *dl = downloader_create(cfg.download_dir);
  if (!dl) { fprintf(stderr, "Failed to create downloader\n"); library_close(lib); player_destroy(player); return 1; }

  player_crossfade_set(player, cfg.crossfade);

  vs_ui *ui = ui_create(&cfg, player, lib, dl);
  if (!ui) { fprintf(stderr, "Failed to create UI\n"); downloader_destroy(dl); library_close(lib); player_destroy(player); return 1; }

  int rc = ui_run(ui);
  ui_destroy(ui);
  downloader_destroy(dl);
  library_close(lib);
  player_destroy(player);
  return rc;
}
