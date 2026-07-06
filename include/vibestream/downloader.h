#ifndef VIBESTREAM_DOWNLOADER_H
#define VIBESTREAM_DOWNLOADER_H

#include "types.h"

typedef struct vs_downloader vs_downloader;

vs_downloader *downloader_create(const char *download_dir);
void downloader_destroy(vs_downloader *dl);

int downloader_enqueue(vs_downloader *dl, const char *url, const char *format);
int downloader_process(vs_downloader *dl);
int downloader_tasks_get(vs_downloader *dl, vs_download_task **out, size_t *count);

#endif
