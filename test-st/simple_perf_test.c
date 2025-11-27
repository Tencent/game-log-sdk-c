/*
 * Simple performance test for tglog single-threaded version
 * Tests sending 100 identical log messages and measures total time
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include "tglog.h"

#define NUM_MESSAGES 1000000

/* Statistics tracking */
static int sent_count = 0;
static int failed_count = 0;
static int callback_count = 0;

/* Get current time in microseconds */
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t) tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* Callback for send results */
static void send_callback(const char *payload, int err_code, const char *err_msg, void *user_data) {
    (void) payload;
    (void) user_data;

    callback_count++;

    if (err_code != TGLOG_ERR_OK) {
        failed_count++;
        printf("Message %d failed: %s (code: %d)\n", callback_count, err_msg, err_code);
    } else {
        sent_count++;
    }

    /* Print progress every 10 messages */
    if (callback_count % 10 == 0 || callback_count == NUM_MESSAGES) {
//        printf("Progress: %d/%d callbacks received (sent: %d, failed: %d)\n",
//               callback_count, NUM_MESSAGES, sent_count, failed_count);
    }
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    /* Configuration */
    tglog_conf conf;
    tglog_conf_set_default(&conf);

    /* Basic configuration */
    tglog_conf_set_server(&conf, TGLOG_NETWORK_TCP, "dev.tglog.com", 20001);

    printf("=== Simple TGLog Performance Test (Single-Threaded) ===\n");
    printf("Configuration:\n");
    printf("  Network: %s\n", conf.network == TGLOG_NETWORK_TCP ? "TCP" : "UDP");
    printf("  Messages to send: %d\n", NUM_MESSAGES);
    printf("  Batch size: %d\n", conf.batching_max_messages);
    printf("  Batch delay: %d ms\n", conf.batching_max_publish_delay);
    printf("  Max messages per update: %d\n", conf.max_messages_per_update);
    printf("\n");

    /* Create client */
    tglog_client *client = tglog_client_create(&conf);
    if (!client) {
        fprintf(stderr, "Failed to create client\n");
        return 1;
    }

    printf("Starting test...\n");
    printf("Endpoint: %s:%d\n\n", conf.host, conf.port);

    /* Prepare test message */
    const char *test_msg = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n";

    /* Start timing */
    uint64_t start_time = get_time_us();

    /* Send messages */
    printf("Sending %d messages...\n", NUM_MESSAGES);
    int ret;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        ret = tglog_client_send(client, test_msg, send_callback, NULL, TGLOG_MSG_F_COPY);
        if (ret != TGLOG_ERR_OK) {
            fprintf(stderr, "Failed to send message %d: error code %d\n", i + 1, ret);
            break;
        }
    }

    uint64_t send_duration = get_time_us() - start_time;
    printf("All messages queued in: %llu us\n", (unsigned long long) send_duration);

    /* Process messages and wait for callbacks */
    printf("Processing messages and waiting for callbacks...\n");

    while (callback_count < NUM_MESSAGES) {
        /* Call update to process messages */
        tglog_client_update(client);

        /* If all callbacks received, break */
        if (callback_count >= NUM_MESSAGES) {
            break;
        }

        /* Sleep 10ms between updates */
        usleep(100);
    }

    /* End timing */
    uint64_t total_duration = get_time_us() - start_time;

    /* Print results */
    printf("\n=== Test Results ===\n");
    printf("Messages sent: %d\n", NUM_MESSAGES);
    printf("Callbacks received: %d\n", callback_count);
    printf("Successfully sent: %d\n", sent_count);
    printf("Failed: %d\n", failed_count);

    printf("  Total time: %llu us\n", (unsigned long long) total_duration);

    /* Get and print metrics */
    printf("\n=== Client Metrics ===\n");
    char metrics_buf[4096];
    tglog_client_print_metrics(client, metrics_buf, sizeof(metrics_buf), true);
    printf("%s\n", metrics_buf);

    char stats_buf[4096];
    tglog_client_print_mem_pool_stats(client, stats_buf, sizeof(stats_buf), false);
    printf("%s\n", stats_buf);
    /* Cleanup */
    tglog_client_destroy(client);

    return (failed_count == 0) ? 0 : 1;
}
