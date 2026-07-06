#ifndef VIBESTREAM_UI_H
#define VIBESTREAM_UI_H

#include "types.h"
#include "player.h"
#include "library.h"
#include "downloader.h"
#include "config.h"

typedef struct vs_ui vs_ui;

vs_ui *ui_create(vs_config *cfg, vs_player *player, vs_library *lib, vs_downloader *dl);
void ui_destroy(vs_ui *ui);
int ui_run(vs_ui *ui);
void ui_refresh(vs_ui *ui);

#endif
