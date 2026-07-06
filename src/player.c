#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "vibestream/player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct vs_player {
  ma_engine engine;
  ma_sound sound;
  int sound_loaded;
  vs_player_state state;
  double volume;
  char current_path[VS_PATH_MAX];
  pthread_mutex_t mutex;
};

vs_player *player_create(void) {
  vs_player *p = calloc(1, sizeof(vs_player));
  if (!p) return NULL;
  pthread_mutex_init(&p->mutex, NULL);
  if (ma_engine_init(NULL, &p->engine) != MA_SUCCESS) {
    free(p);
    return NULL;
  }
  p->volume = 0.75;
  p->state = VS_STOPPED;
  p->sound_loaded = 0;
  ma_engine_set_volume(&p->engine, (float)p->volume);
  return p;
}

void player_destroy(vs_player *p) {
  if (!p) return;
  pthread_mutex_lock(&p->mutex);
  if (p->sound_loaded) ma_sound_uninit(&p->sound);
  ma_engine_uninit(&p->engine);
  pthread_mutex_unlock(&p->mutex);
  pthread_mutex_destroy(&p->mutex);
  free(p);
}

int player_play(vs_player *p, const char *path) {
  pthread_mutex_lock(&p->mutex);
  if (p->sound_loaded) {
    ma_sound_stop(&p->sound);
    ma_sound_uninit(&p->sound);
    p->sound_loaded = 0;
  }
  ma_result result = ma_sound_init_from_file(&p->engine, path, 0, NULL, NULL, &p->sound);
  if (result != MA_SUCCESS) {
    pthread_mutex_unlock(&p->mutex);
    return -1;
  }
  p->sound_loaded = 1;
  strncpy(p->current_path, path, VS_PATH_MAX - 1);
  ma_sound_start(&p->sound);
  p->state = VS_PLAYING;
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_toggle(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  if (!p->sound_loaded) { pthread_mutex_unlock(&p->mutex); return -1; }
  if (p->state == VS_PLAYING) {
    ma_sound_stop(&p->sound);
    p->state = VS_PAUSED;
  } else {
    ma_sound_start(&p->sound);
    p->state = VS_PLAYING;
  }
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_stop(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  if (p->sound_loaded) {
    ma_sound_stop(&p->sound);
    ma_sound_seek_to_pcm_frame(&p->sound, 0);
  }
  p->state = VS_STOPPED;
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_seek(vs_player *p, double seconds) {
  pthread_mutex_lock(&p->mutex);
  if (!p->sound_loaded) { pthread_mutex_unlock(&p->mutex); return -1; }
  ma_sound_seek_to_pcm_frame(&p->sound, (ma_uint64)(seconds * ma_engine_get_sample_rate(&p->engine)));
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_seek_relative(vs_player *p, double delta) {
  double pos = player_position_get(p) + delta;
  if (pos < 0) pos = 0;
  return player_seek(p, pos);
}

double player_volume_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double v = p->volume;
  pthread_mutex_unlock(&p->mutex);
  return v;
}

void player_volume_set(vs_player *p, double vol) {
  if (vol < 0) vol = 0;
  if (vol > 1) vol = 1;
  pthread_mutex_lock(&p->mutex);
  p->volume = vol;
  ma_engine_set_volume(&p->engine, (float)vol);
  pthread_mutex_unlock(&p->mutex);
}

double player_position_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double pos = 0;
  if (p->sound_loaded) {
    ma_uint64 frame;
    ma_sound_get_cursor_in_pcm_frames(&p->sound, &frame);
    pos = (double)frame / ma_engine_get_sample_rate(&p->engine);
  }
  pthread_mutex_unlock(&p->mutex);
  return pos;
}

double player_duration_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double dur = 0;
  if (p->sound_loaded) {
    ma_uint64 frames;
    ma_sound_get_length_in_pcm_frames(&p->sound, &frames);
    dur = (double)frames / ma_engine_get_sample_rate(&p->engine);
  }
  pthread_mutex_unlock(&p->mutex);
  return dur;
}

vs_player_state player_state_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  vs_player_state s = p->state;
  pthread_mutex_unlock(&p->mutex);
  return s;
}
