#ifndef VIBESTREAM_PLAYER_H
#define VIBESTREAM_PLAYER_H

#include "types.h"
#include <stdbool.h>

typedef struct vs_player vs_player;

vs_player *player_create(void);
void player_destroy(vs_player *p);

int  player_play(vs_player *p, const char *path);
int  player_toggle(vs_player *p);
int  player_stop(vs_player *p);
int  player_seek(vs_player *p, double seconds);
int  player_seek_relative(vs_player *p, double delta);

double player_volume_get(vs_player *p);
void   player_volume_set(vs_player *p, double vol);
double player_position_get(vs_player *p);
double player_duration_get(vs_player *p);
vs_player_state player_state_get(vs_player *p);

double player_crossfade_get(vs_player *p);
void   player_crossfade_set(vs_player *p, double seconds);
int    player_start_crossfade(vs_player *p, const char *next_path);

#endif
