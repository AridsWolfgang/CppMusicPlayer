#include "vibestream/ui.h"
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

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
  char search_query[256];
  vs_song *search_results;
  size_t search_count;
  int sel_search;
  int scroll_search;

  vs_download_task *dl_tasks;
  size_t dl_count;

  WINDOW *w_header;
  WINDOW *w_library;
  WINDOW *w_queue;
  WINDOW *w_nowplaying;
  WINDOW *w_status;
};

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
  } else {
    prev = ui->queue_index - 1;
  }
  ui->queue_index = prev;
  player_play(ui->player, ui->queue[ui->queue_index].path);
}

static void ui_shuffle_queue(vs_ui *ui) {
  if (ui->queue_count < 2) return;
  srand((unsigned)time(NULL));
  for (size_t i = ui->queue_count - 1; i > 0; i--) {
    size_t j = rand() % (i + 1);
    vs_song tmp = ui->queue[i];
    ui->queue[i] = ui->queue[j];
    ui->queue[j] = tmp;
  }
}

static void draw_header(vs_ui *ui) {
  WINDOW *w = ui->w_header;
  int mx = getmaxx(w);
  werase(w);
  wattron(w, A_REVERSE);
  const char *title = " VibeStream ";
  mvwprintw(w, 0, 0, "%s", title);
  if (player_state_get(ui->player) != VS_STOPPED && ui->queue_count > 0) {
    vs_song *s = &ui->queue[ui->queue_index];
    int rem = mx - (int)strlen(title) - 2;
    if (rem > 0) {
      char buf[512];
      snprintf(buf, sizeof(buf), "%s - %s", s->artist, s->title);
      buf[rem] = 0;
      mvwprintw(w, 0, (int)strlen(title) + 1, "%s", buf);
    }
  }
  wattroff(w, A_REVERSE);
  wnoutrefresh(w);
}

static void draw_library(vs_ui *ui) {
  WINDOW *w = ui->w_library;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);
  wattron(w, A_UNDERLINE);
  mvwprintw(w, 0, 0, " Library ");
  wattroff(w, A_UNDERLINE);

  size_t n = ui->searching ? ui->search_count : ui->songs_count;
  vs_song *arr = ui->searching ? ui->search_results : ui->songs;
  int *sel = ui->searching ? &ui->sel_search : &ui->sel_lib;
  int *scr = ui->searching ? &ui->scroll_search : &ui->scroll_lib;

  if (n == 0) { wnoutrefresh(w); return; }
  if (*sel >= (int)n) *sel = (int)n - 1;
  if (*sel < 0) *sel = 0;
  if (*sel < *scr) *scr = *sel;
  if (*sel >= *scr + my - 2) *scr = *sel - my + 3;

  for (int i = 0; i < my - 2 && i + *scr < (int)n; i++) {
    vs_song *s = &arr[i + *scr];
    int active = (i + *scr) == *sel;
    if (active && ui->focus == 0) wattron(w, A_REVERSE);
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", s->title);
    buf[mx - 1] = 0;
    mvwprintw(w, i + 1, 1, "%s", buf);
    if (active && ui->focus == 0) wattroff(w, A_REVERSE);
  }
  wnoutrefresh(w);
}

static void draw_queue(vs_ui *ui) {
  WINDOW *w = ui->w_queue;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);
  wattron(w, A_UNDERLINE);
  mvwprintw(w, 0, 0, " Queue ");
  wattroff(w, A_UNDERLINE);

  if (ui->queue_count == 0) { wnoutrefresh(w); return; }
  if (ui->sel_queue >= (int)ui->queue_count) ui->sel_queue = (int)ui->queue_count - 1;
  if (ui->sel_queue < 0) ui->sel_queue = 0;
  if (ui->sel_queue < ui->scroll_queue) ui->scroll_queue = ui->sel_queue;
  if (ui->sel_queue >= ui->scroll_queue + my - 2)
    ui->scroll_queue = ui->sel_queue - my + 3;

  for (int i = 0; i < my - 2 && i + ui->scroll_queue < (int)ui->queue_count; i++) {
    vs_song *s = &ui->queue[i + ui->scroll_queue];
    int active = (i + ui->scroll_queue) == ui->sel_queue;
    int playing = (i + ui->scroll_queue) == (int)ui->queue_index
                  && player_state_get(ui->player) != VS_STOPPED;
    if (active && ui->focus == 1) wattron(w, A_REVERSE);
    char buf[512];
    snprintf(buf, sizeof(buf), "%c %s", playing ? '>' : ' ', s->title);
    buf[mx - 1] = 0;
    mvwprintw(w, i + 1, 1, "%s", buf);
    if (active && ui->focus == 1) wattroff(w, A_REVERSE);
  }
  wnoutrefresh(w);
}

static void draw_nowplaying(vs_ui *ui) {
  WINDOW *w = ui->w_nowplaying;
  int mx = getmaxx(w), my = getmaxy(w);
  werase(w);
  vs_player_state st = player_state_get(ui->player);

  if (st == VS_STOPPED || ui->queue_count == 0) {
    const char *msg = ui->queue_count == 0 ? " Queue is empty " : " Stopped ";
    mvwprintw(w, my / 2, (mx - (int)strlen(msg)) / 2, "%s", msg);
    wnoutrefresh(w);
    return;
  }

  vs_song *s = &ui->queue[ui->queue_index];
  mvwprintw(w, 1, 2, "Title:  %s", s->title);
  mvwprintw(w, 2, 2, "Artist: %s", s->artist);
  mvwprintw(w, 3, 2, "Album:  %s", s->album);

  double pos = player_position_get(ui->player);
  double dur = player_duration_get(ui->player);
  if (dur <= 0) dur = s->duration;
  if (dur <= 0) dur = 1;

  int bar_w = mx - 14;
  if (bar_w < 8) bar_w = 8;
  double frac = pos / dur;
  if (frac > 1.0) frac = 1.0;
  if (frac < 0) frac = 0;
  int filled = (int)(frac * bar_w);

  char bar[512];
  int bi = 0;
  bar[bi++] = '[';
  for (int i = 0; i < bar_w; i++) bar[bi++] = i < filled ? '#' : '-';
  bar[bi++] = ']';
  bar[bi] = 0;

  int mins = (int)pos / 60, secs = (int)pos % 60;
  int dmins = (int)dur / 60, dsecs = (int)dur % 60;
  mvwprintw(w, 5, 2, "%s %d:%02d/%d:%02d", bar, mins, secs, dmins, dsecs);

  double vol = player_volume_get(ui->player);
  int vbar = (int)(vol * 20);
  char vbuf[32];
  for (int i = 0; i < 20; i++) vbuf[i] = i < vbar ? '#' : '-';
  vbuf[20] = 0;
  mvwprintw(w, 6, 2, "Vol: [%s] %d%%", vbuf, (int)(vol * 100));

  const char *rm = "none";
  if (ui->cfg->repeat == VS_REPEAT_ALL) rm = "all";
  else if (ui->cfg->repeat == VS_REPEAT_ONE) rm = "one";
  mvwprintw(w, 7, 2, "Repeat: %s  Shuffle: %s", rm, ui->cfg->shuffle ? "on" : "off");

  wnoutrefresh(w);
}

static void draw_status(vs_ui *ui) {
  WINDOW *w = ui->w_status;
  int mx = getmaxx(w);
  werase(w);
  wattron(w, A_REVERSE);
  const char *help;
  if (ui->searching) {
    help = " Type to search, ESC to cancel, Enter to search ";
  } else {
    help = " [Tab] focus  [j/k] nav  [Enter] play  [Space] pause  [s] stop  [n] next  [p] prev  [+/-] vol  [/] search  [r] repeat  [d] download  [q] quit ";
  }
  char buf[1024];
  snprintf(buf, sizeof(buf), "%.*s", mx - 1, help);
  mvwprintw(w, 0, 0, "%s", buf);
  wattroff(w, A_REVERSE);
  wnoutrefresh(w);
}

static void handle_search_input(vs_ui *ui, int ch) {
  if (ch == 27) {
    ui->searching = 0;
    ui->search_query[0] = 0;
    ui->focus = 0;
    ui_load_library(ui);
  } else if (ch == '\n' || ch == KEY_ENTER) {
    ui->searching = 0;
    ui->focus = 0;
    ui->sel_search = 0;
    ui->scroll_search = 0;
    if (!ui->search_query[0]) {
      free(ui->search_results); ui->search_results = NULL; ui->search_count = 0;
    }
  } else if (ch == KEY_BACKSPACE || ch == 127) {
    size_t len = strlen(ui->search_query);
    if (len > 0) ui->search_query[len - 1] = 0;
    ui_load_library(ui);
  } else if (isprint(ch)) {
    size_t len = strlen(ui->search_query);
    if (len < sizeof(ui->search_query) - 1) {
      ui->search_query[len] = (char)ch;
      ui->search_query[len + 1] = 0;
    }
    ui_load_library(ui);
  }
}

static void handle_key(vs_ui *ui, int ch) {
  if (ui->searching) { handle_search_input(ui, ch); return; }

  switch (ch) {
    case 'q': ui->running = 0; break;
    case '\t': ui->focus = (ui->focus + 1) % 3; break;
    case KEY_DOWN: case 'j':
      if (ui->focus == 0) ui->sel_lib++;
      else if (ui->focus == 1) ui->sel_queue++;
      break;
    case KEY_UP: case 'k':
      if (ui->focus == 0 && ui->sel_lib > 0) ui->sel_lib--;
      else if (ui->focus == 1 && ui->sel_queue > 0) ui->sel_queue--;
      break;
    case '\n': case KEY_ENTER:
      if (ui->focus == 0) {
        size_t n = ui->searching ? ui->search_count : ui->songs_count;
        int sel = ui->searching ? ui->sel_search : ui->sel_lib;
        vs_song *arr = ui->searching ? ui->search_results : ui->songs;
        if (sel >= 0 && sel < (int)n) ui_play_song(ui, &arr[sel]);
      } else if (ui->focus == 1) {
        if (ui->sel_queue >= 0 && ui->sel_queue < (int)ui->queue_count)
          ui_play_song(ui, &ui->queue[ui->sel_queue]);
      }
      break;
    case ' ':
      player_toggle(ui->player);
      break;
    case 's':
      player_stop(ui->player);
      break;
    case 'n':
      ui_play_next(ui);
      break;
    case 'p':
      ui_play_prev(ui);
      break;
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
    case 'r': {
      ui->cfg->repeat = (ui->cfg->repeat + 1) % 3;
      break;
    }
    case 'x': {
      ui->cfg->shuffle = !ui->cfg->shuffle;
      if (ui->cfg->shuffle) {
        size_t cur_id = ui->queue_count > 0 ? ui->queue[ui->queue_index].id : -1;
        ui_shuffle_queue(ui);
        for (size_t i = 0; i < ui->queue_count; i++) {
          if (ui->queue[i].id == (int64_t)cur_id) {
            ui->queue_index = i;
            break;
          }
        }
      }
      break;
    }
    case '/':
      ui->searching = 1;
      ui->search_query[0] = 0;
      break;
    case 'd': {
      if (ui->focus == 0) {
        size_t n = ui->searching ? ui->search_count : ui->songs_count;
        int sel = ui->searching ? ui->sel_search : ui->sel_lib;
        vs_song *arr = ui->searching ? ui->search_results : ui->songs;
        if (sel >= 0 && sel < (int)n) {
          char url[VS_URL_MAX];
          snprintf(url, sizeof(url), "https://music.youtube.com/search?q=%s+%s",
                   arr[sel].title, arr[sel].artist);
        }
      }
      break;
    }
    case 'a': {
      if (ui->focus == 0) {
        size_t n = ui->searching ? ui->search_count : ui->songs_count;
        int sel = ui->searching ? ui->sel_search : ui->sel_lib;
        vs_song *arr = ui->searching ? ui->search_results : ui->songs;
        if (sel >= 0 && sel < (int)n) ui_add_to_queue(ui, &arr[sel]);
      }
      break;
    }
    case 'D': {
      if (ui->focus == 1 && ui->sel_queue >= 0 && ui->sel_queue < (int)ui->queue_count) {
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
    }
  }
}

vs_ui *ui_create(vs_config *cfg, vs_player *player, vs_library *lib, vs_downloader *dl) {
  vs_ui *ui = calloc(1, sizeof(vs_ui));
  if (!ui) return NULL;
  ui->cfg = cfg;
  ui->player = player;
  ui->lib = lib;
  ui->dl = dl;
  ui->running = 1;
  ui->focus = 0;

  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  halfdelay(5);
  start_color();

  int mx = getmaxx(stdscr), my = getmaxy(stdscr);
  ui->w_header    = newwin(1, mx, 0, 0);
  ui->w_library   = newwin(my - 2, mx / 3, 1, 0);
  ui->w_queue     = newwin(my - 2, mx / 3, 1, mx / 3);
  ui->w_nowplaying = newwin(my - 2, mx - 2 * (mx / 3), 1, 2 * (mx / 3));
  ui->w_status    = newwin(1, mx, my - 1, 0);

  ui_load_all_songs(ui);
  ui_load_library(ui);

  return ui;
}

void ui_destroy(vs_ui *ui) {
  if (!ui) return;
  delwin(ui->w_header);
  delwin(ui->w_library);
  delwin(ui->w_queue);
  delwin(ui->w_nowplaying);
  delwin(ui->w_status);
  endwin();
  free(ui->songs);
  free(ui->search_results);
  free(ui->queue);
  free(ui->dl_tasks);
  free(ui);
}

int ui_run(vs_ui *ui) {
  while (ui->running) {
    draw_header(ui);
    draw_library(ui);
    draw_queue(ui);
    draw_nowplaying(ui);
    draw_status(ui);
    doupdate();

    if (ui->searching) {
      if (ui->search_query[0]) {
        WINDOW *w = ui->w_status;
        int mx = getmaxx(w);
        werase(w);
        wattron(w, A_REVERSE);
        char buf[512];
        snprintf(buf, sizeof(buf), " Search: %s_", ui->search_query);
        buf[mx - 1] = 0;
        mvwprintw(w, 0, 0, "%s", buf);
        wattroff(w, A_REVERSE);
        wnoutrefresh(w);
        doupdate();
      }
    }

    if (player_state_get(ui->player) == VS_PLAYING) {
      double pos = player_position_get(ui->player);
      double dur = player_duration_get(ui->player);
      if (dur > 0 && pos >= dur - 0.5) {
        ui_play_next(ui);
      }
    }

    int ch = wgetch(stdscr);
    if (ch != ERR) handle_key(ui, ch);
  }
  config_save(ui->cfg);
  return 0;
}
