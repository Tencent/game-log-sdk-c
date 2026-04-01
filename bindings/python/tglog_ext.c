/*
 * High-performance Python C extension for tglog
 * Minimal overhead for big data applications
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "tglog.h"
#include <string.h>

/* Platform-specific mutex abstraction */
#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION tglog_py_mutex_t;
    #define tglog_py_mutex_init(m) InitializeCriticalSection(m)
    #define tglog_py_mutex_destroy(m) DeleteCriticalSection(m)
    #define tglog_py_mutex_lock(m) EnterCriticalSection(m)
    #define tglog_py_mutex_unlock(m) LeaveCriticalSection(m)
#else
    #include <pthread.h>
    typedef pthread_mutex_t tglog_py_mutex_t;
    #define tglog_py_mutex_init(m) pthread_mutex_init(m, NULL)
    #define tglog_py_mutex_destroy(m) pthread_mutex_destroy(m)
    #define tglog_py_mutex_lock(m) pthread_mutex_lock(m)
    #define tglog_py_mutex_unlock(m) pthread_mutex_unlock(m)
#endif

/* Global initialization flag */
static int g_initialized = 0;
static tglog_py_mutex_t g_init_lock;
static int g_lock_initialized = 0;

/* Callback counter for performance testing */
typedef struct {
    uint64_t success_count;
    uint64_t error_count;
    tglog_py_mutex_t lock;
} CallbackCounter;

/* Python TglogClient object */
typedef struct {
    PyObject_HEAD
    tglog_client *client;
    CallbackCounter *counter;
    char *host;
    int port;
    int destroyed;
} TglogClientObject;

/* Fast C callback for performance testing */
static void fast_callback(const char *payload, int err_code, const char *err_msg, void *user_data) {
    CallbackCounter *counter = (CallbackCounter *)user_data;
    if (counter) {
        tglog_py_mutex_lock(&counter->lock);
        if (err_code == 0) {
            counter->success_count++;
        } else {
            counter->error_count++;
        }
        tglog_py_mutex_unlock(&counter->lock);
    }
}

/* Initialize tglog (call once) */
static void ensure_initialized(void) {
    tglog_py_mutex_lock(&g_init_lock);
    if (!g_initialized) {
        tglog_use_thread();
        g_initialized = 1;
    }
    tglog_py_mutex_unlock(&g_init_lock);
}

/* Destructor */
static void TglogClient_dealloc(TglogClientObject *self) {
    if (self->client && !self->destroyed) {
        tglog_client_destroy(self->client);
        self->destroyed = 1;
    }
    if (self->counter) {
        tglog_py_mutex_destroy(&self->counter->lock);
        free(self->counter);
    }
    if (self->host) {
        free(self->host);
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

/* Constructor */
static int TglogClient_init(TglogClientObject *self, PyObject *args, PyObject *kwds) {
    static char *kwlist[] = {"protocol", "host", "port", "workers", NULL};
    char *protocol = "tcp";
    char *host = "127.0.0.1";
    int port = 20001;
    int workers = 8;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ssii", kwlist,
                                     &protocol, &host, &port, &workers)) {
        return -1;
    }
    
    ensure_initialized();
    
    /* Determine network type */
    tglog_network network = TGLOG_NETWORK_TCP;
    if (strcasecmp(protocol, "udp") == 0) {
        network = TGLOG_NETWORK_UDP;
    }
    
    /* Create configuration */
    tglog_conf *conf = tglog_conf_create(network, host, port, NULL, NULL, NULL);
    if (!conf) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create configuration");
        return -1;
    }
    
    /* Set optimal performance settings */
    conf->worker_num = workers;
    conf->batching_max_publish_delay = 10;
    conf->batching_max_messages = 50;
    conf->batching_max_size = 32 * 1024;
    conf->max_conn_lifetime = 300;
    conf->max_pending_messages = 409600;
    
    /* Create client */
    self->client = tglog_client_create(conf);
    tglog_conf_destroy(conf);
    
    if (!self->client) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create client");
        return -1;
    }
    
    /* Initialize callback counter */
    self->counter = (CallbackCounter *)calloc(1, sizeof(CallbackCounter));
    if (self->counter) {
        tglog_py_mutex_init(&self->counter->lock);
    }
    
    /* Store configuration */
    self->host = strdup(host);
    self->port = port;
    self->destroyed = 0;
    
    return 0;
}

/* Send single message */
static PyObject *TglogClient_send(TglogClientObject *self, PyObject *args, PyObject *kwargs) {
    const char *message;
    Py_ssize_t message_len;
    int use_callback = 0;
    char msg_flag = 0x02;  /* Default to TGLOG_MSG_F_COPY for safety */

    static char *kwlist[] = {"message", "use_callback", "msg_flag", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#|pb", kwlist,
                                     &message, &message_len, &use_callback, &msg_flag)) {
        return NULL;
    }

    if (self->destroyed || !self->client) {
        PyErr_SetString(PyExc_RuntimeError, "Client has been destroyed");
        return NULL;
    }

    tglog_send_callback callback = use_callback ? fast_callback : NULL;
    void *user_data = use_callback ? self->counter : NULL;

    int result = tglog_client_send(self->client, message, callback, user_data, msg_flag);

    return PyLong_FromLong(result);
}


/* Get callback statistics */
static PyObject *TglogClient_get_stats(TglogClientObject *self, PyObject *Py_UNUSED(ignored)) {
    uint64_t success = 0;
    uint64_t error = 0;
    
    if (self->counter) {
        tglog_py_mutex_lock(&self->counter->lock);
        success = self->counter->success_count;
        error = self->counter->error_count;
        tglog_py_mutex_unlock(&self->counter->lock);
    }
    
    return Py_BuildValue("{s:K, s:K}", "sent", success, "error", error);
}

/* Get metrics */
static PyObject *TglogClient_get_metrics(TglogClientObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->destroyed || !self->client) {
        Py_RETURN_NONE;
    }

    char json_str[8192];
    int result = tglog_client_print_metrics(self->client, json_str, sizeof(json_str), true);
    if (result < 0) {
        Py_RETURN_NONE;
    }
    
    PyObject *py_result = PyUnicode_FromString(json_str);
    return py_result;
}

/* Destroy client */
static PyObject *TglogClient_destroy(TglogClientObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->client && !self->destroyed) {
        tglog_client_destroy(self->client);
        self->client = NULL;
        self->destroyed = 1;
    }
    Py_RETURN_NONE;
}

/* Method definitions */
static PyMethodDef TglogClient_methods[] = {
    {"send", (PyCFunction)TglogClient_send, METH_VARARGS | METH_KEYWORDS,
     "Send a single message"},
    {"get_stats", (PyCFunction)TglogClient_get_stats, METH_NOARGS,
     "Get callback statistics (success_count, error_count)"},
    {"get_metrics", (PyCFunction)TglogClient_get_metrics, METH_NOARGS,
     "Get client metrics as JSON string"},
    {"destroy", (PyCFunction)TglogClient_destroy, METH_NOARGS,
     "Destroy the client"},
    {NULL}
};

/* Type definition */
static PyTypeObject TglogClientType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tglog_ext.TglogClient",
    .tp_doc = "High-performance TgLog client",
    .tp_basicsize = sizeof(TglogClientObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)TglogClient_init,
    .tp_dealloc = (destructor)TglogClient_dealloc,
    .tp_methods = TglogClient_methods,
};

/* Module methods */
static PyObject *tglog_ext_version(PyObject *self, PyObject *Py_UNUSED(ignored)) {
    return PyUnicode_FromString("1.0.0");
}

static PyMethodDef module_methods[] = {
    {"version", tglog_ext_version, METH_NOARGS, "Get module version"},
    {NULL}
};

/* Module definition */
static struct PyModuleDef tglog_ext_module = {
    PyModuleDef_HEAD_INIT,
    "tglog_ext",
    "High-performance Python extension for TgLog",
    -1,
    module_methods
};

/* Module initialization */
PyMODINIT_FUNC PyInit_tglog_ext(void) {
    PyObject *m;
    
    /* Initialize global mutex */
    if (!g_lock_initialized) {
        tglog_py_mutex_init(&g_init_lock);
        g_lock_initialized = 1;
    }
    
    if (PyType_Ready(&TglogClientType) < 0) {
        return NULL;
    }
    
    m = PyModule_Create(&tglog_ext_module);
    if (m == NULL) {
        return NULL;
    }
    
    Py_INCREF(&TglogClientType);
    if (PyModule_AddObject(m, "TglogClient", (PyObject *)&TglogClientType) < 0) {
        Py_DECREF(&TglogClientType);
        Py_DECREF(m);
        return NULL;
    }
    
    /* Add message flag constants */
    PyModule_AddIntConstant(m, "MSG_F_NONE", 0x00);
    PyModule_AddIntConstant(m, "MSG_F_FREE", 0x01);
    PyModule_AddIntConstant(m, "MSG_F_COPY", 0x02);

    /* Add error codes */
    PyModule_AddIntConstant(m, "ERR_OK", TGLOG_ERR_OK);
    PyModule_AddIntConstant(m, "ERR_SEND_TIMEOUT", TGLOG_ERR_SEND_TIMEOUT);
    PyModule_AddIntConstant(m, "ERR_SEND_FAILED", TGLOG_ERR_SEND_FAILED);
    PyModule_AddIntConstant(m, "ERR_PRODUCER_CLOSED", TGLOG_ERR_PRODUCER_CLOSED);
    PyModule_AddIntConstant(m, "ERR_SEND_QUEUE_IS_FULL", TGLOG_ERR_SEND_QUEUE_IS_FULL);
    PyModule_AddIntConstant(m, "ERR_NO_AVAILABLE_CONN", TGLOG_ERR_NO_AVAILABLE_CONN);
    PyModule_AddIntConstant(m, "ERR_NEW_CONN_FAILED", TGLOG_ERR_NEW_CONN_FAILED);
    PyModule_AddIntConstant(m, "ERR_CONN_WRITE_FAILED", TGLOG_ERR_CONN_WRITE_FAILED);
    PyModule_AddIntConstant(m, "ERR_CONN_READ_FAILED", TGLOG_ERR_CONN_READ_FAILED);
    PyModule_AddIntConstant(m, "ERR_LOG_TOO_LONG", TGLOG_ERR_LOG_TOO_LONG);
    PyModule_AddIntConstant(m, "ERR_BAD_LOG", TGLOG_ERR_BAD_LOG);
    PyModule_AddIntConstant(m, "ERR_BATCH_SIZE_EXCEEDED", TGLOG_ERR_BATCH_SIZE_EXCEEDED);
    PyModule_AddIntConstant(m, "ERR_MEMORY_ALLOC_FAILED", TGLOG_ERR_MEMORY_ALLOC_FAILED);
    PyModule_AddIntConstant(m, "ERR_WORKER_BUSY", TGLOG_ERR_WORKER_BUSY);
    PyModule_AddIntConstant(m, "ERR_NO_MATCH_REQ_FOR_RSP", TGLOG_ERR_NO_MATCH_REQ_FOR_RSP);
    PyModule_AddIntConstant(m, "ERR_CONN_CLOSED_BY_PEER", TGLOG_ERR_CONN_CLOSED_BY_PEER);
    PyModule_AddIntConstant(m, "ERR_CONN_EAGAIN", TGLOG_ERR_CONN_EAGAIN);
    PyModule_AddIntConstant(m, "ERR_BAD_FLAG", TGLOG_ERR_BAD_FLAG);
    PyModule_AddIntConstant(m, "ERR_UNKNOWN", TGLOG_ERR_UNKNOWN);
    
    return m;
}