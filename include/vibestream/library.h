#ifndef VIBESTREAM_LIBRARY_H
#define VIBESTREAM_LIBRARY_H

#include "types.h"
#include <stdbool.h>

typedef struct vs_library vs_library;

vs_library *library_open(const char *db_path);
void library_close(vs_library *lib);

int library_scan(vs_library *lib, const char *dir, int recursive);
int library_update_metadata(vs_library *lib);
int library_update_song_metadata(vs_library *lib, int64_t id);

int library_songs_get(vs_library *lib, vs_song **out, size_t *count);
int library_song_by_id(vs_library *lib, int64_t id, vs_song *out);

int library_artists_get(vs_library *lib, char ***out, size_t *count);
int library_albums_by_artist(vs_library *lib, const char *artist, char ***out, size_t *count);
int library_songs_by_album(vs_library *lib, const char *artist, const char *album, vs_song **out, size_t *count);

int library_playlist_create(vs_library *lib, const char *name, int64_t *out_id);
int library_playlist_delete(vs_library *lib, int64_t id);
int library_playlists_get(vs_library *lib, vs_playlist **out, size_t *count);
int library_playlist_by_id(vs_library *lib, int64_t id, vs_playlist *out);
int library_playlist_add_song(vs_library *lib, int64_t playlist_id, int64_t song_id);
int library_playlist_remove_song(vs_library *lib, int64_t playlist_id, int64_t song_id);
int library_playlist_songs(vs_library *lib, int64_t playlist_id, vs_song **out, size_t *count);
int library_playlist_save_queue(vs_library *lib, const char *name, vs_song *queue, size_t count);
int library_playlist_clear(vs_library *lib, int64_t playlist_id);

int library_search(vs_library *lib, const char *query, vs_song **out, size_t *count);

#endif
