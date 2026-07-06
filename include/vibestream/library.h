#ifndef VIBESTREAM_LIBRARY_H
#define VIBESTREAM_LIBRARY_H

#include "types.h"
#include <stdbool.h>

typedef struct vs_library vs_library;

vs_library *library_open(const char *db_path);
void library_close(vs_library *lib);

int library_scan(vs_library *lib, const char *dir, int recursive);
int library_songs_get(vs_library *lib, vs_song **out, size_t *count);
int library_song_by_id(vs_library *lib, int64_t id, vs_song *out);

int library_playlist_create(vs_library *lib, const char *name);
int library_playlist_delete(vs_library *lib, int64_t id);
int library_playlists_get(vs_library *lib, vs_playlist **out, size_t *count);
int library_playlist_add_song(vs_library *lib, int64_t playlist_id, int64_t song_id);
int library_playlist_remove_song(vs_library *lib, int64_t playlist_id, int64_t song_id);
int library_playlist_songs(vs_library *lib, int64_t playlist_id, vs_song **out, size_t *count);

int library_search(vs_library *lib, const char *query, vs_song **out, size_t *count);

#endif
