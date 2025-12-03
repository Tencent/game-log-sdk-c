#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "pal/pal.h"
#include "tdr/tdr.h"
#include "tbus/tbus.h"
#include "tapp/tapp.h"
#include "tlog/tlog.h"
#include "tloghelp/tlogload.h"

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
    char *log_templates[10];  // Store 10 different size logs
    int log_count;
};

typedef struct app_context APP_CONTEXT;
typedef struct app_context *LPAPP_CONTEXT;

TAPPCTX g_framework_context;
APP_CONTEXT g_app_context;

// Forward declarations
char* generate_log(int target_size);
void init_log_templates(APP_CONTEXT *app_ctx);
void free_log_templates(APP_CONTEXT *app_ctx);
char* get_random_log(APP_CONTEXT *app_ctx);

void uninit_app_context() {
    free_log_templates(&g_app_context);
}

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
    strcpy(g_app_context.app_id, tdr_tdrip_to_ineta(framework_ctx->iId));
    memcpy(&g_app_context.app_cfg, (LPTESTTAPPCFG) framework_ctx->stConfData.pszBuff, sizeof(g_app_context.app_cfg));

    tapp_get_category("tglog_tapp_debug", &g_app_context.app_debug_log);
    if (g_app_context.app_debug_log == NULL) {
        printf("get tglog_tapp_debug category failed\n");
        return -1;
    }

    tapp_get_category("tglog_tapp_net", &g_app_context.tlog_client);
    if (g_app_context.tlog_client == NULL) {
        printf("get tglog_tapp_net category failed\n");
        return -2;
    }

    tglog_conf conf = {0};
    tglog_conf_set_default(&conf);
    conf.write_log = tglog_client_debug_log;
    tglog_network network = TGLOG_NETWORK_TCP;
    if (strcmp(g_app_context.app_cfg.szNetwork, "udp") == 0) {
        network = TGLOG_NETWORK_UDP;
    }

    tglog_conf_set_server(&conf, network, g_app_context.app_cfg.szServer, g_app_context.app_cfg.iPort);
    g_app_context.client = tglog_client_create(&conf);
    if (g_app_context.client == NULL) {
        return -1;
    }

    g_app_context.sent_count = 0;
    g_app_context.sent_success_count = 0;
    g_app_context.sent_fail_count = 0;
    g_app_context.done = false;
    g_app_context.log_count = 0;

    // Initialize random seed
    srand(time(NULL));

    // Initialize log templates
    init_log_templates(&g_app_context);

    gettimeofday(&g_app_context.start_time, NULL);
    return 0;
}

int svr_fini(TAPPCTX *framework_ctx, void *user_data) {
    uninit_app_context();
    return -1;
}

int svr_stop(TAPPCTX *framework_ctx, void *user_data) {
    return -1;
}

// Helper function to generate a log of approximately target_size bytes
char* generate_log(int target_size) {
    const char *base_log = "TGLogTappTestLog|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|";
    const char *padding_chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int padding_chars_len = strlen(padding_chars);

    char *log = (char*)malloc(target_size + 1);
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

// Initialize log templates with different sizes
void init_log_templates(APP_CONTEXT *app_ctx) {
    int sizes[] = {250, 500, 1000, 2000, 4000, 8000, 16000, 32000, 48000, 63000};
    app_ctx->log_count = 10;

    for (int i = 0; i < app_ctx->log_count; i++) {
        app_ctx->log_templates[i] = generate_log(sizes[i]);
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
// 95% of logs are <= 4K, 5% are > 4K
char* get_random_log(APP_CONTEXT *app_ctx) {
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

    if (app_ctx->client == NULL || app_ctx->tlog_client == NULL) {
        return -1;
    }

    if (app_ctx->sent_count >= app_ctx->app_cfg.iCount) {
        usleep(1000);
        return 0;
    }

    for (int i = 0; i < app_ctx->app_cfg.iBatch; i++) {
        char *log_to_send = get_random_log(app_ctx);
        if (log_to_send == NULL) {
            break;
        }

        if (app_ctx->app_cfg.iUseTLog) {
            tlog_error(app_ctx->tlog_client, 0, 0, "%s", log_to_send);
            app_ctx->sent_count++;
            app_ctx->sent_success_count++;
        } else {
            tglog_client_send(app_ctx->client, log_to_send, tglog_callback, user_data, TGLOG_MSG_F_COPY);
            app_ctx->sent_count++;
            if (app_ctx->sent_count % 500000 == 0) {
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

    if (app_ctx->client == NULL) {
        return -1;
    }

    if (app_ctx->done) {
        return -1;
    }

    // Update client every 5ms only for non-tlog mode, we set update interval to 5ms in start.sh with --timer=5
    if (!app_ctx->app_cfg.iUseTLog) {
        tglog_client_update(app_ctx->client);
    }

    if (app_ctx->sent_fail_count + app_ctx->sent_success_count >= app_ctx->app_cfg.iCount) {
        app_ctx->done = true;
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        long elapsed_time = (end_time.tv_sec - app_ctx->start_time.tv_sec) * 1000000 + (end_time.tv_usec - app_ctx->start_time.tv_usec);
        tlog_info(app_ctx->app_debug_log, 0, 0, "Sent %d logs in %ld microseconds. Success: %d, Fail: %d, UseTLog: %d",
                  app_ctx->sent_count, elapsed_time, app_ctx->sent_success_count, app_ctx->sent_fail_count, app_ctx->app_cfg.iUseTLog);

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

