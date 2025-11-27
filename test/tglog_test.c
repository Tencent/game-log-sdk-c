/* Standard library headers */
#include <getopt.h>
#include <stdio.h>

/* System headers */
#include <sys/time.h>
#include <unistd.h>

/* Third-party library headers */
#include "ck_pr.h"

/* Project headers */
#include "tglog.h"

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


void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -t, --to-send NUM      Number of messages to send (default: 1000000)\n");
    printf("  -r, --rate NUM         Token bucket refill rate (default: 1000000)\n");
    printf("  -p, --protocol PROTO    Protocol type: udp or tcp (default: tcp)\n");
    printf("  -i, --ip IP_ADDRESS    Target IP address (default: dev.tglog.com)\n");
    printf("  -P, --port PORT        Target port (default: 20001)\n");
    printf("  -w, --worker NUM       Number of workers (default: 8)\n");
    printf("  -h, --help             Show this help message\n");
}

uint32_t sent = 0;

void callback(const char *payload, int err_code, const char *err_msg, void *user_data) {
    if (err_code != 0) {
        printf("callback: %s, %d, %s\n", payload, err_code, err_msg);
    }
    ck_pr_add_32(&sent, 1);
}

int main(int argc, char *argv[]) {
    // 设置默认值
    uint32_t to_send = 1000000;
    int rate = 1000000;
    tglog_network protocol = TGLOG_NETWORK_TCP;
    const char *ip = "dev.tglog.com";
    int port = 20001;
    int worker = 8;

    // 解析命令行参数
    struct option long_options[] = {
            {"to-send",  required_argument, 0, 't'},
            {"rate",     required_argument, 0, 'r'},
            {"protocol", required_argument, 0, 'p'},
            {"ip",       required_argument, 0, 'i'},
            {"port",     required_argument, 0, 'P'},
            {"worker",   required_argument, 0, 'w'},
            {"help",     no_argument,       0, 'h'},
            {0, 0,                          0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:c:r:p:i:P:w:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 't':
                to_send = atoi(optarg);
                break;
            case 'r':
                rate = atoi(optarg);
                break;
            case 'p':
                if (strcmp(optarg, "udp") == 0) {
                    protocol = TGLOG_NETWORK_UDP;
                } else if (strcmp(optarg, "tcp") == 0) {
                    protocol = TGLOG_NETWORK_TCP;
                } else {
                    fprintf(stderr, "Invalid protocol: %s. Use 'udp' or 'tcp'.\n", optarg);
                    return 1;
                }
                break;
            case 'i':
                ip = optarg;
                break;
            case 'P':
                port = atoi(optarg);
                break;
            case 'w':
                worker = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    token_bucket bucket;
    init_token_bucket(&bucket, rate + 1, rate);

    tglog_use_thread();

    tglog_conf *conf = tglog_conf_create(protocol, ip, port, NULL, NULL, NULL);
    if (!conf) {
        printf("failed to create conf\n");
        return 1;
    }

    conf->can_log = tglog_can_log_info;
    conf->max_conn_lifetime = 300;
    conf->worker_num = worker;
    /*use to test batch delay and check if messages will be flushed when we destroy the client*/
//    conf->batching_max_size = 50 * 1024;
//    conf->batching_max_publish_delay = 5000;
//    conf->batching_max_messages = 2000;

    tglog_client *client = tglog_client_create(conf);
    // the conf has been copied to the client, we can destroy it safely
    tglog_conf_destroy(conf);

    if (!client) {
        printf("failed to create client\n");
        return 1;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    char *log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n";
    int i = 0;
    while (true) {
        // if you want to limit the rate, you can use the token bucket
//        if (!consume_token(&bucket, 1)) {
//            usleep(500);
//            continue;
//        }

        tglog_client_send(client, log, callback, NULL, TGLOG_MSG_F_COPY);

        if (++i == to_send) {
            break;
        }
    }

    gettimeofday(&end, NULL);
    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    double elapsed = seconds * 1000 + microseconds / 1000.0;
    printf("send time elapsed: %f milliseconds\n", elapsed);

    while (ck_pr_load_32(&sent) < to_send) {
        usleep(1000);
    }

    printf("sent %d logs\n", sent);
    gettimeofday(&end, NULL);
    seconds = end.tv_sec - start.tv_sec;
    microseconds = end.tv_usec - start.tv_usec;
    elapsed = seconds * 1000 + microseconds / 1000.0;
    printf("done time elapsed: %f milliseconds\n", elapsed);

    {
        sleep(1);
        char json[8192];
        tglog_client_print_metrics(client, json, sizeof(json), true);
        printf("%s\n", json);
        tglog_client_print_mem_pool_stats(client, json, sizeof(json), false);
        printf("%s\n", json);
    }
    tglog_client_destroy(client);
    return 0;
}
