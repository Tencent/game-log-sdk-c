#define _GNU_SOURCE  // For usleep on Linux

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

#include "tglog.h"
#include "ck_pr.h"
#include "TglogJNI.h"

// Global JVM reference for thread attachment
static JavaVM *g_jvm = NULL;

// Flag to ensure tglog_use_thread is called only once
static int g_use_thread_called = 0;

// Thread-local storage for JNIEnv to avoid repeated attach/detach
static pthread_key_t g_jni_env_key;
static pthread_once_t g_jni_env_key_once = PTHREAD_ONCE_INIT;
static int g_jvm_shutting_down = 0;  // Use ck_pr for atomic access
static int g_jni_env_key_created = 0;  // Track if key was created

// Initialize thread-local storage key
static void jni_env_key_init(void) {
    // Use NULL destructor - we don't want to detach threads
    // The JVM will handle cleanup when it shuts down
    // Detaching in destructor causes crashes during JVM shutdown
    if (pthread_key_create(&g_jni_env_key, NULL) == 0) {
        g_jni_env_key_created = 1;
    }
}

// Get or attach JNIEnv for current thread
static JNIEnv *get_jni_env(void) {
    // Check shutdown flag first using ck_pr atomic load
    if (ck_pr_load_int(&g_jvm_shutting_down)) {
        return NULL;
    }

    // Save JVM pointer locally to avoid race
    JavaVM *jvm = g_jvm;
    if (jvm == NULL) {
        return NULL;
    }

    // Ensure key is initialized
    pthread_once(&g_jni_env_key_once, jni_env_key_init);

    // Check thread-local storage first
    JNIEnv *env = (JNIEnv *) pthread_getspecific(g_jni_env_key);
    if (env != NULL) {
        // Double-check shutdown didn't happen
        if (ck_pr_load_int(&g_jvm_shutting_down)) {
            pthread_setspecific(g_jni_env_key, NULL);
            return NULL;
        }
        return env;
    }

    // Try to get existing env
    jint result = (*jvm)->GetEnv(jvm, (void **) &env, JNI_VERSION_1_8);

    if (result == JNI_OK && env != NULL) {
        // Already attached, store in TLS
        pthread_setspecific(g_jni_env_key, env);
        return env;
    }

    if (result == JNI_EDETACHED) {
        // Check shutdown again before attaching
        if (ck_pr_load_int(&g_jvm_shutting_down)) {
            return NULL;
        }

        // Attach as daemon thread so it doesn't prevent JVM shutdown
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_8;
        args.name = "tglog-worker";
        args.group = NULL;

        // AttachCurrentThreadAsDaemon makes the thread a daemon thread
        // Daemon threads don't prevent JVM from exiting
        result = (*jvm)->AttachCurrentThreadAsDaemon(jvm, (void **) &env, &args);
        if (result == JNI_OK && env != NULL) {
            // Store in TLS for future use
            pthread_setspecific(g_jni_env_key, env);
            return env;
        }
    }

    return NULL;
}

// JNI library initialization
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    g_jvm = vm;
    ck_pr_store_int(&g_jvm_shutting_down, 0);
    return JNI_VERSION_1_8;  // For JDK 8 compatibility
}

// JNI library cleanup
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    // Mark JVM as shutting down using ck_pr atomic store
    ck_pr_store_int(&g_jvm_shutting_down, 1);

    // Ensure the store is visible to all threads before continuing
    ck_pr_fence_store();

    // Clean up pthread key if it was created
    if (g_jni_env_key_created) {
        pthread_key_delete(g_jni_env_key);
        g_jni_env_key_created = 0;
        // Reset pthread_once so it can be initialized again if library is reloaded
        g_jni_env_key_once = (pthread_once_t) PTHREAD_ONCE_INIT;
    }

    // Clear the JVM reference last
    g_jvm = NULL;

    // Reset use_thread flag for potential reload
    ck_pr_store_int(&g_use_thread_called, 0);
}

// Simple Java debug callbacks structure
typedef struct java_debug_callbacks {
    jobject can_log_callback;     // Global reference to Java canLog callback
    jobject write_log_callback;   // Global reference to Java writeLog callback
} java_debug_callbacks_t;

// Create Java debug callbacks structure
static java_debug_callbacks_t *java_debug_callbacks_create(JNIEnv *env, jobject can_log_callback, jobject write_log_callback) {
    java_debug_callbacks_t *callbacks = malloc(sizeof(java_debug_callbacks_t));
    if (callbacks == NULL) {
        return NULL;
    }

    // Create global references for callbacks with proper error handling
    callbacks->can_log_callback = NULL;
    callbacks->write_log_callback = NULL;

    if (can_log_callback) {
        callbacks->can_log_callback = (*env)->NewGlobalRef(env, can_log_callback);
        if (callbacks->can_log_callback == NULL) {
            free(callbacks);
            return NULL;
        }
    }

    if (write_log_callback) {
        callbacks->write_log_callback = (*env)->NewGlobalRef(env, write_log_callback);
        if (callbacks->write_log_callback == NULL) {
            // Cleanup the first callback if second one fails
            if (callbacks->can_log_callback) {
                (*env)->DeleteGlobalRef(env, callbacks->can_log_callback);
            }
            free(callbacks);
            return NULL;
        }
    }

    return callbacks;
}

// Destroy Java debug callbacks structure
static void java_debug_callbacks_destroy(java_debug_callbacks_t *callbacks) {
    if (callbacks == NULL) {
        return;
    }

    // Try to get the env if this is already a Java thread
    JNIEnv *env = NULL;
    if (g_jvm != NULL) {
        (*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_8);
    }

    if (env != NULL) {
        if (callbacks->can_log_callback != NULL) {
            (*env)->DeleteGlobalRef(env, callbacks->can_log_callback);
            callbacks->can_log_callback = NULL;
        }
        if (callbacks->write_log_callback != NULL) {
            (*env)->DeleteGlobalRef(env, callbacks->write_log_callback);
            callbacks->write_log_callback = NULL;
        }
    }

    // If we can't get env, the global refs will leak, but this is better than crashing
    free(callbacks);
}

// Send callback context for individual send operations
typedef struct send_callback_ctx {
    jobject java_callback;  // Global reference to Java callback
} send_callback_ctx_t;

// Java debug canLog bridge
static bool java_debug_can_log_bridge(const tglog_level level, void *userdata) {
    java_debug_callbacks_t *callbacks = (java_debug_callbacks_t *) userdata;
    if (callbacks == NULL || callbacks->can_log_callback == NULL || g_jvm == NULL) {
        return false; // No callback means no logging
    }

    // Get cached JNI environment (will attach if needed and cache in TLS)
    JNIEnv *env = get_jni_env();
    if (env == NULL) {
        return false;
    }

    jobject logLevelObj = NULL;
    jboolean result = JNI_FALSE;

    // Dynamic lookup
    jclass logLevelClass = (*env)->FindClass(env, "TglogLogLevel");
    if (logLevelClass != NULL) {
        jmethodID fromValueMethod = (*env)->GetStaticMethodID(env, logLevelClass, "fromValue", "(I)LTglogLogLevel;");
        if (fromValueMethod != NULL) {
            logLevelObj = (*env)->CallStaticObjectMethod(env, logLevelClass, fromValueMethod, (jint)
                    level);
            if (logLevelObj != NULL) {
                jclass callbackClass = (*env)->GetObjectClass(env, callbacks->can_log_callback);
                if (callbackClass != NULL) {
                    jmethodID canLogMethod = (*env)->GetMethodID(env, callbackClass, "canLog", "(LTglogLogLevel;)Z");
                    if (canLogMethod != NULL) {
                        result = (*env)->CallBooleanMethod(env, callbacks->can_log_callback, canLogMethod, logLevelObj);
                    }
                    (*env)->DeleteLocalRef(env, callbackClass);
                }
                (*env)->DeleteLocalRef(env, logLevelObj);
            }
        }
        (*env)->DeleteLocalRef(env, logLevelClass);
    }

    // Clear any exceptions
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }

    bool ret = (result == JNI_TRUE);

    // No need to detach - thread remains attached and cached in TLS

    return ret;
}

// Java debug writeLog bridge
static void java_debug_write_log_bridge(void *userdata, tglog_level level, const char *file, const int line, const char *func, const char *format, ...) {
    java_debug_callbacks_t *callbacks = (java_debug_callbacks_t *) userdata;
    if (callbacks == NULL || callbacks->write_log_callback == NULL || g_jvm == NULL) {
        return;
    }

    // Format the message
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Get cached JNI environment (will attach if needed and cache in TLS)
    JNIEnv *env = get_jni_env();
    if (env == NULL) {
        return;
    }

    jobject logLevelObj = NULL;
    jstring jFile = NULL;
    jstring jFunc = NULL;
    jstring jMessage = NULL;

    // Dynamic lookup
    jclass logLevelClass = (*env)->FindClass(env, "TglogLogLevel");
    if (logLevelClass != NULL) {
        jmethodID fromValueMethod = (*env)->GetStaticMethodID(env, logLevelClass, "fromValue", "(I)LTglogLogLevel;");
        if (fromValueMethod != NULL) {
            logLevelObj = (*env)->CallStaticObjectMethod(env, logLevelClass, fromValueMethod, (jint)
                    level);
            if (logLevelObj != NULL) {
                // Create Java strings
                jFile = (*env)->NewStringUTF(env, file ? file : "");
                jFunc = (*env)->NewStringUTF(env, func ? func : "");
                jMessage = (*env)->NewStringUTF(env, buffer);

                if (jFile != NULL && jFunc != NULL && jMessage != NULL) {
                    jclass callbackClass = (*env)->GetObjectClass(env, callbacks->write_log_callback);
                    if (callbackClass != NULL) {
                        jmethodID writeLogMethod = (*env)->GetMethodID(env, callbackClass, "writeLog",
                                                                       "(LTglogLogLevel;Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;)V");
                        if (writeLogMethod != NULL) {
                            (*env)->CallVoidMethod(env, callbacks->write_log_callback, writeLogMethod,
                                                   logLevelObj, jFile, (jint)
                                                           line, jFunc, jMessage);
                        }
                        (*env)->DeleteLocalRef(env, callbackClass);
                    }
                }
            }
        }
        (*env)->DeleteLocalRef(env, logLevelClass);
    }

    // Cleanup
    if (jFile != NULL) (*env)->DeleteLocalRef(env, jFile);
    if (jFunc != NULL) (*env)->DeleteLocalRef(env, jFunc);
    if (jMessage != NULL) (*env)->DeleteLocalRef(env, jMessage);
    if (logLevelObj != NULL) (*env)->DeleteLocalRef(env, logLevelObj);

    // Clear any exceptions
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }

    // No need to detach - thread remains attached and cached in TLS
}

// Send callback bridge
static void java_send_callback_bridge(const char *payload, int err_code, const char *err_msg, void *user_data) {
    send_callback_ctx_t *ctx = (send_callback_ctx_t *) user_data;
    if (ctx == NULL || ctx->java_callback == NULL || g_jvm == NULL) {
        if (ctx != NULL) {
            // Try to clean up global reference if possible
            if (ctx->java_callback != NULL && g_jvm != NULL) {
                JNIEnv *env = NULL;
                if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_8) == JNI_OK && env != NULL) {
                    (*env)->DeleteGlobalRef(env, ctx->java_callback);
                }
            }
            free(ctx);
        }
        return;
    }

    // Get cached JNI environment (will attach if needed and cache in TLS)
    JNIEnv *env = get_jni_env();
    if (env == NULL) {
        // Try to get env directly for cleanup
        if ((*g_jvm)->GetEnv(g_jvm, (void **) &env, JNI_VERSION_1_8) == JNI_OK && env != NULL) {
            (*env)->DeleteGlobalRef(env, ctx->java_callback);
        }
        free(ctx);
        return;
    }

    jstring jPayload = (*env)->NewStringUTF(env, payload ? payload : "");
    if (jPayload == NULL) {
        (*env)->DeleteGlobalRef(env, ctx->java_callback);
        free(ctx);
        // No need to detach - thread remains attached for future callbacks
        return;
    }

    if (err_code == 0) {
        // Success callback - dynamic lookup
        jclass callbackClass = (*env)->GetObjectClass(env, ctx->java_callback);
        if (callbackClass != NULL) {
            jmethodID onSuccessMethod = (*env)->GetMethodID(env, callbackClass, "onSuccess", "(Ljava/lang/String;)V");
            if (onSuccessMethod != NULL) {
                (*env)->CallVoidMethod(env, ctx->java_callback, onSuccessMethod, jPayload);
            }
            (*env)->DeleteLocalRef(env, callbackClass);
        }
    } else {
        // Error callback - dynamic lookup
        jclass callbackClass = (*env)->GetObjectClass(env, ctx->java_callback);
        if (callbackClass != NULL) {
            jmethodID onErrorMethod = (*env)->GetMethodID(env, callbackClass, "onError", "(Ljava/lang/String;ILjava/lang/String;)V");
            if (onErrorMethod != NULL) {
                jstring jErrorMsg = (*env)->NewStringUTF(env, err_msg ? err_msg : "");
                if (jErrorMsg != NULL) {
                    (*env)->CallVoidMethod(env, ctx->java_callback, onErrorMethod, jPayload, (jint)
                            err_code, jErrorMsg);
                    (*env)->DeleteLocalRef(env, jErrorMsg);
                }
            }
            (*env)->DeleteLocalRef(env, callbackClass);
        }
    }

    // Cleanup
    (*env)->DeleteLocalRef(env, jPayload);
    (*env)->DeleteGlobalRef(env, ctx->java_callback);
    free(ctx);

    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }

    // No need to detach - thread remains attached and cached in TLS
}

// JNI method implementations

JNIEXPORT void JNICALL Java_TglogJNI_useThread(JNIEnv *env, jclass cls) {
    // Ensure tglog_use_thread is called only once per process
    // Use atomic compare-and-swap to avoid race conditions
    if (ck_pr_cas_int(&g_use_thread_called, 0, 1)) {
        // First call - proceed
        tglog_use_thread();
    }
    // Subsequent calls are silently ignored - no exception thrown
}

JNIEXPORT jlong JNICALL Java_TglogJNI_createConf(JNIEnv *env, jclass cls, jint network, jstring host, jint port, jobject canLogCallback, jobject writeLogCallback) {
    if (host == NULL) {
        return TglogJNI_INVALID_HANDLE;
    }

    const char *host_str = (*env)->GetStringUTFChars(env, host, NULL);
    if (host_str == NULL) {
        return TglogJNI_INVALID_HANDLE;
    }

    // Create Java debug callbacks if provided
    java_debug_callbacks_t *debug_callbacks = NULL;
    can_log_func can_log = NULL;
    write_log_func write_log = NULL;

    if (canLogCallback != NULL || writeLogCallback != NULL) {
        debug_callbacks = java_debug_callbacks_create(env, canLogCallback, writeLogCallback);
        if (debug_callbacks != NULL) {
            can_log = java_debug_can_log_bridge;
            write_log = java_debug_write_log_bridge;
        }
    }

    // Create configuration
    tglog_conf *conf = tglog_conf_create((tglog_network) network, host_str, (int) port,
                                         can_log, write_log, debug_callbacks);
    (*env)->ReleaseStringUTFChars(env, host, host_str);

    if (conf == NULL) {
        if (debug_callbacks != NULL) {
            java_debug_callbacks_destroy(debug_callbacks);
        }
        return TglogJNI_INVALID_HANDLE;
    }

    return (jlong) (uintptr_t) conf;
}

JNIEXPORT jlong JNICALL Java_TglogJNI_createConfFromConfig(JNIEnv *env, jclass cls, jobject config) {
    if (config == NULL) {
        return TglogJNI_INVALID_HANDLE;
    }

    // Get TglogConfig class and field IDs
    jclass configClass = (*env)->GetObjectClass(env, config);
    if (configClass == NULL) {
        return TglogJNI_INVALID_HANDLE;
    }

    // Get required fields
    jfieldID networkField = (*env)->GetFieldID(env, configClass, "network", "I");
    jfieldID hostField = (*env)->GetFieldID(env, configClass, "host", "Ljava/lang/String;");
    jfieldID portField = (*env)->GetFieldID(env, configClass, "port", "I");
    jfieldID canLogCallbackField = (*env)->GetFieldID(env, configClass, "canLogCallback", "LTglogCanLogCallback;");
    jfieldID writeLogCallbackField = (*env)->GetFieldID(env, configClass, "writeLogCallback", "LTglogWriteLogCallback;");

    if (networkField == NULL || hostField == NULL || portField == NULL) {
        (*env)->DeleteLocalRef(env, configClass);
        return TglogJNI_INVALID_HANDLE;
    }

    // Extract basic configuration
    jint network = (*env)->GetIntField(env, config, networkField);
    jstring host = (jstring) (*env)->GetObjectField(env, config, hostField);
    jint port = (*env)->GetIntField(env, config, portField);

    jobject canLogCallback = NULL;
    jobject writeLogCallback = NULL;
    if (canLogCallbackField != NULL) {
        canLogCallback = (*env)->GetObjectField(env, config, canLogCallbackField);
    }
    if (writeLogCallbackField != NULL) {
        writeLogCallback = (*env)->GetObjectField(env, config, writeLogCallbackField);
    }

    if (host == NULL) {
        (*env)->DeleteLocalRef(env, configClass);
        return TglogJNI_INVALID_HANDLE;
    }

    const char *host_str = (*env)->GetStringUTFChars(env, host, NULL);
    if (host_str == NULL) {
        (*env)->DeleteLocalRef(env, configClass);
        return TglogJNI_INVALID_HANDLE;
    }

    // Create Java debug callbacks if provided
    java_debug_callbacks_t *debug_callbacks = NULL;
    can_log_func can_log = NULL;
    write_log_func write_log = NULL;

    if (canLogCallback != NULL || writeLogCallback != NULL) {
        debug_callbacks = java_debug_callbacks_create(env, canLogCallback, writeLogCallback);
        if (debug_callbacks != NULL) {
            can_log = java_debug_can_log_bridge;
            write_log = java_debug_write_log_bridge;
        }
    }

    // Create configuration
    tglog_conf *conf = tglog_conf_create((tglog_network) network, host_str, (int) port,
                                         can_log, write_log, debug_callbacks);

    (*env)->ReleaseStringUTFChars(env, host, host_str);

    if (conf == NULL) {
        if (debug_callbacks != NULL) {
            java_debug_callbacks_destroy(debug_callbacks);
        }
        (*env)->DeleteLocalRef(env, configClass);
        return TglogJNI_INVALID_HANDLE;
    }

    // Set additional configuration parameters
    jfieldID updateIntervalField = (*env)->GetFieldID(env, configClass, "updateInterval", "I");
    jfieldID connTimeoutField = (*env)->GetFieldID(env, configClass, "connTimeout", "I");
    jfieldID writeHighWatermarkField = (*env)->GetFieldID(env, configClass, "writeHighWatermark", "I");
    jfieldID socketSendBufferSizeField = (*env)->GetFieldID(env, configClass, "socketSendBufferSize", "I");
    jfieldID socketRecvBufferSizeField = (*env)->GetFieldID(env, configClass, "socketRecvBufferSize", "I");
    jfieldID maxConnLifetimeField = (*env)->GetFieldID(env, configClass, "maxConnLifetime", "I");
    jfieldID workerNumField = (*env)->GetFieldID(env, configClass, "workerNum", "I");
    jfieldID batchingMaxPublishDelayField = (*env)->GetFieldID(env, configClass, "batchingMaxPublishDelay", "I");
    jfieldID batchingMaxMessagesField = (*env)->GetFieldID(env, configClass, "batchingMaxMessages", "I");
    jfieldID batchingMaxSizeField = (*env)->GetFieldID(env, configClass, "batchingMaxSize", "I");
    jfieldID maxPendingMessagesField = (*env)->GetFieldID(env, configClass, "maxPendingMessages", "I");
    jfieldID sendTimeoutField = (*env)->GetFieldID(env, configClass, "sendTimeout", "I");
    jfieldID maxRetriesField = (*env)->GetFieldID(env, configClass, "maxRetries", "I");
    jfieldID stopTimeoutField = (*env)->GetFieldID(env, configClass, "stopTimeout", "I");

    if (updateIntervalField != NULL) {
        conf->update_interval = (*env)->GetIntField(env, config, updateIntervalField);
    }
    if (connTimeoutField != NULL) {
        conf->conn_timeout = (*env)->GetIntField(env, config, connTimeoutField);
    }
    if (writeHighWatermarkField != NULL) {
        conf->write_high_watermark = (*env)->GetIntField(env, config, writeHighWatermarkField);
    }
    if (socketSendBufferSizeField != NULL) {
        conf->socket_send_buffer_size = (*env)->GetIntField(env, config, socketSendBufferSizeField);
    }
    if (socketRecvBufferSizeField != NULL) {
        conf->socket_recv_buffer_size = (*env)->GetIntField(env, config, socketRecvBufferSizeField);
    }
    if (maxConnLifetimeField != NULL) {
        conf->max_conn_lifetime = (*env)->GetIntField(env, config, maxConnLifetimeField);
    }
    if (workerNumField != NULL) {
        conf->worker_num = (*env)->GetIntField(env, config, workerNumField);
    }
    if (batchingMaxPublishDelayField != NULL) {
        conf->batching_max_publish_delay = (*env)->GetIntField(env, config, batchingMaxPublishDelayField);
    }
    if (batchingMaxMessagesField != NULL) {
        conf->batching_max_messages = (*env)->GetIntField(env, config, batchingMaxMessagesField);
    }
    if (batchingMaxSizeField != NULL) {
        conf->batching_max_size = (*env)->GetIntField(env, config, batchingMaxSizeField);
    }
    if (maxPendingMessagesField != NULL) {
        conf->max_pending_messages = (*env)->GetIntField(env, config, maxPendingMessagesField);
    }
    if (sendTimeoutField != NULL) {
        conf->send_timeout = (*env)->GetIntField(env, config, sendTimeoutField);
    }
    if (maxRetriesField != NULL) {
        conf->max_retries = (*env)->GetIntField(env, config, maxRetriesField);
    }
    if (stopTimeoutField != NULL) {
        conf->stop_timeout = (*env)->GetIntField(env, config, stopTimeoutField);
    }

    (*env)->DeleteLocalRef(env, configClass);
    return (jlong) (uintptr_t) conf;
}

JNIEXPORT jlong JNICALL Java_TglogJNI_createClient(JNIEnv *env, jclass cls, jlong confHandle) {
    if (confHandle == TglogJNI_INVALID_HANDLE) {
        return TglogJNI_INVALID_HANDLE;
    }

    tglog_conf *conf = (tglog_conf *) (uintptr_t) confHandle;

    // Simply create the client without modifying the conf
    // The client will copy the conf including user_data
    // Note: This means conf and client share the same debug_callbacks pointer
    // We need to be careful about memory management
    tglog_client *client = tglog_client_create(conf);
    if (client == NULL) {
        return TglogJNI_INVALID_HANDLE;
    }

    return (jlong) (uintptr_t) client;
}

JNIEXPORT void JNICALL Java_TglogJNI_destroyConf(JNIEnv *env, jclass cls, jlong confHandle) {
    if (confHandle == TglogJNI_INVALID_HANDLE) {
        return;
    }

    tglog_conf *conf = (tglog_conf *) (uintptr_t) confHandle;

    // DO NOT destroy Java debug callbacks here!
    // The client has a copy of the conf including the user_data pointer
    // and may still be using it. The callbacks will be destroyed when
    // the last client using them is destroyed.

    tglog_conf_destroy(conf);
}

JNIEXPORT void JNICALL Java_TglogJNI_destroyClient(JNIEnv *env, jclass cls, jlong clientHandle) {
    if (clientHandle == TglogJNI_INVALID_HANDLE) {
        return;
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;

    // Save the debug callbacks pointer BEFORE destroying the client
    // to avoid use-after-free
    java_debug_callbacks_t *debug_callbacks = (java_debug_callbacks_t *) client->conf.user_data;

    // Destroy the client (this may zero out the structure)
    tglog_client_destroy(client);
    // Do NOT access client after this point!

    // Now destroy the debug callbacks
    // This is safe because we saved the pointer before client destruction
    if (debug_callbacks != NULL) {
        java_debug_callbacks_destroy(debug_callbacks);
    }
}

JNIEXPORT jint JNICALL Java_TglogJNI_sendAsync(JNIEnv *env, jclass cls, jlong clientHandle, jstring logPayload, jobject callback, jbyte msgFlag) {
    if (clientHandle == TglogJNI_INVALID_HANDLE) {
        return TGLOG_ERR_UNKNOWN;  // Invalid handle
    }

    if (logPayload == NULL) {
        return TGLOG_ERR_BAD_LOG;  // More specific error for bad log
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;

    const char *payload_str = (*env)->GetStringUTFChars(env, logPayload, NULL);
    if (payload_str == NULL) {
        return TGLOG_ERR_MEMORY_ALLOC_FAILED;
    }

    int result;
    if (callback != NULL) {
        // Create send callback context
        send_callback_ctx_t *send_ctx = malloc(sizeof(send_callback_ctx_t));
        if (send_ctx == NULL) {
            (*env)->ReleaseStringUTFChars(env, logPayload, payload_str);
            return TGLOG_ERR_MEMORY_ALLOC_FAILED;
        }

        send_ctx->java_callback = (*env)->NewGlobalRef(env, callback);
        if (send_ctx->java_callback == NULL) {
            free(send_ctx);
            (*env)->ReleaseStringUTFChars(env, logPayload, payload_str);
            return TGLOG_ERR_MEMORY_ALLOC_FAILED;
        }

        result = tglog_client_send(client, payload_str, java_send_callback_bridge, send_ctx, (char) msgFlag);

        // IMPORTANT: Do NOT cleanup here even if result != TGLOG_ERR_OK
        // tglog_client_send guarantees the callback will always be called
        // The callback (java_send_callback_bridge) is responsible for cleanup:
        // - It will DeleteGlobalRef for ctx->java_callback
        // - It will free(ctx)
        // Cleaning up here would cause use-after-free when the callback is triggered
    } else {
        // Fire-and-forget mode
        result = tglog_client_send(client, payload_str, NULL, NULL, (char) msgFlag);
    }

    (*env)->ReleaseStringUTFChars(env, logPayload, payload_str);
    return (jint) result;
}

JNIEXPORT jstring JNICALL Java_TglogJNI_getMetrics(JNIEnv *env, jclass cls, jlong clientHandle) {
    if (clientHandle == TglogJNI_INVALID_HANDLE) {
        return NULL;
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;

    // First, try with a reasonable buffer size
    char buffer[8192];
    // Call tglog_client_print_metrics with json=true
    int result_len = tglog_client_print_metrics(client, buffer, sizeof(buffer), true);
    // Check for error
    if (result_len < 0) {
        return NULL;
    }

    jstring result = (*env)->NewStringUTF(env, buffer);
    if (result == NULL || (*env)->ExceptionCheck(env)) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
        }
        return NULL;
    }

    return result;
}

JNIEXPORT jstring JNICALL Java_TglogJNI_getErrorMessage(JNIEnv *env, jclass cls, jint errorCode) {
    const char *msg = tglog_get_err_msg((int) errorCode);
    if (msg == NULL) {
        return (*env)->NewStringUTF(env, "Unknown error");
    }

    jstring result = (*env)->NewStringUTF(env, msg);
    if (result == NULL || (*env)->ExceptionCheck(env)) {
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
        }
        return (*env)->NewStringUTF(env, "Unknown error");
    }

    return result;
}

// DirectByteBuffer optimized send function for TglogJNI (without callback)
JNIEXPORT jint JNICALL Java_TglogJNI_sendDirectBuffer(JNIEnv *env, jclass cls,
                                                      jlong clientHandle, jobject buffer,
                                                      jint offset, jint length, jbyte msgFlag) {
    if (clientHandle == TglogJNI_INVALID_HANDLE || buffer == NULL) {
        return TGLOG_ERR_UNKNOWN;
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;

    // Get direct buffer address - this is zero-copy!
    char *data = (char *) (*env)->GetDirectBufferAddress(env, buffer);
    if (data == NULL) {
        return TGLOG_ERR_UNKNOWN;
    }

    // Get buffer capacity to validate offset and length
    jlong capacity = (*env)->GetDirectBufferCapacity(env, buffer);
    if (offset < 0 || length <= 0 || offset + length > capacity) {
        return TGLOG_ERR_BAD_LOG;
    }

    // Get pointer to the log data
    char *log_start = data + offset;

    // Check if there's a null terminator at the expected position
    // Java side should have added it for zero-copy performance
    if (offset + length < capacity && log_start[length] == '\0') {
        // Perfect! The buffer is already null-terminated, true zero-copy
        return tglog_client_send(client, log_start, NULL, NULL, (char) msgFlag);
    }

    // Fallback: Need to make a copy to ensure null termination
    // This should rarely happen if Java side prepares buffer correctly
    char *log_copy = (char *) malloc(length + 1);
    if (log_copy == NULL) {
        return TGLOG_ERR_MEMORY_ALLOC_FAILED;
    }

    memcpy(log_copy, log_start, length);
    log_copy[length] = '\0';

    int result = tglog_client_send(client, log_copy, NULL, NULL, (char) msgFlag);
    free(log_copy);

    return result;
}

// DirectByteBuffer optimized send function with callback support
JNIEXPORT jint JNICALL Java_TglogJNI_sendDirectBufferAsync(JNIEnv *env, jclass cls,
                                                           jlong clientHandle, jobject buffer,
                                                           jint offset, jint length, jobject callback, jbyte msgFlag) {
    if (clientHandle == TglogJNI_INVALID_HANDLE || buffer == NULL) {
        return TGLOG_ERR_UNKNOWN;
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;

    // Get direct buffer address - this is zero-copy!
    char *data = (char *) (*env)->GetDirectBufferAddress(env, buffer);
    if (data == NULL) {
        return TGLOG_ERR_UNKNOWN;
    }

    // Get buffer capacity to validate offset and length
    jlong capacity = (*env)->GetDirectBufferCapacity(env, buffer);
    if (offset < 0 || length <= 0 || offset + length > capacity) {
        return TGLOG_ERR_BAD_LOG;
    }

    // Get pointer to the log data
    char *log_start = data + offset;

    int result;
    if (callback != NULL) {
        // Create send callback context
        send_callback_ctx_t *send_ctx = malloc(sizeof(send_callback_ctx_t));
        if (send_ctx == NULL) {
            return TGLOG_ERR_MEMORY_ALLOC_FAILED;
        }

        send_ctx->java_callback = (*env)->NewGlobalRef(env, callback);
        if (send_ctx->java_callback == NULL) {
            free(send_ctx);
            return TGLOG_ERR_MEMORY_ALLOC_FAILED;
        }

        // Check if there's a null terminator for zero-copy
        if (offset + length < capacity && log_start[length] == '\0') {
            // Zero-copy with callback
            result = tglog_client_send(client, log_start, java_send_callback_bridge, send_ctx, (char) msgFlag);
        } else {
            // Need to copy for null termination
            char *log_copy = (char *) malloc(length + 1);
            if (log_copy == NULL) {
                (*env)->DeleteGlobalRef(env, send_ctx->java_callback);
                free(send_ctx);
                return TGLOG_ERR_MEMORY_ALLOC_FAILED;
            }

            memcpy(log_copy, log_start, length);
            log_copy[length] = '\0';

            result = tglog_client_send(client, log_copy, java_send_callback_bridge, send_ctx, (char) msgFlag);
            free(log_copy);
        }

        // NOTE: Do NOT cleanup send_ctx here - callback will handle it
    } else {
        // Fire-and-forget mode
        if (offset + length < capacity && log_start[length] == '\0') {
            result = tglog_client_send(client, log_start, NULL, NULL, (char) msgFlag);
        } else {
            char *log_copy = (char *) malloc(length + 1);
            if (log_copy == NULL) {
                return TGLOG_ERR_MEMORY_ALLOC_FAILED;
            }

            memcpy(log_copy, log_start, length);
            log_copy[length] = '\0';

            result = tglog_client_send(client, log_copy, NULL, NULL, (char) msgFlag);
            free(log_copy);
        }
    }

    return result;
}


JNIEXPORT jobject JNICALL Java_TglogJNI_getStats(JNIEnv *env, jclass cls, jlong clientHandle) {
    if (clientHandle == TglogJNI_INVALID_HANDLE) {
        return NULL;
    }

    tglog_client *client = (tglog_client *) (uintptr_t) clientHandle;
    tglog_metrics *metrics = client->metrics;  // Access metrics directly from client
    if (metrics == NULL) {
        return NULL;
    }

    // Find TglogStats class
    jclass statsClass = (*env)->FindClass(env, "TglogStats");
    if (statsClass == NULL || (*env)->ExceptionCheck(env)) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        return NULL;
    }

    // Create worker stats array if workers exist
    jobjectArray workerStatsArray = NULL;
    if (metrics->workers && metrics->worker_count > 0) {
        // Worker constructor without worker array
        jmethodID workerConstructor = (*env)->GetMethodID(env, statsClass, "<init>", "(JJJJJJJJJJJJJJJJJI)V");
        if (workerConstructor == NULL || (*env)->ExceptionCheck(env)) {
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
            (*env)->DeleteLocalRef(env, statsClass);
            return NULL;
        }

        workerStatsArray = (*env)->NewObjectArray(env, metrics->worker_count, statsClass, NULL);
        if (workerStatsArray == NULL || (*env)->ExceptionCheck(env)) {
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
            (*env)->DeleteLocalRef(env, statsClass);
            return NULL;
        }

        // Populate worker stats
        for (int i = 0; i < metrics->worker_count; i++) {
            if (metrics->workers[i] != NULL) {
                tglog_metrics *worker = metrics->workers[i];
                jobject workerStats = (*env)->NewObject(env, statsClass, workerConstructor,
                                                        (jlong)
                                                                ck_pr_load_64(&worker->request_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->enqueued_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->dequeued_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->succeeded_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->failed_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->timeout_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->retry_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->dropped_msg_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->opened_conn_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->closed_conn_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->error_conn_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->eagain_err_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->endpoint_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->add_endpoint_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->remove_endpoint_count),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->msg_queue_size),
                                                        (jlong)
                                                                ck_pr_load_64(&worker->msg_queue_capacity),
                                                        1  // Worker-level metrics don't have sub-workers
                );

                if (workerStats != NULL) {
                    (*env)->SetObjectArrayElement(env, workerStatsArray, i, workerStats);
                    (*env)->DeleteLocalRef(env, workerStats);
                }

                if ((*env)->ExceptionCheck(env)) {
                    (*env)->ExceptionClear(env);
                }
            }
        }
    }

    // Get main constructor with worker stats array
    jmethodID constructor = (*env)->GetMethodID(env, statsClass, "<init>", "(JJJJJJJJJJJJJJJJJI[LTglogStats;)V");
    if (constructor == NULL || (*env)->ExceptionCheck(env)) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        if (workerStatsArray) (*env)->DeleteLocalRef(env, workerStatsArray);
        (*env)->DeleteLocalRef(env, statsClass);
        return NULL;
    }

    // Create main TglogStats object
    jobject result = (*env)->NewObject(env, statsClass, constructor,
                                       (jlong)
                                               ck_pr_load_64(&metrics->request_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->enqueued_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->dequeued_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->succeeded_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->failed_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->timeout_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->retry_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->dropped_msg_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->opened_conn_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->closed_conn_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->error_conn_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->eagain_err_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->endpoint_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->add_endpoint_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->remove_endpoint_count),
                                       (jlong)
                                               ck_pr_load_64(&metrics->msg_queue_size),
                                       (jlong)
                                               ck_pr_load_64(&metrics->msg_queue_capacity),
                                       (jint)
                                               metrics->worker_count,
                                       workerStatsArray
    );

    // Cleanup
    if (workerStatsArray) (*env)->DeleteLocalRef(env, workerStatsArray);
    (*env)->DeleteLocalRef(env, statsClass);

    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        return NULL;
    }

    return result;
}