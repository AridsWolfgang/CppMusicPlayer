#include "vibestream/library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sqlite3.h>

struct vs_library {
  sqlite3 *db;
};

static const char *extensions[] = {".mp3", ".flac", ".wav", ".ogg", ".m4a", ".opus", NULL};

static int has_audio_ext(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) return 0;
  for (int i = 0; extensions[i]; i++) {
    if (strcasecmp(dot, extensions[i]) == 0) return 1;
  }
  return 0;
}

static void scan_dir_r(vs_library *lib, const char *dir, int recursive) {
  DIR *d = opendir(dir);
  if (!d) return;
  struct dirent *ent;
  char full[VS_PATH_MAX];
  while ((ent = readdir(d))) {
    if (ent->d_name[0] == '.') continue;
    snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
    struct stat st;
    if (stat(full, &st) != 0) continue;
    if (S_ISDIR(st.st_mode)) {
      if (recursive) scan_dir_r(lib, full, recursive);
    } else if (S_ISREG(st.st_mode) && has_audio_ext(full)) {
      sqlite3_stmt *stmt;
      const char *sql = "INSERT OR IGNORE INTO songs (path, title, artist, album, duration) VALUES (?, ?, ?, ?, ?)";
      if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) continue;
      sqlite3_bind_text(stmt, 1, full, -1, SQLITE_TRANSIENT);

      const char *name = ent->d_name;
      const char *dot = strrchr(name, '.');
      char title[VS_TITLE_MAX];
      if (dot) {
        size_t n = dot - name;
        if (n >= VS_TITLE_MAX) n = VS_TITLE_MAX - 1;
        strncpy(title, name, n);
        title[n] = '\0';
      } else {
        strncpy(title, name, VS_TITLE_MAX - 1);
      }
      sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, "Unknown", -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 4, "Unknown", -1, SQLITE_STATIC);
      sqlite3_bind_double(stmt, 5, 0.0);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
    }
  }
  closedir(d);
}

vs_library *library_open(const char *db_path) {
  vs_library *lib = calloc(1, sizeof(vs_library));
  if (!lib) return NULL;
  if (sqlite3_open(db_path, &lib->db) != SQLITE_OK) {
    free(lib);
    return NULL;
  }
  const char *schema =
    "CREATE TABLE IF NOT EXISTS songs ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  path TEXT UNIQUE NOT NULL,"
    "  title TEXT NOT NULL,"
    "  artist TEXT DEFAULT 'Unknown',"
    "  album TEXT DEFAULT 'Unknown',"
    "  duration REAL DEFAULT 0"
    ");"
    "CREATE TABLE IF NOT EXISTS playlists ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS playlist_songs ("
    "  playlist_id INTEGER,"
    "  song_id INTEGER,"
    "  position INTEGER,"
    "  FOREIGN KEY(playlist_id) REFERENCES playlists(id),"
    "  FOREIGN KEY(song_id) REFERENCES songs(id),"
    "  PRIMARY KEY(playlist_id, song_id)"
    ");";
  sqlite3_exec(lib->db, schema, NULL, NULL, NULL);
  return lib;
}

void library_close(vs_library *lib) {
  if (lib) {
    sqlite3_close(lib->db);
    free(lib);
  }
}

int library_scan(vs_library *lib, const char *dir, int recursive) {
  scan_dir_r(lib, dir, recursive);
  return 0;
}

int library_songs_get(vs_library *lib, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, duration FROM songs ORDER BY title";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

  size_t cap = 256;
  *out = malloc(cap * sizeof(vs_song));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) {
      cap *= 2;
      *out = realloc(*out, cap * sizeof(vs_song));
    }
    vs_song *s = &(*out)[*count];
    s->id = sqlite3_column_int64(stmt, 0);
    strncpy(s->path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(s->title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    strncpy(s->artist, (const char *)sqlite3_column_text(stmt, 3), VS_ARTIST_MAX - 1);
    strncpy(s->album, (const char *)sqlite3_column_text(stmt, 4), VS_ALBUM_MAX - 1);
    s->duration = sqlite3_column_double(stmt, 5);
    s->playlist_id = 0;
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_song_by_id(vs_library *lib, int64_t id, vs_song *out) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, duration FROM songs WHERE id=?";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, id);
  int rc = -1;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out->id = sqlite3_column_int64(stmt, 0);
    strncpy(out->path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(out->title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    strncpy(out->artist, (const char *)sqlite3_column_text(stmt, 3), VS_ARTIST_MAX - 1);
    strncpy(out->album, (const char *)sqlite3_column_text(stmt, 4), VS_ALBUM_MAX - 1);
    out->duration = sqlite3_column_double(stmt, 5);
    out->playlist_id = 0;
    rc = 0;
  }
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_create(vs_library *lib, const char *name) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO playlists (name) VALUES (?)";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_delete(vs_library *lib, int64_t id) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM playlist_songs WHERE playlist_id=?";
  sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
  sqlite3_bind_int64(stmt, 1, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  sql = "DELETE FROM playlists WHERE id=?";
  sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL);
  sqlite3_bind_int64(stmt, 1, id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlists_get(vs_library *lib, vs_playlist **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, name FROM playlists ORDER BY name";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

  size_t cap = 16;
  *out = malloc(cap * sizeof(vs_playlist));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) {
      cap *= 2;
      *out = realloc(*out, cap * sizeof(vs_playlist));
    }
    vs_playlist *pl = &(*out)[*count];
    pl->id = sqlite3_column_int64(stmt, 0);
    strncpy(pl->name, (const char *)sqlite3_column_text(stmt, 1), VS_TITLE_MAX - 1);
    pl->songs = NULL;
    pl->count = 0;
    pl->capacity = 0;
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_playlist_add_song(vs_library *lib, int64_t playlist_id, int64_t song_id) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT OR IGNORE INTO playlist_songs (playlist_id, song_id, position) "
                    "VALUES (?, ?, (SELECT COALESCE(MAX(position),0)+1 FROM playlist_songs WHERE playlist_id=?))";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);
  sqlite3_bind_int64(stmt, 3, playlist_id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_remove_song(vs_library *lib, int64_t playlist_id, int64_t song_id) {
  sqlite3_stmt *stmt;
  const char *sql = "DELETE FROM playlist_songs WHERE playlist_id=? AND song_id=?";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);
  sqlite3_bind_int64(stmt, 2, song_id);
  int rc = sqlite3_step(stmt) == SQLITE_DONE ? 0 : -1;
  sqlite3_finalize(stmt);
  return rc;
}

int library_playlist_songs(vs_library *lib, int64_t playlist_id, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql =
    "SELECT s.id, s.path, s.title, s.artist, s.album, s.duration "
    "FROM songs s JOIN playlist_songs ps ON s.id = ps.song_id "
    "WHERE ps.playlist_id=? ORDER BY ps.position";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  sqlite3_bind_int64(stmt, 1, playlist_id);

  size_t cap = 256;
  *out = malloc(cap * sizeof(vs_song));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) {
      cap *= 2;
      *out = realloc(*out, cap * sizeof(vs_song));
    }
    vs_song *s = &(*out)[*count];
    s->id = sqlite3_column_int64(stmt, 0);
    strncpy(s->path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(s->title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    strncpy(s->artist, (const char *)sqlite3_column_text(stmt, 3), VS_ARTIST_MAX - 1);
    strncpy(s->album, (const char *)sqlite3_column_text(stmt, 4), VS_ALBUM_MAX - 1);
    s->duration = sqlite3_column_double(stmt, 5);
    s->playlist_id = playlist_id;
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}

int library_search(vs_library *lib, const char *query, vs_song **out, size_t *count) {
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, path, title, artist, album, duration FROM songs "
                    "WHERE title LIKE ? OR artist LIKE ? OR album LIKE ? ORDER BY title";
  if (sqlite3_prepare_v2(lib->db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
  char pattern[VS_TITLE_MAX + 2];
  snprintf(pattern, sizeof(pattern), "%%%s%%", query);
  sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_STATIC);

  size_t cap = 256;
  *out = malloc(cap * sizeof(vs_song));
  *count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (*count >= cap) {
      cap *= 2;
      *out = realloc(*out, cap * sizeof(vs_song));
    }
    vs_song *s = &(*out)[*count];
    s->id = sqlite3_column_int64(stmt, 0);
    strncpy(s->path, (const char *)sqlite3_column_text(stmt, 1), VS_PATH_MAX - 1);
    strncpy(s->title, (const char *)sqlite3_column_text(stmt, 2), VS_TITLE_MAX - 1);
    strncpy(s->artist, (const char *)sqlite3_column_text(stmt, 3), VS_ARTIST_MAX - 1);
    strncpy(s->album, (const char *)sqlite3_column_text(stmt, 4), VS_ALBUM_MAX - 1);
    s->duration = sqlite3_column_double(stmt, 5);
    s->playlist_id = 0;
    (*count)++;
  }
  sqlite3_finalize(stmt);
  return 0;
}
