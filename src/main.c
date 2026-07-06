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

static void get_db_path(char *buf, size_t len) {
  const char *xdg = getenv("XDG_DATA_HOME");
  if (xdg && *xdg) {
    snprintf(buf, len, "%s/vibestream/library.db", xdg);
  } else {
    const char *home = getenv("HOME");
    if (!home) {
      struct passwd *pw = getpwuid(getuid());
      home = pw ? pw->pw_dir : ".";
    }
    snprintf(buf, len, "%s/.local/share/vibestream/library.db", home);
  }
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "%s", buf);
  char *slash = strrchr(tmp, '/');
  if (slash) {
    *slash = '\0';
    char mk[1024];
    snprintf(mk, sizeof(mk), "mkdir -p %s", tmp);
    system(mk);
  }
}

int main(int argc, char **argv) {
  vs_config cfg;
  config_load(&cfg);

  if (argc > 1) {
    if (strcmp(argv[1], "--rescan") == 0 || strcmp(argv[1], "-r") == 0) {
      snprintf(cfg.music_dir, sizeof(cfg.music_dir), "%s", argv[2] ? argv[2] : cfg.music_dir);
    }
  }

  vs_player *player = player_create();
  if (!player) {
    fprintf(stderr, "Failed to initialize audio engine\n");
    return 1;
  }

  char db_path[1024];
  get_db_path(db_path, sizeof(db_path));
  vs_library *lib = library_open(db_path);
  if (!lib) {
    fprintf(stderr, "Failed to open library database\n");
    player_destroy(player);
    return 1;
  }

  if (argc > 1 && (strcmp(argv[1], "--rescan") == 0 || strcmp(argv[1], "-r") == 0)) {
    printf("Scanning %s for music...\n", cfg.music_dir);
    library_scan(lib, cfg.music_dir, 1);
    printf("Scan complete.\n");
  }

  vs_downloader *dl = downloader_create(cfg.download_dir);
  if (!dl) {
    fprintf(stderr, "Failed to create downloader\n");
    library_close(lib);
    player_destroy(player);
    return 1;
  }

  vs_ui *ui = ui_create(&cfg, player, lib, dl);
  if (!ui) {
    fprintf(stderr, "Failed to create UI\n");
    downloader_destroy(dl);
    library_close(lib);
    player_destroy(player);
    return 1;
  }

  int rc = ui_run(ui);
  ui_destroy(ui);
  downloader_destroy(dl);
  library_close(lib);
  player_destroy(player);
  return rc;
}
