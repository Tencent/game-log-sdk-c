#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "pal/pal.h"
#include "tdr/tdr.h"
#include "tapp/tapp.h"
#include "tlog/tlog.h"

#include "tglog_tapp_def.h"
#include "tglog.h"

extern unsigned char g_szMetalib_TestTappLib[];


struct app_context {
    TESTTAPPCFG app_cfg;
    LPTLOGCATEGORYINST app_debug_log;
    tglog_client *client;
    LPTLOGCATEGORYINST tlog_client;
    char app_id[64];
    int sent_count;
    int sent_success_count;
    int sent_fail_count;
    struct timeval start_time;
    bool done;
    char *log_templates[10];            // Store 10 different size logs
    int log_count;
    long max_large_log_send_time_us;    // Maximum send time for large logs (>=16K) in microseconds
    int large_log_count;                // Count of large logs sent (>=16K)
    int max_log_size;                   // Maximum log size
};

typedef struct app_context APP_CONTEXT;
typedef struct app_context *LPAPP_CONTEXT;

TAPPCTX g_framework_context;
APP_CONTEXT g_app_context;

// Forward declarations
char *generate_log(int target_size);

void init_log_templates(APP_CONTEXT *app_ctx);

void free_log_templates(APP_CONTEXT *app_ctx);

char *get_random_log(APP_CONTEXT *app_ctx, int *log_size);

void write_login_log(unsigned long long roleId, int roleLevel) {

}

void write_logout_log(unsigned long long roleId, int roleLevel) {

}

void tglog_client_debug_log(void *user_data, tglog_level level, const char *file, const int line, const char *func, const char *format, ...) {
    if (g_app_context.app_debug_log == NULL) {
        return;
    }

    int priority = TLOG_PRIORITY_DEBUG;
    switch (level) {
        case TGLOG_LEVEL_DEBUG:
            priority = TLOG_PRIORITY_DEBUG;
            break;
        case TGLOG_LEVEL_INFO:
            priority = TLOG_PRIORITY_INFO;
            break;
        case TGLOG_LEVEL_WARN:
            priority = TLOG_PRIORITY_WARN;
            break;
        case TGLOG_LEVEL_ERROR:
            priority = TLOG_PRIORITY_ERROR;
            break;
        case TGLOG_LEVEL_FATAL:
            priority = TLOG_PRIORITY_FATAL;
            break;
    }

    static char log_buf[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);

    if (tlog_category_is_priority_enabled(g_app_context.app_debug_log, priority) && (tlog_category_can_write(g_app_context.app_debug_log, priority, 0, 0))) {
        TLOGEVENT stEvt;
        const TLOGLOCINFO loc_info = {file, line, func, NULL};
        stEvt.evt_priority = priority;
        stEvt.evt_id = 0;
        stEvt.evt_cls = 0;
        stEvt.evt_is_msg_binary = 0;
        stEvt.evt_loc = &loc_info;
        tlog_category_logv(g_app_context.app_debug_log, &stEvt, "%s", log_buf);
    }
}

int svr_init(TAPPCTX *framework_ctx, void *user_data) {
    LPAPP_CONTEXT app_ctx = (LPAPP_CONTEXT) user_data;
    strcpy(app_ctx->app_id, tdr_tdrip_to_ineta(framework_ctx->iId));
    memcpy(&app_ctx->app_cfg, (LPTESTTAPPCFG) framework_ctx->stConfData.pszBuff, sizeof(app_ctx->app_cfg));

    tapp_get_category("tglog_tapp_debug", &app_ctx->app_debug_log);
    if (app_ctx->app_debug_log == NULL) {
        printf("get tglog_tapp_debug category failed\n");
        return -1;
    }

    tlog_info(app_ctx->app_debug_log, 0, 0, "tglog tapp test start");

    if (app_ctx->app_cfg.iUseTLog) {
        tapp_get_category("tglog_tapp_net", &app_ctx->tlog_client);
        if (app_ctx->tlog_client == NULL) {
            printf("get tglog_tapp_net category failed\n");
            return -2;
        }
    } else {
        tglog_conf conf = {0};
        tglog_conf_set_default(&conf);
        if (app_ctx->app_cfg.iClientBatch > 0) {
            conf.batching_max_messages = app_ctx->app_cfg.iClientBatch;
        }
        conf.write_log = tglog_client_debug_log;
        tglog_network network = TGLOG_NETWORK_TCP;
        if (strcmp(app_ctx->app_cfg.szNetwork, "udp") == 0) {
            network = TGLOG_NETWORK_UDP;
        }

        tglog_conf_set_server(&conf, network, app_ctx->app_cfg.szServer, app_ctx->app_cfg.iPort);
        app_ctx->client = tglog_client_create(&conf);
        if (app_ctx->client == NULL) {
            return -3;
        }
    }

    app_ctx->sent_count = 0;
    app_ctx->sent_success_count = 0;
    app_ctx->sent_fail_count = 0;
    app_ctx->done = false;
    app_ctx->log_count = 0;
    app_ctx->max_large_log_send_time_us = 0;
    app_ctx->large_log_count = 0;
    app_ctx->max_log_size = 0;

    // Initialize random seed
    srand(time(NULL));

    // Initialize log templates
    init_log_templates(&g_app_context);

    gettimeofday(&app_ctx->start_time, NULL);
    return 0;
}

int svr_fini(TAPPCTX *framework_ctx, void *user_data) {
    LPAPP_CONTEXT app_ctx = (LPAPP_CONTEXT) user_data;
    if (app_ctx->client) {
        char buf[8192];
        tglog_client_print_metrics(app_ctx->client, buf, sizeof(buf), true);
        tlog_info(app_ctx->app_debug_log, 0, 0, "%s", buf);

        tglog_client_print_mem_pool_stats(app_ctx->client, buf, sizeof(buf), false);
        tlog_info(app_ctx->app_debug_log, 0, 0, "%s", buf);
        tglog_client_destroy(app_ctx->client);
    }
    free_log_templates(app_ctx);
    return -1;
}

int svr_stop(TAPPCTX *framework_ctx, void *user_data) {
    return -1;
}

// Helper function to generate a log of approximately target_size bytes
char *generate_log(int target_size) {
    const char *base_log = "TGLogTappTestLog|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|";
    const char *padding_chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int padding_chars_len = strlen(padding_chars);

    char *log = (char *) malloc(target_size + 1);
    if (log == NULL) {
        return NULL;
    }

    int base_len = strlen(base_log);
    strcpy(log, base_log);

    // Fill the rest with padding to reach target size
    int current_len = base_len;
    while (current_len < target_size - 2) {  // Leave room for "|\n"
        log[current_len] = padding_chars[rand() % padding_chars_len];
        current_len++;
    }

    // End with "|\n"
    log[current_len++] = '|';
    log[current_len++] = '\n';
    log[current_len] = '\0';

    return log;
}

static int g_sizes[] = {250, 500, 1000, 2000, 4000, 8000, 16000, 32000, 48000, 63000};

// Initialize log templates with different sizes
void init_log_templates(APP_CONTEXT *app_ctx) {
    app_ctx->log_count = 10;

    for (int i = 0; i < app_ctx->log_count; i++) {
        app_ctx->log_templates[i] = generate_log(g_sizes[i]);
        if (app_ctx->log_templates[i] == NULL) {
            printf("Failed to generate log template %d\n", i);
        }
    }
}

// Free log templates
void free_log_templates(APP_CONTEXT *app_ctx) {
    for (int i = 0; i < app_ctx->log_count; i++) {
        if (app_ctx->log_templates[i] != NULL) {
            free(app_ctx->log_templates[i]);
            app_ctx->log_templates[i] = NULL;
        }
    }
}

// Get a random log from templates
// 95% of logs are <= 1K, 5% are > 1K
char *get_random_log(APP_CONTEXT *app_ctx, int *log_size) {
    if (app_ctx->log_count == 0) {
        return NULL;
    }

    // Generate a random number between 0-99
    int random_percent = rand() % 100;

    // 95% chance: select from logs <= 1K (indices 0-2: 250, 500, 1000)
    // 5% chance: select from logs > 1K (indices 3-9: 2000, 4000, 8000, 16000, 32000, 48000, 63000)
    int index;
    if (random_percent < 95) {
        // Select from first 3 templates (<=1K)
        index = rand() % 3;
    } else {
        // Select from last 7 templates (>1K)
        index = 3 + (rand() % 7);
    }

    *log_size = g_sizes[index];
    return app_ctx->log_templates[index];
}

void tglog_callback(const char *payload, int err_code, const char *err_msg, void *user_data) {
    LPAPP_CONTEXT app_ctx = (LPAPP_CONTEXT) user_data;
    (void) payload;

    if (err_code != TGLOG_ERR_OK) {
        app_ctx->sent_fail_count++;
        if (app_ctx->sent_fail_count <= 10) {  /* Only print first 10 errors to avoid spam */
            printf("Log failed: %s (code: %d)\n", err_msg, err_code);
        }
    } else {
        app_ctx->sent_success_count++;
    }
}

int svr_proc(TAPPCTX *framework_ctx, void *user_data) {
    LPAPP_CONTEXT app_ctx = (LPAPP_CONTEXT) user_data;

    if (app_ctx->app_cfg.iUseTLog) {
        if (app_ctx->tlog_client == NULL) {
            return -1;
        }
    } else {
        if (app_ctx->client == NULL) {
            return -1;
        }
    }

    if (app_ctx->sent_count >= app_ctx->app_cfg.iCount) {
        usleep(1000);
        return 0;
    }

    for (int i = 0; i < app_ctx->app_cfg.iBatch; i++) {
        int log_size;
        char *log_to_send = get_random_log(app_ctx, &log_size);
        if (log_to_send == NULL) {
            break;
        }

        if (log_size >= 16000) {
            app_ctx->large_log_count++;
        }

        if (log_size > app_ctx->max_log_size) {
            app_ctx->max_log_size = log_size;
        }

        if (app_ctx->app_cfg.iUseTLog) {
            if (log_size >= 16000) {
                struct timeval start, end;
                gettimeofday(&start, NULL);

                tlog_error(app_ctx->tlog_client, 0, 0, "%s", log_to_send);

                gettimeofday(&end, NULL);
                long elapsed_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);

                if (elapsed_us > app_ctx->max_large_log_send_time_us) {
                    app_ctx->max_large_log_send_time_us = elapsed_us;
                }
            } else {
                tlog_error(app_ctx->tlog_client, 0, 0, "%s", log_to_send);
            }
            app_ctx->sent_count++;
            app_ctx->sent_success_count++;
            if (app_ctx->sent_count % 500000 == 0) {
                tlog_info(app_ctx->app_debug_log, 0, 0, "UseTLog: %d, Sent: %d, LargeLogCount: %d, MaxLogSize: %d, MaxLargeLogSendTime: %ld us",
                          app_ctx->app_cfg.iUseTLog,
                          app_ctx->sent_count,
                          app_ctx->large_log_count,
                          app_ctx->max_log_size,
                          app_ctx->app_cfg.iUseTLog ? app_ctx->max_large_log_send_time_us : app_ctx->client->metrics->max_update_time_us);
            }
        } else {
            //测试表明，TGLOG_MSG_F_NONE零拷贝性能更好，但需要保证log_to_send在回调之前不被释放，最好使用内存池分配，这里简单起见直接使用TGLOG_MSG_F_COPY
            tglog_client_send(app_ctx->client, log_to_send, tglog_callback, user_data, TGLOG_MSG_F_COPY);
            app_ctx->sent_count++;
            if (app_ctx->sent_count % 500000 == 0) {
                tlog_info(app_ctx->app_debug_log, 0, 0, "UseTLog: %d, Sent: %d, LargeLogCount: %d, MaxLogSize: %d, MaxLargeLogSendTime: %ld us",
                          app_ctx->app_cfg.iUseTLog,
                          app_ctx->sent_count,
                          app_ctx->large_log_count,
                          app_ctx->max_log_size,
                          app_ctx->app_cfg.iUseTLog ? app_ctx->max_large_log_send_time_us : app_ctx->client->metrics->max_update_time_us);
                char buf[4096];
                tglog_client_print_metrics(app_ctx->client, buf, sizeof(buf), true);
                tlog_info(app_ctx->app_debug_log, 0, 0, "%s", buf);
            }
        }

        if (app_ctx->sent_count >= app_ctx->app_cfg.iCount) {
            break;
        }
    }

    // use usleep to simulate other game logics
    usleep(1000);
    return 0;
}

// called every 5ms
int svr_tick(TAPPCTX *framework_ctx, void *user_data) {
    LPAPP_CONTEXT app_ctx = (LPAPP_CONTEXT) user_data;

    if (app_ctx->app_cfg.iUseTLog) {
        if (app_ctx->tlog_client == NULL) {
            return -1;
        }
    } else {
        if (app_ctx->client == NULL) {
            return -1;
        }
    }

    if (app_ctx->done) {
        // Update client every 5ms only for non-tlog mode, we set update interval to 5ms in start.sh with --timer=5
        if (app_ctx->client) {
            tglog_client_update(app_ctx->client);
        }
        return -1;
    }

    // Update client every 5ms only for non-tlog mode, we set update interval to 5ms in start.sh with --timer=5
    if (app_ctx->client) {
        tglog_client_update(app_ctx->client);
    }

    if (app_ctx->sent_fail_count + app_ctx->sent_success_count >= app_ctx->app_cfg.iCount) {
        app_ctx->done = true;
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        long elapsed_time = (end_time.tv_sec - app_ctx->start_time.tv_sec) * 1000000 + (end_time.tv_usec - app_ctx->start_time.tv_usec);
        tlog_info(app_ctx->app_debug_log, 0, 0, "Sent %d logs in %ld microseconds. Success: %d, Fail: %d, UseTLog: %d, LargeLogCount: %d, MaxLogSize: %d, MaxLargeLogSendTime: %ld us",
                  app_ctx->sent_count,
                  elapsed_time,
                  app_ctx->sent_success_count,
                  app_ctx->sent_fail_count,
                  app_ctx->app_cfg.iUseTLog,
                  app_ctx->large_log_count,
                  app_ctx->max_log_size,
                  app_ctx->app_cfg.iUseTLog ? app_ctx->max_large_log_send_time_us : app_ctx->client->metrics->max_update_time_us);

        if (!app_ctx->app_cfg.iUseTLog) {
            char buf[8192];
            tglog_client_print_metrics(app_ctx->client, buf, sizeof(buf), true);
            tlog_info(app_ctx->app_debug_log, 0, 0, "%s", buf);

            tglog_client_print_mem_pool_stats(app_ctx->client, buf, sizeof(buf), false);
            tlog_info(app_ctx->app_debug_log, 0, 0, "%s", buf);
        }
    }

    return 0;
}

int svr_reload(TAPPCTX *framework_ctx, void *user_data) {
    return 0;
}

int main(int argc, char *argv[]) {
    memset(&g_framework_context, 0, sizeof(g_framework_context));
    memset(&g_app_context, 0, sizeof(g_app_context));

    int ret = 0;

    g_framework_context.argc = argc;
    g_framework_context.argv = argv;

    g_framework_context.pfnInit = (PFNTAPPFUNC) svr_init;
    g_framework_context.pfnFini = (PFNTAPPFUNC) svr_fini;
    g_framework_context.pfnStop = (PFNTAPPFUNC) svr_stop;
    g_framework_context.pfnProc = (PFNTAPPFUNC) svr_proc;
    g_framework_context.pfnTick = (PFNTAPPFUNC) svr_tick;
    g_framework_context.pfnReload = (PFNTAPPFUNC) svr_reload;

    g_framework_context.iLib = (intptr_t) g_szMetalib_TestTappLib;
    g_framework_context.stConfData.pszMetaName = (char *) "TestTappCfg";

    ret = tapp_def_init(&g_framework_context, &g_app_context);
    if (ret < 0) {
        return ret;
    }

    ret = tapp_def_mainloop(&g_framework_context, &g_app_context);

    tapp_def_fini(&g_framework_context, &g_app_context);

    return ret;
}

