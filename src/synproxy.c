/**
 * collectd - src/synproxy.c
 * Copyright (C) 2017 Marek Becka
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Marek Becka <https://github.com/marekbecka>
 **/

#include "collectd.h"

#include "common.h"
#include "plugin.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

static const char *synproxy_stat_path = "/proc/net/stat/synproxy";

static const char *column_names[] = {
  "entries", "syn_received", "invalid", "valid", "retransmission", "reopened"
};
static const char *column_types[] = {
  "current_connections", "synproxy_connections", "synproxy_cookies", 
  "synproxy_cookies", "synproxy_cookies", "synproxy_connections"
};

static void synproxy_submit(value_t *results) {
  value_list_t vl = VALUE_LIST_INIT;

  for (unsigned n = 1; n < 6; n++) {
    vl.values = &results[n];
    vl.values_len = 1;

    sstrncpy(vl.plugin, "synproxy", sizeof(vl.plugin));
    sstrncpy(vl.type, column_types[n], sizeof(vl.type));
    sstrncpy(vl.type_instance, column_names[n], sizeof(vl.type_instance));

    plugin_dispatch_values(&vl);
  } 
}

static int synproxy_read(void) {
  FILE *fh;
  char buf[1024];
  value_t results[6];
  int is_header = 1, status = 0;

  fh = fopen(synproxy_stat_path, "r");
  if (fh == NULL) {
    ERROR("synproxy plugin: unable to open %s", synproxy_stat_path);
    return -1;
  }

  memset(results, 0, sizeof(results));

  while (fgets(buf, sizeof(buf), fh) != NULL) {
    int numfields;
    char *fields[6], *endprt;

    if (is_header) {
      is_header = 0;
      continue;
    }

    numfields = strsplit(buf, fields, STATIC_ARRAY_SIZE(fields));
    if (numfields != 6) {
      ERROR("synproxy plugin: unexpected number of columns in %s", 
             synproxy_stat_path);
      status = -1;
      break;
    }

    /* 1st column (entries) is hardcoded to 0 in kernel code */
    for (unsigned n = 1; n < 6; n++) {
      char *endptr = NULL;
      errno = 0;

      results[n].derive += strtoull(fields[n], &endprt, 16);
      if ((endptr == fields[n]) || errno != 0) {
        ERROR("synproxy plugin: unable to parse value: %s", fields[n]);
        status = -1;
        goto err_close;
      }
    }
  }

err_close:
  fclose(fh);

  if (status == 0) {
    synproxy_submit(results);
  }

  return status;  
}

void module_register(void) {
  plugin_register_read("synproxy", synproxy_read);
} /* void module_register */

