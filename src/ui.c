#include "vibestream/ui.h"
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

#define COLOR_HEADER   1
#define COLOR_STATUS   2
#define COLOR_SELECT   3
#define COLOR_PLAYING  4
#define COLOR_DIM      5
#define COLOR_ACCENT   6
#define COLOR_TITLE    7

struct vs_ui {
  vs_config *cfg;
  vs_player *player;
  vs_library *lib;
  vs_downloader *dl;

  int running;
  int focus;
  int sel_lib;
  int sel_queue;
  int scroll_lib;
  int scroll_queue;

  vs_song *songs;
  size_t songs_count;
  vs_song *queue;
  size_t queue_count;
  size_t queue_index;

  int searching;
  char search_query[VS_QUERY_MAX];
  vs_song *search_results;
  size_t search_count;
  int sel_search;
  int scroll_search;

  vs_browser_state browser;
  char **artist_list;
  size_t artist_count;
  char **album_list;
  size_t album_count;

  vs_download_task *dl_tasks;
  size_t dl_count;

  vs_playlist *playlists;
  size_t playlist_count;
  vs_song *pl_songs;
  size_t pl_songs_count;

  int show_help;
  int download_mode;
  int download_input;
  char download_url[VS_URL_MAX];
  int download_url_pos;

  int create_playlist_mode;
  char pl_name[VS_NAME_MAX];
  int pl_name_pos;

  int sel_help;
  int scroll_help;

  WINDOW *w_header;
  WINDOW *w_library;
  WINDOW *w_queue;
  WINDOW *w_nowplaying;
  WINDOW *w_status;
  WINDOW *w_overlay;
};

static void ui_play_song(vs_ui *ui, vs_song *song);

static void init_colors(void) {
  if (!has_colors()) return;
  start_color();
  init_pair(COLOR_HEADER,  COLOR_WHITE,  COLOR_BLUE);
  init_pair(COLOR_STATUS,  COLOR_WHITE,  COLOR_BLUE);
  init_pair(COLOR_SELECT,  COLOR_BLACK,  COLOR_CYAN);
  init_pair(COLOR_PLAYING, COLOR_GREEN,  COLOR_BLACK);
  init_pair(COLOR_DIM,     COLOR_WHITE,  COLOR_BLACK);
  init_pair(COLOR_ACCENT,  COLOR_YELLOW, COLOR_BLACK);
  init_pair(COLOR_TITLE,   COLOR_CYAN,   COLOR_BLACK);
}

static void load_for_browser(vs_ui *ui) {
  free(ui->artist_list); ui->artist_list = NULL; ui->artist_count = 0;
  free(ui->album_list);  ui->album_list  = NULL; ui->album_count  = 0;
  free(ui->pl_songs);    ui->pl_songs    = NULL; ui->pl_songs_count = 0;

  switch (ui->browser.level) {
    case VS_BROWSE_ARTISTS:
      library_artists_get(ui->lib, &ui->artist_list, &ui->artist_count);
      free(ui->playlists); library_playlists_get(ui->lib, &ui->playlists, &ui->playlist_count);
      break;
    case VS_BROWSE_ALBUMS:
      library_albums_by_artist(ui->lib, ui->browser.current_artist, &ui->album_list, &ui->album_count);
      break;
    case VS_BROWSE_SONGS:
      library_songs_by_album(ui->lib, ui->browser.current_artist, ui->browser.current_album,
                             &ui->pl_songs, &ui->pl_songs_count);
      break;
    case VS_BROWSE_ALL_SONGS:
      break;
    case VS_BROWSE_PLAYLISTS:
      free(ui->playlists);
      library_playlists_get(ui->lib, &ui->playlists, &ui->playlist_count);
      break;
    case VS_BROWSE_PLAYLIST_SONGS:
      if (ui->browser.current_playlist_id > 0)
        library_playlist_songs(ui->lib, ui->browser.current_playlist_id, &ui->pl_songs, &ui->pl_songs_count);
      break;
  }
  if (ui->sel_lib >= (int)ui->songs_count && ui->songs_count > 0)
    ui->sel_lib = (int)ui->songs_count - 1;
  if (ui->sel_lib < 0) ui->sel_lib = 0;
}

static void library_browser_enter(vs_ui *ui) {
  int sel = ui->sel_lib;
  switch (ui->browser.level) {
    case VS_BROWSE_ARTISTS: {
      int total = (int)(ui->artist_count + 2 + ui->playlist_count);
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_ALL_SONGS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      } else if (sel == 1) {
        ui->browser.level = VS_BROWSE_PLAYLISTS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      } else if (sel - 2 < (int)ui->artist_count) {
        int idx = sel - 2;
        strncpy(ui->browser.current_artist, ui->artist_list[idx], VS_ARTIST_MAX - 1);
        ui->browser.level = VS_BROWSE_ALBUMS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      } else {
        int pidx = sel - 2 - (int)ui->artist_count;
        if (pidx < (int)ui->playlist_count) {
          ui->browser.current_playlist_id = ui->playlists[pidx].id;
          ui->browser.level = VS_BROWSE_PLAYLIST_SONGS;
          ui->sel_lib = 0; ui->scroll_lib = 0;
          load_for_browser(ui);
        }
      }
      break;
    }
    case VS_BROWSE_ALBUMS:
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_ARTISTS;
        ui->sel_lib = 2;
        ui->browser.current_artist[0] = 0;
        load_for_browser(ui);
      } else if (sel - 1 < (int)ui->album_count) {
        strncpy(ui->browser.current_album, ui->album_list[sel - 1], VS_ALBUM_MAX - 1);
        ui->browser.level = VS_BROWSE_SONGS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      }
      break;
    case VS_BROWSE_SONGS:
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_ALBUMS;
        ui->sel_lib = 1;
        ui->browser.current_album[0] = 0;
        load_for_browser(ui);
      } else if (sel - 1 < (int)ui->pl_songs_count) {
        ui_play_song(ui, &ui->pl_songs[sel - 1]);
      }
      break;
    case VS_BROWSE_ALL_SONGS:
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_ARTISTS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      } else if (sel - 1 < (int)ui->songs_count) {
        ui_play_song(ui, &ui->songs[sel - 1]);
      }
      break;
    case VS_BROWSE_PLAYLISTS:
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_ARTISTS;
        ui->sel_lib = 1;
        load_for_browser(ui);
      } else if (sel - 1 < (int)ui->playlist_count) {
        ui->browser.current_playlist_id = ui->playlists[sel - 1].id;
        ui->browser.level = VS_BROWSE_PLAYLIST_SONGS;
        ui->sel_lib = 0; ui->scroll_lib = 0;
        load_for_browser(ui);
      }
      break;
    case VS_BROWSE_PLAYLIST_SONGS:
      if (sel == 0) {
        ui->browser.level = VS_BROWSE_PLAYLISTS;
        ui->browser.current_playlist_id = 0;
        ui->sel_lib = 1;
        load_for_browser(ui);
      } else if (sel - 1 < (int)ui->pl_songs_count) {
        ui_play_song(ui, &ui->pl_songs[sel - 1]);
      }
      break;
  }
}

static void ui_load_library(vs_ui *ui) {
  free(ui->search_results); ui->search_results = NULL; ui->search_count = 0;
  if (ui->searching && ui->search_query[0])
    library_search(ui->lib, ui->search_query, &ui->search_results, &ui->search_count);
}

static void ui_load_all_songs(vs_ui *ui) {
  free(ui->songs);
  library_songs_get(ui->lib, &ui->songs, &ui->songs_count);
}

static void ui_add_to_queue(vs_ui *ui, vs_song *song) {
  ui->queue = realloc(ui->queue, (ui->queue_count + 1) * sizeof(vs_song));
  ui->queue[ui->queue_count++] = *song;
}

static void ui_play_song(vs_ui *ui, vs_song *song) {
  if (!song || !song->path[0]) return;
  if (player_play(ui->player, song->path) == 0) {
    int found = -1;
    for (size_t i = 0; i < ui->queue_count; i++) {
      if (ui->queue[i].id == song->id) { found = (int)i; break; }
    }
    if (found < 0) {
      ui_add_to_queue(ui, song);
      ui->queue_index = ui->queue_count - 1;
    } else {
      ui->queue_index = (size_t)found;
    }
  }
}

static void ui_play_next(vs_ui *ui) {
  if (ui->queue_count == 0) return;
  if (ui->cfg->repeat == VS_REPEAT_ONE) {
    player_play(ui->player, ui->queue[ui->queue_index].path);
    return;
  }
  size_t next = ui->queue_index + 1;
  if (next >= ui->queue_count) {
    if (ui->cfg->repeat == VS_REPEAT_ALL) next = 0;
    else { player_stop(ui->player); return; }
  }
  ui->queue_index = next;
  double cf = player_crossfade_get(ui->player);
  if (cf > 0 && player_state_get(ui->player) == VS_PLAYING)
    player_start_crossfade(ui->player, ui->queue[ui->queue_index].path);
  else
    player_play(ui->player, ui->queue[ui->queue_index].path);
}

static void ui_play_prev(vs_ui *ui) {
  if (ui->queue_count == 0) return;
  double pos = player_position_get(ui->player);
  if (pos > 3.0) { player_seek(ui->player, 0); return; }
  size_t prev;
  if (ui->queue_index == 0) {
    if (ui->cfg->repeat == VS_REPEAT_ALL) prev = ui->queue_count - 1;
    else return;
  } else { prev = ui->queue_index - 1; }
  ui->queue_index = prev;
  player_play(ui->player, ui->queue[ui->queue_index].path);
}

static void ui_shuffle_queue(vs_ui *ui) {
  if (ui->queue_count < 2) return;
  srand((unsigned)time(NULL));
  for (size_t i = ui->queue_count - 1; i > 0; i--) {
    size_t j = rand() % (i + 1);
    vs_song tmp = ui->queue[i]; ui->queue[i] = ui->queue[j]; ui->queue[j] = tmp;
  }
}

static void draw_header(vs_ui *ui) {
  WINDOW *w = ui->w_header;
  int mx = getmaxx(w);
  werase(w);
  wattron(w, A_REVERSE | COLOR_PAIR(COLOR_HEADER));
  const char *view = " Library ";
  if (ui->browser.level == VS_BROWSE_ALL_SONGS) view = " All Songs ";
  else if (ui->browser.level == VS_BROWSE_PLAYLISTS || ui->browser.level == VS_BROWSE_PLAYLIST_SONGS) view = " Playlists ";
  else if (ui->browser.level == VS_BROWSE_ALBUMS) view = " Albums ";
  else if (ui->browser.level == VS_BROWSE_SONGS) view = " Songs ";
  if (ui->searching) view = " Search ";
  if (ui->download_mode) view = " Download ";
  if (ui->create_playlist_mode) view = " Save Playlist ";
  if (ui->show_help) view = " Help ";
  char buf[1024];
  int pos = 0;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "VibeStream%s", view);
  if (player_state_get(ui->player) != VS_STOPPED && ui->queue_count > 0) {
    vs_song *s = &ui->queue[ui->queue_index];
    if (pos < mx - 5)
      pos += snprintf(buf + pos, sizeof(buf) - pos, " | %s - %s", s->artist, s->title);
  }
  buf[mx - 1] = 0;
  mvwprintw(w, 0, 0, "%s", buf);
  wattroff(w, A_REVERSE | COLOR_PAIR(COLOR_HEADER));
  wnoutrefresh(w);
}

static int lib_list_count(vs_ui *ui) {
  switch (ui->browser.level) {
    case VS_BROWSE_ARTISTS:    return (int)(ui->artist_count + 2 + ui->playlist_count);
    case VS_BROWSE_ALBUMS:     return (int)(ui->album_count + 1);
    case VS_BROWSE_SONGS:      return (int)(ui->pl_songs_count + 1);
    case VS_BROWSE_ALL_SONGS:  return (int)(ui->songs_count + 1);
    case VS_BROWSE_PLAYLISTS:  return (int)(ui->playlist_count + 1);
    case VS_BROWSE_PLAYLIST_SONGS: return (int)(ui->pl_songs_count + 1);
  }
  return 0;
}

static const char *lib_list_item(vs_ui *ui, int idx) {
  static char buf[1024];
  switch (ui->browser.level) {
    case VS_BROWSE_ARTISTS:
      if (idx == 0) return "All Songs";
      if (idx == 1) return "Playlists";
      idx -= 2;
      if (idx < (int)ui->artist_count) return ui->artist_list[idx];
      idx -= (int)ui->artist_count;
      if (idx < (int)ui->playlist_count) {
        snprintf(buf, sizeof(buf), "  %s", ui->playlists[idx].name);
        return buf;
      }
      return "";
    case VS_BROWSE_ALBUMS:
      if (idx == 0) return "<< Back to Artists";
      idx--;
      if (idx < (int)ui->album_count) return ui->album_list[idx];
      return "";
    case VS_BROWSE_SONGS:
      if (idx == 0) { snprintf(buf, sizeof(buf), "<< Back to Albums"); return buf; }
      idx--;
      if (idx < (int)ui->pl_songs_count) {
        vs_song *s = &ui->pl_songs[idx];
        if (s->track > 0)
          snprintf(buf, sizeof(buf), "%02d. %s", s->track, s->title);
        else
          snprintf(buf, sizeof(buf), "%s", s->title);
        return buf;
      }
      return "";
    case VS_BROWSE_ALL_SONGS:
      if (idx == 0) return "<< Back";
      idx--;
      if (idx < (int)ui->songs_count) {
        vs_song *s = &ui->songs[idx];
        snprintf(buf, sizeof(buf), "%s - %s", s->artist, s->title);
        return buf;
      }
      return "";
    case VS_BROWSE_PLAYLISTS:
      if (idx == 0) return "<< Back to Library";
      idx--;
      if (idx < (int)ui->playlist_count) return ui->playlists[idx].name;
      return "";
    case VS_BROWSE_PLAYLIST_SONGS:
      if (idx == 0) { snprintf(buf, sizeof(buf), "<< Back to Playlists"); return buf; }
      idx--;
      if (idx < (int)ui->pl_songs_count) {
        vs_song *s = &ui->pl_songs[idx];
        snprintf(buf, sizeof(buf), "%s - %s", s->artist, s->title);
        return buf;
      }
      return "";
  }
  return "";
}

static void draw_library(vs_ui *ui) {
  WINDOW *w = ui->w_library;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);

  wattron(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));
  const char *hl = "";
  switch (ui->browser.level) {
    case VS_BROWSE_ARTISTS: hl = "Browse"; break;
    case VS_BROWSE_ALBUMS:  hl = ui->browser.current_artist; break;
    case VS_BROWSE_SONGS: hl = ui->browser.current_album; break;
    default: hl = "Songs";
  }
  if (strlen(hl) == 0) hl = "Songs";
  mvwprintw(w, 0, 0, " %.60s", hl);
  wattroff(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));

  int total = lib_list_count(ui);
  int *sel = ui->searching ? &ui->sel_search : &ui->sel_lib;
  int *scr;

  if (ui->searching) {
    scr = &ui->scroll_search;
    total = (int)ui->search_count;
  } else {
    scr = &ui->scroll_lib;
  }

  if (*sel >= total) *sel = total > 0 ? total - 1 : 0;
  if (*sel < 0) *sel = 0;
  if (*sel < *scr) *scr = *sel;
  if (*sel >= *scr + my - 2) *scr = *sel - my + 3;

  if (total == 0) {
    mvwprintw(w, my / 2, 2, "No songs found");
    wnoutrefresh(w);
    mvwaddch(w, 0, mx - 1, ACS_VLINE);
    return;
  }

  for (int i = 0; i < my - 2 && i + *scr < total; i++) {
    int active = (i + *scr) == *sel;
    int line = i + *scr;
    const char *text;

    if (ui->searching) {
      if (line < (int)ui->search_count) {
        vs_song *s = &ui->search_results[line];
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s - %s", s->artist, s->title);
        text = tmp;
      } else text = "";
    } else {
      text = lib_list_item(ui, line);
    }

    if (!text || !*text) { mvwprintw(w, i + 1, 1, "%*s", mx - 2, " "); continue; }
    if (active && (ui->focus == 0 || ui->searching)) wattron(w, A_REVERSE | COLOR_PAIR(COLOR_SELECT));
    char buf[512];
    snprintf(buf, sizeof(buf), "%.*s", mx - 3, text);
    mvwprintw(w, i + 1, 1, "%s", buf);
    if (active && (ui->focus == 0 || ui->searching)) wattroff(w, A_REVERSE | COLOR_PAIR(COLOR_SELECT));
  }
  mvwaddch(w, 0, mx - 1, ACS_VLINE);
  wnoutrefresh(w);
}

static void draw_queue(vs_ui *ui) {
  WINDOW *w = ui->w_queue;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);
  wattron(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));
  char hdr[64];
  snprintf(hdr, sizeof(hdr), " Queue [%zu]", ui->queue_count);
  mvwprintw(w, 0, 0, "%s", hdr);
  wattroff(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));

  if (ui->queue_count == 0) { wnoutrefresh(w); return; }
  if (ui->sel_queue >= (int)ui->queue_count) ui->sel_queue = (int)ui->queue_count - 1;
  if (ui->sel_queue < 0) ui->sel_queue = 0;
  if (ui->sel_queue < ui->scroll_queue) ui->scroll_queue = ui->sel_queue;
  if (ui->sel_queue >= ui->scroll_queue + my - 2) ui->scroll_queue = ui->sel_queue - my + 3;

  for (int i = 0; i < my - 2 && i + ui->scroll_queue < (int)ui->queue_count; i++) {
    vs_song *s = &ui->queue[i + ui->scroll_queue];
    int active = (i + ui->scroll_queue) == ui->sel_queue;
    int playing = (i + ui->scroll_queue) == (int)ui->queue_index
                  && player_state_get(ui->player) != VS_STOPPED;
    if (active && ui->focus == 1) wattron(w, A_REVERSE | COLOR_PAIR(COLOR_SELECT));
    if (playing) wattron(w, COLOR_PAIR(COLOR_PLAYING));
    char buf[512];
    snprintf(buf, sizeof(buf), "%c %s - %s", playing ? '>' : ' ', s->artist, s->title);
    buf[mx - 1] = 0;
    mvwprintw(w, i + 1, 1, "%s", buf);
    if (playing) wattroff(w, COLOR_PAIR(COLOR_PLAYING));
    if (active && ui->focus == 1) wattroff(w, A_REVERSE | COLOR_PAIR(COLOR_SELECT));
  }
  wnoutrefresh(w);
}

static void draw_nowplaying_bar(WINDOW *w, int y, int x, int width, double pos, double dur) {
  if (dur <= 0) dur = 1;
  double frac = pos / dur;
  if (frac > 1.0) frac = 1.0;
  if (frac < 0) frac = 0;
  int filled = (int)(frac * (width - 2));
  if (filled > width - 2) filled = width - 2;
  mvwaddch(w, y, x, '[');
  wattron(w, COLOR_PAIR(COLOR_ACCENT));
  for (int i = 0; i < filled; i++) mvwaddch(w, y, x + 1 + i, '=');
  wattroff(w, COLOR_PAIR(COLOR_ACCENT));
  for (int i = filled; i < width - 2; i++) mvwaddch(w, y, x + 1 + i, '-');
  mvwaddch(w, y, x + width - 1, ']');
  int mins = (int)pos / 60, secs = (int)pos % 60;
  int dmins = (int)dur / 60, dsecs = (int)dur % 60;
  mvwprintw(w, y, x + width + 2, "%d:%02d/%d:%02d", mins, secs, dmins, dsecs);
}

static void draw_nowplaying(vs_ui *ui) {
  WINDOW *w = ui->w_nowplaying;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);
  vs_player_state st = player_state_get(ui->player);

  wattron(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));
  mvwprintw(w, 0, 0, " Now Playing ");
  wattroff(w, A_UNDERLINE | COLOR_PAIR(COLOR_TITLE));

  if (st == VS_STOPPED || ui->queue_count == 0) {
    if (ui->download_mode) {
      mvwprintw(w, 2, 2, "Enter URL:");
      mvwprintw(w, 3, 2, "%.*s", mx - 6, ui->download_url);
      wattron(w, A_BLINK);
      mvwaddch(w, 3, 2 + (int)strlen(ui->download_url), '_');
      wattroff(w, A_BLINK);
    } else if (ui->create_playlist_mode) {
      mvwprintw(w, 2, 2, "Playlist name:");
      mvwprintw(w, 3, 2, "%.*s", mx - 6, ui->pl_name);
      wattron(w, A_BLINK);
      mvwaddch(w, 3, 2 + (int)strlen(ui->pl_name), '_');
      wattroff(w, A_BLINK);
    } else {
      const char *msg = ui->queue_count == 0 ? " Queue is empty " : " Stopped ";
      mvwprintw(w, my / 2, (mx - (int)strlen(msg)) / 2, "%s", msg);
    }
    wnoutrefresh(w);
    return;
  }

  vs_song *s = &ui->queue[ui->queue_index];
  wattron(w, COLOR_PAIR(COLOR_ACCENT));
  mvwprintw(w, 1, 2, "%.*s", mx - 6, s->title);
  wattroff(w, COLOR_PAIR(COLOR_ACCENT));
  mvwprintw(w, 2, 2, "by %.*s", mx - 10, s->artist);
  mvwprintw(w, 3, 2, "on %.*s", mx - 10, s->album);
  if (s->year > 0) {
    mvwprintw(w, 4, 2, "(%d)", s->year);
  }
  if (s->genre[0]) {
    mvwprintw(w, 4, mx > 20 ? 20 : 10, "Genre: %s", s->genre);
  }

  double pos = player_position_get(ui->player);
  double dur = player_duration_get(ui->player);
  if (dur <= 0) dur = s->duration;

  int bar_w = mx - 16;
  if (bar_w < 10) bar_w = 10;
  draw_nowplaying_bar(w, 6, 2, bar_w, pos, dur);

  double vol = player_volume_get(ui->player);
  int vbar = (int)(vol * 16);
  mvwprintw(w, 7, 2, "Vol: ");
  wattron(w, COLOR_PAIR(COLOR_ACCENT));
  for (int i = 0; i < 16; i++) waddch(w, i < vbar ? '#' : '-');
  wattroff(w, COLOR_PAIR(COLOR_ACCENT));
  mvwprintw(w, 7, 21, "%d%%", (int)(vol * 100));

  const char *rm = "none";
  if (ui->cfg->repeat == VS_REPEAT_ALL) rm = "all";
  else if (ui->cfg->repeat == VS_REPEAT_ONE) rm = "one";
  mvwprintw(w, 8, 2, "Repeat: %s  Shuffle: %s  Crossfade: %.1fs",
            rm, ui->cfg->shuffle ? "on" : "off", ui->cfg->crossfade);

  int dl_count = 0;
  for (size_t i = 0; i < ui->dl_count; i++)
    if (ui->dl_tasks[i].status == VS_DL_DOWNLOADING || ui->dl_tasks[i].status == VS_DL_PENDING) dl_count++;
  if (dl_count > 0 || ui->dl_count > 0) {
    mvwprintw(w, 9, 2, "Downloads: %zu total", ui->dl_count);
  }

  wnoutrefresh(w);
}

static void draw_status(vs_ui *ui) {
  WINDOW *w = ui->w_status;
  int mx = getmaxx(w);
  werase(w);
  wattron(w, A_REVERSE | COLOR_PAIR(COLOR_STATUS));

  const char *help = "";
  if (ui->show_help) help = " [q] close help ";
  else if (ui->download_mode) help = " Type URL, Enter to confirm, ESC to cancel ";
  else if (ui->create_playlist_mode) help = " Type name, Enter to save, ESC to cancel ";
  else if (ui->searching) help = " Type to search, Enter to finish, ESC to cancel ";
  else help = " [Tab] focus  [j/k] nav  [Enter] open/play  [Space] pause  [s] stop  [n/p] next/prev  [+/-] vol  [h/l] seek  [/] search  [r] repeat  [x] shuffle  [c] crossfade  [d] download  [a] add  [D] remove  [S] save queue  [?] help  [q] quit ";

  char buf[2048];
  snprintf(buf, sizeof(buf), "%.*s", mx - 1, help);
  mvwprintw(w, 0, 0, "%s", buf);
  wattroff(w, A_REVERSE | COLOR_PAIR(COLOR_STATUS));
  wnoutrefresh(w);
}

static void draw_help(vs_ui *ui) {
  int my, mx;
  getmaxyx(stdscr, my, mx);
  int h = 24, w = 50;
  int sy = (my - h) / 2, sx = (mx - w) / 2;
  if (sy < 1) sy = 1;
  if (sx < 1) sx = 1;

  if (ui->w_overlay) delwin(ui->w_overlay);
  ui->w_overlay = newwin(h, w, sy, sx);
  WINDOW *ow = ui->w_overlay;
  werase(ow);
  wattron(ow, COLOR_PAIR(COLOR_HEADER));
  box(ow, 0, 0);
  wattroff(ow, COLOR_PAIR(COLOR_HEADER));
  mvwprintw(ow, 0, 2, " Help - Key Bindings ");

  const char *help_items[] = {
    "Tab        Switch focus (Library/Queue)",
    "j / Down   Navigate down",
    "k / Up     Navigate up",
    "Enter      Open / Play selected",
    "Space      Play / Pause",
    "s          Stop playback",
    "n          Next track",
    "p          Previous track",
    "+ / -      Volume up / down",
    "h / l      Seek backward / forward (5s)",
    "r          Toggle repeat (none/all/one)",
    "x          Toggle shuffle",
    "c          Change crossfade duration",
    "/          Search library",
    "a          Add song to queue",
    "D          Remove song from queue",
    "S          Save queue as playlist",
    "d          Download song/add URL",
    "?          Toggle this help",
    "q / ESC    Quit / Cancel input",
    "",
    "Browse: Artists -> Albums -> Songs",
    "Use left arrow / back option to go up",
  };
  int n_items = sizeof(help_items) / sizeof(help_items[0]);

  for (int i = 0; i < h - 2 && i < n_items; i++) {
    mvwprintw(ow, i + 1, 2, "%-40s", help_items[i]);
  }
  wnoutrefresh(ow);
}

static void close_overlay(vs_ui *ui) {
  if (ui->w_overlay) {
    delwin(ui->w_overlay);
    ui->w_overlay = NULL;
  }
}

static void handle_search_input(vs_ui *ui, int ch) {
  if (ch == 27) {
    ui->searching = 0; ui->search_query[0] = 0; ui->focus = 0;
    free(ui->search_results); ui->search_results = NULL; ui->search_count = 0;
  } else if (ch == '\n' || ch == KEY_ENTER) {
    ui->searching = 0; ui->focus = 0; ui->sel_search = 0; ui->scroll_search = 0;
  } else if (ch == KEY_BACKSPACE || ch == 127) {
    size_t len = strlen(ui->search_query);
    if (len > 0) ui->search_query[len - 1] = 0;
    ui_load_library(ui);
  } else if (isprint(ch)) {
    size_t len = strlen(ui->search_query);
    if (len < sizeof(ui->search_query) - 1) {
      ui->search_query[len] = (char)ch; ui->search_query[len + 1] = 0;
    }
    ui_load_library(ui);
  }
}

static void handle_download_input(vs_ui *ui, int ch) {
  if (ch == 27) {
    ui->download_mode = 0; ui->download_url[0] = 0; ui->download_url_pos = 0;
  } else if (ch == '\n' || ch == KEY_ENTER) {
    if (ui->download_url[0]) {
      downloader_enqueue(ui->dl, ui->download_url, "mp3");
      downloader_process(ui->dl);
    }
    ui->download_mode = 0; ui->download_url[0] = 0; ui->download_url_pos = 0;
  } else if (ch == KEY_BACKSPACE || ch == 127) {
    if (ui->download_url_pos > 0) ui->download_url[--ui->download_url_pos] = 0;
  } else if (isprint(ch)) {
    if (ui->download_url_pos < (int)sizeof(ui->download_url) - 2)
      ui->download_url[ui->download_url_pos++] = (char)ch;
    ui->download_url[ui->download_url_pos] = 0;
  }
}

static void handle_playlist_input(vs_ui *ui, int ch) {
  if (ch == 27) {
    ui->create_playlist_mode = 0; ui->pl_name[0] = 0; ui->pl_name_pos = 0;
  } else if (ch == '\n' || ch == KEY_ENTER) {
    if (ui->pl_name[0] && ui->queue_count > 0) {
      library_playlist_save_queue(ui->lib, ui->pl_name, ui->queue, ui->queue_count);
    }
    ui->create_playlist_mode = 0; ui->pl_name[0] = 0; ui->pl_name_pos = 0;
  } else if (ch == KEY_BACKSPACE || ch == 127) {
    if (ui->pl_name_pos > 0) ui->pl_name[--ui->pl_name_pos] = 0;
  } else if (isprint(ch)) {
    if (ui->pl_name_pos < (int)sizeof(ui->pl_name) - 2)
      ui->pl_name[ui->pl_name_pos++] = (char)ch;
    ui->pl_name[ui->pl_name_pos] = 0;
  }
}

static vs_song *lib_get_song(vs_ui *ui, int idx) {
  switch (ui->browser.level) {
    case VS_BROWSE_SONGS:
      if (idx > 0 && idx - 1 < (int)ui->pl_songs_count) return &ui->pl_songs[idx - 1];
      break;
    case VS_BROWSE_ALL_SONGS:
      if (idx > 0 && idx - 1 < (int)ui->songs_count) return &ui->songs[idx - 1];
      break;
    case VS_BROWSE_PLAYLIST_SONGS:
      if (idx > 0 && idx - 1 < (int)ui->pl_songs_count) return &ui->pl_songs[idx - 1];
      break;
    default:
      break;
  }
  return NULL;
}

static void handle_key(vs_ui *ui, int ch) {
  if (ui->show_help) { close_overlay(ui); ui->show_help = 0; return; }
  if (ui->searching) { handle_search_input(ui, ch); return; }
  if (ui->download_mode) { handle_download_input(ui, ch); return; }
  if (ui->create_playlist_mode) { handle_playlist_input(ui, ch); return; }

  switch (ch) {
    case 'q': case 'Q': ui->running = 0; break;
    case 27:  ui->running = 0; break;
    case '?': ui->show_help = 1; break;

    case '	':
      ui->focus = (ui->focus + 1) % 2;
      break;

    case KEY_DOWN: case 'j':
      if (ui->focus == 0) ui->sel_lib++;
      else if (ui->focus == 1) ui->sel_queue++;
      break;
    case KEY_UP: case 'k':
      if (ui->focus == 0 && ui->sel_lib > 0) ui->sel_lib--;
      else if (ui->focus == 1 && ui->sel_queue > 0) ui->sel_queue--;
      break;
    case KEY_LEFT: case 'H':
      if (ui->focus == 0) {
        if (ui->browser.level == VS_BROWSE_ALBUMS ||
            ui->browser.level == VS_BROWSE_ALL_SONGS) {
          ui->browser.level = VS_BROWSE_ARTISTS;
          ui->sel_lib = 0; load_for_browser(ui);
        } else if (ui->browser.level == VS_BROWSE_SONGS) {
          ui->browser.level = VS_BROWSE_ALBUMS;
          ui->sel_lib = 1; load_for_browser(ui);
        } else if (ui->browser.level == VS_BROWSE_PLAYLIST_SONGS) {
          ui->browser.level = VS_BROWSE_PLAYLISTS;
          ui->sel_lib = 1; load_for_browser(ui);
        } else if (ui->browser.level == VS_BROWSE_PLAYLISTS) {
          ui->browser.level = VS_BROWSE_ARTISTS;
          ui->sel_lib = 1; load_for_browser(ui);
        }
      } else {
        player_seek_relative(ui->player, -5);
      }
      break;
    case KEY_RIGHT: case 'L':
      if (ui->focus == 1) {
        player_seek_relative(ui->player, 5);
      }
      break;

    case '\n': case KEY_ENTER:
      if (ui->focus == 0) {
        if (ui->searching) {
          if (ui->sel_search >= 0 && ui->sel_search < (int)ui->search_count)
            ui_play_song(ui, &ui->search_results[ui->sel_search]);
        } else {
          library_browser_enter(ui);
        }
      } else if (ui->focus == 1) {
        if (ui->sel_queue >= 0 && ui->sel_queue < (int)ui->queue_count)
          ui_play_song(ui, &ui->queue[ui->sel_queue]);
      }
      break;

    case ' ': player_toggle(ui->player); break;
    case 's': player_stop(ui->player); break;
    case 'n': case 'N': ui_play_next(ui); break;
    case 'p': case 'P': ui_play_prev(ui); break;

    case '+': case '=': {
      double v = player_volume_get(ui->player) + 0.05;
      if (v > 1.0) v = 1.0;
      player_volume_set(ui->player, v);
      break;
    }
    case '-': case '_': {
      double v = player_volume_get(ui->player) - 0.05;
      if (v < 0) v = 0;
      player_volume_set(ui->player, v);
      break;
    }

    case 'h': player_seek_relative(ui->player, -5); break;
    case 'l': player_seek_relative(ui->player, 5); break;

    case 'r':
      ui->cfg->repeat = (ui->cfg->repeat + 1) % 3;
      break;

    case 'x':
      ui->cfg->shuffle = !ui->cfg->shuffle;
      if (ui->cfg->shuffle) {
        size_t cur_id = ui->queue_count > 0 ? ui->queue[ui->queue_index].id : (size_t)-1;
        ui_shuffle_queue(ui);
        for (size_t i = 0; i < ui->queue_count; i++) {
          if (ui->queue[i].id == (int64_t)cur_id) { ui->queue_index = i; break; }
        }
      }
      break;

    case 'c': {
      double cf = player_crossfade_get(ui->player);
      cf += 0.5;
      if (cf > 10.0) cf = 0.0;
      player_crossfade_set(ui->player, cf);
      ui->cfg->crossfade = cf;
      break;
    }

    case '/':
      if (ui->focus == 0) { ui->searching = 1; ui->search_query[0] = 0; }
      break;

    case 'a':
      if (ui->focus == 0 && !ui->searching) {
        vs_song *s = lib_get_song(ui, ui->sel_lib);
        if (s) ui_add_to_queue(ui, s);
      }
      break;

    case 'd':
      if (ui->focus == 0 && !ui->searching) {
        int total = lib_list_count(ui);
        if (ui->sel_lib >= 0 && ui->sel_lib < total) {
          const char *text = lib_list_item(ui, ui->sel_lib);
          if (text && text[0] && text[0] != '<') {
            char url[VS_URL_MAX];
            snprintf(url, sizeof(url), "ytsearch1:%s", text);
            downloader_enqueue(ui->dl, url, "mp3");
          }
        }
      } else {
        ui->download_mode = 1; ui->download_url[0] = 0; ui->download_url_pos = 0;
      }
      break;

    case 'D':
      if (ui->focus == 1 && ui->sel_queue < (int)ui->queue_count) {
        size_t idx = (size_t)ui->sel_queue;
        if (idx < ui->queue_count) {
          memmove(&ui->queue[idx], &ui->queue[idx + 1],
                  (ui->queue_count - idx - 1) * sizeof(vs_song));
          ui->queue_count--;
          if (ui->sel_queue >= (int)ui->queue_count && ui->queue_count > 0)
            ui->sel_queue = (int)ui->queue_count - 1;
          if (idx < ui->queue_index) ui->queue_index--;
          else if (idx == ui->queue_index && ui->queue_index >= ui->queue_count)
            ui->queue_index = ui->queue_count > 0 ? ui->queue_count - 1 : 0;
        }
      }
      break;

    case 'S':
      if (ui->focus == 1 && ui->queue_count > 0) {
        ui->create_playlist_mode = 1; ui->pl_name[0] = 0; ui->pl_name_pos = 0;
      }
      break;
  }
}

vs_ui *ui_create(vs_config *cfg, vs_player *player, vs_library *lib, vs_downloader *dl) {
  vs_ui *ui = calloc(1, sizeof(vs_ui));
  if (!ui) return NULL;
  ui->cfg = cfg; ui->player = player; ui->lib = lib; ui->dl = dl;
  ui->running = 1; ui->focus = 0;

  initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
  halfdelay(5);
  init_colors();

  int mx = getmaxx(stdscr), my = getmaxy(stdscr);
  int lw = mx * 40 / 100;
  if (lw < 20) lw = 20;
  int rw = mx - lw;
  int qh = my / 2;

  ui->w_header     = newwin(1, mx, 0, 0);
  ui->w_library    = newwin(my - 2, lw, 1, 0);
  ui->w_queue      = newwin(qh, rw, 1, lw);
  ui->w_nowplaying = newwin(my - 2 - qh, rw, 1 + qh, lw);
  ui->w_status     = newwin(1, mx, my - 1, 0);

  ui->browser.level = VS_BROWSE_ARTISTS;
  ui_load_all_songs(ui);
  load_for_browser(ui);
  return ui;
}

void ui_destroy(vs_ui *ui) {
  if (!ui) return;
  close_overlay(ui);
  delwin(ui->w_header); delwin(ui->w_library); delwin(ui->w_queue);
  delwin(ui->w_nowplaying); delwin(ui->w_status);
  endwin();
  free(ui->songs); free(ui->search_results); free(ui->queue);
  free(ui->dl_tasks); free(ui->artist_list);
  free(ui->album_list); free(ui->playlists); free(ui->pl_songs);
  for (size_t i = 0; i < ui->artist_count; i++) free(ui->artist_list[i]);
  for (size_t i = 0; i < ui->album_count; i++) free(ui->album_list[i]);
  free(ui);
}

int ui_run(vs_ui *ui) {
  while (ui->running) {
    downloader_process(ui->dl);
    downloader_tasks_get(ui->dl, &ui->dl_tasks, &ui->dl_count);

    draw_header(ui);
    draw_library(ui);
    draw_queue(ui);
    draw_nowplaying(ui);
    draw_status(ui);
    if (ui->show_help) draw_help(ui);
    doupdate();

    if (player_state_get(ui->player) == VS_PLAYING) {
      double pos = player_position_get(ui->player);
      double dur = player_duration_get(ui->player);
      if (dur > 0 && pos >= dur - 0.3) {
        ui_play_next(ui);
      }
    }

    int ch = wgetch(stdscr);
    if (ch != ERR) handle_key(ui, ch);
  }
  config_save(ui->cfg);
  return 0;
}
