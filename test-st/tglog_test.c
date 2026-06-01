/* Standard library headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Platform compatibility layer (must come before POSIX headers) */
#ifdef _WIN32
#include "tglog_platform.h"
#else
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/* Project headers */
#include "tglog.h"

/* Token bucket for rate limiting */
typedef struct {
    long capacity;
    long tokens;
    int refill_rate;
    struct timeval last_check;
} token_bucket;

void init_token_bucket(token_bucket *bucket, int capacity, int refill_rate) {
    bucket->capacity = capacity;
    bucket->tokens = capacity;
    bucket->refill_rate = refill_rate;
    gettimeofday(&bucket->last_check, NULL);
}

void refill_tokens(token_bucket *bucket) {
    struct timeval now;
    gettimeofday(&now, NULL);
    long seconds_passed = now.tv_sec - bucket->last_check.tv_sec;
    long microseconds_passed = now.tv_usec - bucket->last_check.tv_usec;
    long total_microseconds_passed = seconds_passed * 1000000 + microseconds_passed;

    if (total_microseconds_passed > 0) {
        long new_tokens = (total_microseconds_passed * bucket->refill_rate) / 1000000;
        bucket->tokens = bucket->tokens + new_tokens > bucket->capacity ? bucket->capacity : bucket->tokens + new_tokens;
        bucket->last_check = now;
    }
}

int consume_token(token_bucket *bucket, int tokens_needed) {
    refill_tokens(bucket);
    if (bucket->tokens >= tokens_needed) {
        bucket->tokens -= tokens_needed;
        return 1;
    }
    return 0;
}

/* Statistics tracking */
static uint32_t sent_count = 0;
static uint32_t failed_count = 0;

/* Callback for send results */
static void send_callback(const char *payload, int err_code, const char *err_msg, void *user_data) {
    (void) user_data;
    (void) payload;

    if (err_code != TGLOG_ERR_OK) {
        failed_count++;
        if (failed_count <= 10) {  /* Only print first 10 errors to avoid spam */
            printf("Log failed: %s (code: %d)\n", err_msg, err_code);
        }
    } else {
        sent_count++;
    }
}

void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -t, --to-send NUM      Number of messages to send (default: 10000000)\n");
    printf("  -r, --rate NUM         Token bucket refill rate, 0 means unlimited (default: 200000)\n");
    printf("  -p, --protocol PROTO   Protocol type: udp or tcp (default: tcp)\n");
    printf("  -i, --ip IP_ADDRESS    Target IP address (default: dev.tglog.com)\n");
    printf("  -P, --port PORT        Target port (default: 20001)\n");
    printf("  -h, --help             Show this help message\n");
}

int main(int argc, char *argv[]) {
    /* Default values */
    uint32_t to_send = 10000000;
    int send_rate = 200000;  /* Token bucket refill rate */
    tglog_network protocol = TGLOG_NETWORK_TCP;
    const char *ip = "dev.tglog.com";
    int port = 20001;

    /* Parse command line arguments */
    struct option long_options[] = {
            {"to-send",  required_argument, 0, 't'},
            {"rate",     required_argument, 0, 'r'},
            {"protocol", required_argument, 0, 'p'},
            {"ip",       required_argument, 0, 'i'},
            {"port",     required_argument, 0, 'P'},
            {"help",     no_argument,       0, 'h'},
            {0, 0,                          0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:r:p:i:P:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                to_send = atoi(optarg);
                break;
            case 'r':
                send_rate = atoi(optarg);
                if (send_rate < 0) {
                    fprintf(stderr, "Invalid rate: %s\n", optarg);
                    return 1;
                }
                break;
            case 'p':
                if (strcmp(optarg, "udp") == 0) {
                    protocol = TGLOG_NETWORK_UDP;
                } else if (strcmp(optarg, "tcp") == 0) {
                    protocol = TGLOG_NETWORK_TCP;
                } else {
                    fprintf(stderr, "Invalid protocol: %s\n", optarg);
                    return 1;
                }
                break;
            case 'i':
                ip = optarg;
                break;
            case 'P':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Invalid port: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Create configuration with defaults */
    tglog_conf conf = {0};
    tglog_conf_set_default(&conf);
    /* Only override connection parameters */
    tglog_conf_set_server(&conf,protocol, ip, port);

    /* Create client */
    tglog_client *client = tglog_client_create(&conf);
    if (!client) {
        fprintf(stderr, "Failed to create TGLog client\n");
        return 1;
    }

    /* Initialize token buckets */
    token_bucket send_bucket;
    if (send_rate > 0) {
        init_token_bucket(&send_bucket, 50000, send_rate);
    }

    /* Record start time */
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    /* Fixed log message like in test/tglog_test.c */
    char *log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n";

    /* Main loop */
    uint32_t i = 0;
    uint32_t enqueued = 0;
    uint32_t dropped = 0;
    while (i < to_send) {
        /* Use token bucket to limit the rate for smooth sending */
        if (send_rate > 0 && !consume_token(&send_bucket, 1)) {
            usleep(500);
            continue;
        }

        /* SDK does NOT invoke the callback when send fails (e.g. queue full),
         * so wait only for callbacks of messages that were actually accepted. */
        int rc = tglog_client_send(client, log, send_callback, NULL, TGLOG_MSG_F_NONE);
        if (rc == TGLOG_ERR_OK) {
            enqueued++;
        } else {
            dropped++;
        }
        i++;
    }

    gettimeofday(&current_time, NULL);
    double send_elapsed = (double) (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                          (double) (current_time.tv_usec - start_time.tv_usec);
    printf("send time elapsed: %f us\n", send_elapsed);
    printf("attempted: %u, enqueued: %u, dropped: %u (%.2f%%)\n",
           to_send, enqueued, dropped,
           to_send > 0 ? (double) dropped * 100.0 / to_send : 0.0);

    /* Wait for all callbacks of accepted messages to complete */
    while (sent_count + failed_count < enqueued) {
        usleep(500);
        tglog_client_update(client);
    }

    printf("sent %d logs\n", sent_count);
    gettimeofday(&current_time, NULL);
    double done_elapsed = (double) (current_time.tv_sec - start_time.tv_sec) * 1000000 +
                          (double) (current_time.tv_usec - start_time.tv_usec);
    printf("done time elapsed: %f us\n", done_elapsed);
    if (done_elapsed > 0) {
        double throughput = (double) sent_count * 1000000.0 / done_elapsed;
        printf("throughput: %.2f logs/sec\n", throughput);
    }

    /* Print metrics in JSON format */
    char json[4096];
    tglog_client_print_metrics(client, json, sizeof(json), true);
    printf("%s\n", json);

    /* Destroy client */
    tglog_client_destroy(client);
    return 0;
}