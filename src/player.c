#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "vibestream/player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct vs_player {
  ma_engine engine;
  ma_sound current;
  ma_sound next;
  int current_loaded;
  int next_loaded;
  int crossfading;
  vs_player_state state;
  double volume;
  double crossfade;
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
  p->crossfade = 2.0;
  p->state = VS_STOPPED;
  return p;
}

void player_destroy(vs_player *p) {
  if (!p) return;
  pthread_mutex_lock(&p->mutex);
  if (p->current_loaded) ma_sound_uninit(&p->current);
  if (p->next_loaded) ma_sound_uninit(&p->next);
  ma_engine_uninit(&p->engine);
  pthread_mutex_unlock(&p->mutex);
  pthread_mutex_destroy(&p->mutex);
  free(p);
}

int player_play(vs_player *p, const char *path) {
  pthread_mutex_lock(&p->mutex);
  if (p->next_loaded) {
    ma_sound_stop(&p->next);
    ma_sound_uninit(&p->next);
    p->next_loaded = 0;
  }
  if (p->current_loaded) {
    ma_sound_stop(&p->current);
    ma_sound_uninit(&p->current);
    p->current_loaded = 0;
  }
  ma_result result = ma_sound_init_from_file(&p->engine, path, 0, NULL, NULL, &p->current);
  if (result != MA_SUCCESS) {
    pthread_mutex_unlock(&p->mutex);
    return -1;
  }
  p->current_loaded = 1;
  p->crossfading = 0;
  strncpy(p->current_path, path, VS_PATH_MAX - 1);
  ma_sound_set_volume(&p->current, (float)p->volume);
  ma_sound_start(&p->current);
  p->state = VS_PLAYING;
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_start_crossfade(vs_player *p, const char *next_path) {
  pthread_mutex_lock(&p->mutex);
  if (!p->current_loaded) {
    pthread_mutex_unlock(&p->mutex);
    return player_play(p, next_path);
  }
  if (p->next_loaded) {
    ma_sound_stop(&p->next);
    ma_sound_uninit(&p->next);
    p->next_loaded = 0;
  }
  ma_result result = ma_sound_init_from_file(&p->engine, next_path, 0, NULL, NULL, &p->next);
  if (result != MA_SUCCESS) {
    pthread_mutex_unlock(&p->mutex);
    return -1;
  }
  p->next_loaded = 1;
  int dur_ms = (int)(p->crossfade * 1000);
  if (dur_ms < 1) dur_ms = 1;
  ma_sound_set_volume(&p->next, 0);
  ma_sound_start(&p->next);
  ma_sound_set_fade_in_milliseconds(&p->current, (float)p->volume, 0, (ma_uint64)dur_ms);
  ma_sound_set_fade_in_milliseconds(&p->next, 0, (float)p->volume, (ma_uint64)dur_ms);
  p->crossfading = 1;
  strncpy(p->current_path, next_path, VS_PATH_MAX - 1);
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_toggle(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  if (!p->current_loaded) { pthread_mutex_unlock(&p->mutex); return -1; }
  if (p->state == VS_PLAYING) {
    ma_sound_stop(&p->current);
    if (p->next_loaded) ma_sound_stop(&p->next);
    p->state = VS_PAUSED;
  } else {
    ma_sound_start(&p->current);
    if (p->next_loaded) ma_sound_start(&p->next);
    p->state = VS_PLAYING;
  }
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_stop(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  if (p->current_loaded) { ma_sound_stop(&p->current); ma_sound_seek_to_pcm_frame(&p->current, 0); }
  if (p->next_loaded) { ma_sound_stop(&p->next); ma_sound_uninit(&p->next); p->next_loaded = 0; }
  p->state = VS_STOPPED;
  p->crossfading = 0;
  pthread_mutex_unlock(&p->mutex);
  return 0;
}

int player_seek(vs_player *p, double seconds) {
  pthread_mutex_lock(&p->mutex);
  if (!p->current_loaded) { pthread_mutex_unlock(&p->mutex); return -1; }
  ma_sound_seek_to_pcm_frame(&p->current, (ma_uint64)(seconds * ma_engine_get_sample_rate(&p->engine)));
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
  if (vol < 0) vol = 0; if (vol > 1) vol = 1;
  pthread_mutex_lock(&p->mutex);
  p->volume = vol;
  if (p->current_loaded) ma_sound_set_volume(&p->current, (float)vol);
  pthread_mutex_unlock(&p->mutex);
}

double player_position_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double pos = 0;
  if (p->current_loaded) {
    ma_uint64 frame;
    ma_sound_get_cursor_in_pcm_frames(&p->current, &frame);
    pos = (double)frame / ma_engine_get_sample_rate(&p->engine);
  }
  pthread_mutex_unlock(&p->mutex);
  return pos;
}

double player_duration_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double dur = 0;
  if (p->current_loaded) {
    ma_uint64 frames;
    ma_sound_get_length_in_pcm_frames(&p->current, &frames);
    dur = (double)frames / ma_engine_get_sample_rate(&p->engine);
  }
  pthread_mutex_unlock(&p->mutex);
  return dur;
}

vs_player_state player_state_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  vs_player_state s = p->state;
  if (p->crossfading && p->next_loaded) {
    if (ma_sound_at_end(&p->current)) {
      ma_sound_stop(&p->current);
      ma_sound_uninit(&p->current);
      p->current = p->next;
      p->current_loaded = 1;
      p->next_loaded = 0;
      p->crossfading = 0;
    }
  }
  pthread_mutex_unlock(&p->mutex);
  return s;
}

double player_crossfade_get(vs_player *p) {
  pthread_mutex_lock(&p->mutex);
  double cf = p->crossfade;
  pthread_mutex_unlock(&p->mutex);
  return cf;
}

void player_crossfade_set(vs_player *p, double seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds > 10) seconds = 10;
  pthread_mutex_lock(&p->mutex);
  p->crossfade = seconds;
  pthread_mutex_unlock(&p->mutex);
}
