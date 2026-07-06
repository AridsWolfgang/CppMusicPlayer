#include "vibestream/config.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>

static int get_config_path(char *buf, size_t len) {
  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg) {
    snprintf(buf, len, "%s/vibestream/config.json", xdg);
  } else {
    const char *home = getenv("HOME");
    if (!home) {
      struct passwd *pw = getpwuid(getuid());
      home = pw ? pw->pw_dir : ".";
    }
    snprintf(buf, len, "%s/.config/vibestream/config.json", home);
  }
  return 0;
}

static int ensure_dir(const char *path) {
  char tmp[1024];
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  mkdir(tmp, 0755);
  return 0;
}

void config_defaults(vs_config *cfg) {
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    home = pw ? pw->pw_dir : ".";
  }
  cfg->volume = 0.75;
  cfg->repeat = VS_REPEAT_NONE;
  cfg->shuffle = 0;
  snprintf(cfg->music_dir, sizeof(cfg->music_dir), "%s/Music", home);
  snprintf(cfg->download_dir, sizeof(cfg->download_dir), "%s/Music/vibestream", home);
  memset(cfg->bindings, 0, sizeof(cfg->bindings));
  cfg->bindings['q'] = 1;
  cfg->bindings[' '] = 2;
  cfg->bindings['s'] = 3;
  cfg->bindings['n'] = 4;
  cfg->bindings['p'] = 5;
  cfg->bindings['v'] = 6;
  cfg->bindings['h'] = 7;
  cfg->bindings['l'] = 8;
  cfg->bindings['r'] = 9;
  cfg->bindings['d'] = 10;
  cfg->bindings['/'] = 11;
  cfg->bindings['+'] = 12;
  cfg->bindings['-'] = 13;
}

int config_load(vs_config *cfg) {
  char path[1024];
  get_config_path(path, sizeof(path));
  config_defaults(cfg);

  FILE *f = fopen(path, "r");
  if (!f) return -1;

  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *data = malloc(len + 1);
  if (!data) { fclose(f); return -1; }
  fread(data, 1, len, f);
  data[len] = '\0';
  fclose(f);

  cJSON *json = cJSON_Parse(data);
  free(data);
  if (!json) return -1;

  cJSON *v = cJSON_GetObjectItem(json, "volume");
  if (v && cJSON_IsNumber(v)) cfg->volume = v->valuedouble;

  v = cJSON_GetObjectItem(json, "repeat");
  if (v && cJSON_IsString(v)) {
    if (strcmp(v->valuestring, "all") == 0) cfg->repeat = VS_REPEAT_ALL;
    else if (strcmp(v->valuestring, "one") == 0) cfg->repeat = VS_REPEAT_ONE;
  }

  v = cJSON_GetObjectItem(json, "shuffle");
  if (v && cJSON_IsBool(v)) cfg->shuffle = cJSON_IsTrue(v);

  v = cJSON_GetObjectItem(json, "music_dir");
  if (v && cJSON_IsString(v)) snprintf(cfg->music_dir, sizeof(cfg->music_dir), "%s", v->valuestring);

  v = cJSON_GetObjectItem(json, "download_dir");
  if (v && cJSON_IsString(v)) snprintf(cfg->download_dir, sizeof(cfg->download_dir), "%s", v->valuestring);

  cJSON_Delete(json);
  return 0;
}

int config_save(const vs_config *cfg) {
  char path[1024];
  get_config_path(path, sizeof(path));

  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "volume", cfg->volume);
  cJSON_AddBoolToObject(json, "shuffle", cfg->shuffle);
  cJSON_AddStringToObject(json, "music_dir", cfg->music_dir);
  cJSON_AddStringToObject(json, "download_dir", cfg->download_dir);

  const char *r = "none";
  if (cfg->repeat == VS_REPEAT_ALL) r = "all";
  else if (cfg->repeat == VS_REPEAT_ONE) r = "one";
  cJSON_AddStringToObject(json, "repeat", r);

  char *data = cJSON_Print(json);
  cJSON_Delete(json);

  char dir[1024];
  snprintf(dir, sizeof(dir), "%s", path);
  char *slash = strrchr(dir, '/');
  if (slash) { *slash = '\0'; ensure_dir(dir); }

  FILE *f = fopen(path, "w");
  if (!f) { free(data); return -1; }
  fputs(data, f);
  fclose(f);
  free(data);
  return 0;
}
