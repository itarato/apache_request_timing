#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"

#include <time.h>
#include <syslog.h>

/**
 * Definitions.
 */
static void register_hooks(apr_pool_t *);
static int timing_handler(request_rec *);
static int timing_handler__log_transaction(request_rec *);
double get_micro_timestamp();

/**
 * Static global vars.
 */
static double microtime_start;

/**
 * Define the Apache2 module.
 */
module AP_MODULE_DECLARE_DATA timing_module = {
  STANDARD20_MODULE_STUFF,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  register_hooks
};

static void register_hooks(apr_pool_t *pool) {
  ap_hook_handler(timing_handler, NULL, NULL, APR_HOOK_FIRST);
  ap_hook_log_transaction(timing_handler__log_transaction, NULL, NULL, APR_HOOK_LAST);
}

static int timing_handler(request_rec *r) {
  microtime_start = get_micro_timestamp();
  return DECLINED;
}

static int timing_handler__log_transaction(request_rec *r) {
  openlog("mod_timing", 0, 0);
  syslog(LOG_NOTICE, "FILE: %s ARGS: %s TIME: %.4f", r->filename, r->args, get_micro_timestamp() - microtime_start);
  return OK;
}

double get_micro_timestamp() {  
  struct timespec tv;
  clock_gettime(CLOCK_REALTIME, &tv);
  return 1000.0 * tv.tv_sec + 1e-6f * tv.tv_nsec;
}
