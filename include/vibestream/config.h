#ifndef VIBESTREAM_CONFIG_H
#define VIBESTREAM_CONFIG_H

#include "types.h"

int config_load(vs_config *cfg);
int config_save(const vs_config *cfg);
void config_defaults(vs_config *cfg);

#endif
