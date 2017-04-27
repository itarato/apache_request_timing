#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"

#include <time.h>
#include <syslog.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/**
 * Definitions.
 */
static void register_hooks(apr_pool_t *);

static int timing_handler(request_rec *);

static int timing_handler__log_transaction(request_rec *);

double get_micro_timestamp();

void report_time_via_socket(char *, double);

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
    double elapsed = get_micro_timestamp() - microtime_start;
    syslog(LOG_NOTICE, "FILE: %s ARGS: %s TIME: %.4f", r->filename, r->args, elapsed);
    report_time_via_socket(r->filename, elapsed);
    return OK;
}

double get_micro_timestamp() {
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return 1000.0 * tv.tv_sec + 1e-6f * tv.tv_nsec;
}

void report_time_via_socket(char *site_id, double t) {
    openlog("mod_timing", 0, 0);

    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    portno = 2398;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        syslog(LOG_ERR, "Cannot open socket.");
        return;
    }

    server = gethostbyname("localhost");
    if (server == NULL) {
        syslog(LOG_ERR, "Cannot get hostname.");
        close(sockfd);
        goto close_socket_and_return;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        syslog(LOG_ERR, "Cannot connect to socket.");
        close(sockfd);
        goto close_socket_and_return;
    }

    bzero(buffer, 256);
    sprintf(buffer, "%.4f|", t);
    size_t num_part_len = strlen(buffer);
    strncpy(buffer + num_part_len, site_id, 256 - num_part_len);

    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        syslog(LOG_ERR, "Invalid number of written chars to socket.");
        goto close_socket_and_return;
    }

    close_socket_and_return: close(sockfd);
}
