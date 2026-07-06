#ifndef VIBESTREAM_TYPES_H
#define VIBESTREAM_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define VS_TITLE_MAX  512
#define VS_PATH_MAX   1024
#define VS_ARTIST_MAX 256
#define VS_ALBUM_MAX  256
#define VS_GENRE_MAX  128
#define VS_URL_MAX    1024
#define VS_NAME_MAX   256
#define VS_QUERY_MAX  256

typedef enum {
  VS_STOPPED,
  VS_PLAYING,
  VS_PAUSED
} vs_player_state;

typedef enum {
  VS_REPEAT_NONE,
  VS_REPEAT_ALL,
  VS_REPEAT_ONE
} vs_repeat_mode;

typedef struct {
  int64_t id;
  char title[VS_TITLE_MAX];
  char artist[VS_ARTIST_MAX];
  char album[VS_ALBUM_MAX];
  char genre[VS_GENRE_MAX];
  int year;
  int track;
  char path[VS_PATH_MAX];
  double duration;
  char cover_path[VS_PATH_MAX];
  int64_t playlist_id;
} vs_song;

typedef struct {
  int64_t id;
  char name[VS_NAME_MAX];
  vs_song *songs;
  size_t count;
  size_t capacity;
} vs_playlist;

typedef enum {
  VS_DL_PENDING,
  VS_DL_DOWNLOADING,
  VS_DL_DONE,
  VS_DL_FAILED
} vs_dl_status;

typedef struct {
  char url[VS_URL_MAX];
  char title[VS_TITLE_MAX];
  char path[VS_PATH_MAX];
  vs_dl_status status;
  double progress;
  int64_t song_id;
} vs_download_task;

typedef enum {
  VS_BROWSE_ARTISTS,
  VS_BROWSE_ALBUMS,
  VS_BROWSE_SONGS,
  VS_BROWSE_ALL_SONGS,
  VS_BROWSE_PLAYLISTS,
  VS_BROWSE_PLAYLIST_SONGS
} vs_browse_level;

typedef struct {
  vs_browse_level level;
  char current_artist[VS_ARTIST_MAX];
  char current_album[VS_ALBUM_MAX];
  int64_t current_playlist_id;
} vs_browser_state;

typedef struct {
  double volume;
  vs_repeat_mode repeat;
  int shuffle;
  double crossfade;
  char music_dir[VS_PATH_MAX];
  char download_dir[VS_PATH_MAX];
  int bindings[256];
} vs_config;

#endif
