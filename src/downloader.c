#include "vibestream/downloader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

struct vs_downloader {
  char download_dir[VS_PATH_MAX];
  vs_download_task *tasks;
  size_t count;
  size_t capacity;
  pthread_mutex_t mutex;
};

vs_downloader *downloader_create(const char *download_dir) {
  vs_downloader *dl = calloc(1, sizeof(vs_downloader));
  if (!dl) return NULL;
  snprintf(dl->download_dir, sizeof(dl->download_dir), "%s", download_dir);
  pthread_mutex_init(&dl->mutex, NULL);
  dl->capacity = 16;
  dl->tasks = calloc(dl->capacity, sizeof(vs_download_task));
  return dl;
}

void downloader_destroy(vs_downloader *dl) {
  if (!dl) return;
  pthread_mutex_destroy(&dl->mutex);
  free(dl->tasks);
  free(dl);
}

static int find_slot(vs_downloader *dl) {
  for (size_t i = 0; i < dl->count; i++) {
    if (dl->tasks[i].status == VS_DL_DONE || dl->tasks[i].status == VS_DL_FAILED)
      return (int)i;
  }
  if (dl->count >= dl->capacity) {
    dl->capacity *= 2;
    dl->tasks = realloc(dl->tasks, dl->capacity * sizeof(vs_download_task));
  }
  return (int)dl->count++;
}

int downloader_enqueue(vs_downloader *dl, const char *url, const char *format) {
  (void)format;
  pthread_mutex_lock(&dl->mutex);
  int idx = find_slot(dl);
  vs_download_task *t = &dl->tasks[idx];
  strncpy(t->url, url, VS_URL_MAX - 1);
  t->status = VS_DL_PENDING;
  t->progress = 0.0;
  t->song_id = -1;
  t->title[0] = '\0';
  t->path[0] = '\0';
  pthread_mutex_unlock(&dl->mutex);
  return 0;
}

int downloader_process(vs_downloader *dl) {
  pthread_mutex_lock(&dl->mutex);
  int found = 0;
  for (size_t i = 0; i < dl->count; i++) {
    if (dl->tasks[i].status != VS_DL_PENDING) continue;
    dl->tasks[i].status = VS_DL_DOWNLOADING;
    pthread_mutex_unlock(&dl->mutex);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
      "yt-dlp --extract-audio --audio-format mp3 "
      "--output \"%s/%%(title)s.%%(ext)s\" "
      "--print filename --no-warnings \"%s\" 2>/dev/null",
      dl->download_dir, dl->tasks[i].url);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
      pthread_mutex_lock(&dl->mutex);
      dl->tasks[i].status = VS_DL_FAILED;
      pthread_mutex_unlock(&dl->mutex);
      continue;
    }
    if (fgets(dl->tasks[i].path, VS_PATH_MAX - 1, fp)) {
      size_t len = strlen(dl->tasks[i].path);
      while (len > 0 && (dl->tasks[i].path[len-1] == '\n' || dl->tasks[i].path[len-1] == '\r'))
        dl->tasks[i].path[--len] = '\0';
    }
    int status = pclose(fp);
    pthread_mutex_lock(&dl->mutex);
    dl->tasks[i].status = (status == 0) ? VS_DL_DONE : VS_DL_FAILED;
    dl->tasks[i].progress = (status == 0) ? 1.0 : 0.0;
    if (status == 0) {
      const char *name = strrchr(dl->tasks[i].path, '/');
      if (name) name++; else name = dl->tasks[i].path;
      const char *dot = strrchr(name, '.');
      if (dot) {
        size_t n = dot - name;
        if (n >= VS_TITLE_MAX) n = VS_TITLE_MAX - 1;
        strncpy(dl->tasks[i].title, name, n);
        dl->tasks[i].title[n] = '\0';
      } else {
        strncpy(dl->tasks[i].title, name, VS_TITLE_MAX - 1);
      }
    }
    found = 1;
    pthread_mutex_unlock(&dl->mutex);
    break;
  }
  if (!found) pthread_mutex_unlock(&dl->mutex);
  return 0;
}

int downloader_tasks_get(vs_downloader *dl, vs_download_task **out, size_t *count) {
  pthread_mutex_lock(&dl->mutex);
  *count = dl->count;
  *out = malloc(dl->count * sizeof(vs_download_task));
  memcpy(*out, dl->tasks, dl->count * sizeof(vs_download_task));
  pthread_mutex_unlock(&dl->mutex);
  return 0;
}
