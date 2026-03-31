# cython: language_level=3
# distutils: language=c
# distutils: sources = ../../tglog/client.c ../../tglog/conf.c ../../tglog/conn.c ../../tglog/worker.c ../../tglog/endpoint.c ../../tglog/discoverer.c ../../tglog/request.c ../../tglog/metrics.c ../../tglog/debug_logger.c ../../tglog/err.c ../../tglog/def.c
# distutils: include_dirs = ../../common ../../thirdparty/event/include ../../tglog
# distutils: libraries = event pthread
# distutils: library_dirs = ../../lib ../../thirdparty/event/lib
# distutils: extra_compile_args = -O3 -march=native -ffast-math

"""
High-performance Cython bindings for tglog
Designed for big data applications with minimal overhead
"""

import platform
import sys

# Check platform - we only support Linux/Unix
if platform.system() == "Windows":
    print("Error: tglog is not supported on Windows platform.", file=sys.stderr)
    print("Please use Linux or macOS.", file=sys.stderr)
    sys.exit(1)

from libc.stdint cimport uint32_t, uint64_t
from libc.stdlib cimport malloc, free
from libc.string cimport memcpy, strlen
from libc.stddef cimport size_t
from cpython.bytes cimport PyBytes_AsString, PyBytes_Size
from cpython.mem cimport PyMem_Malloc, PyMem_Free
import threading
import time

# C type definitions
cdef extern from "conf.h":
    ctypedef enum tglog_network:
        TGLOG_NETWORK_UDP = 0
        TGLOG_NETWORK_TCP = 1
    
    ctypedef int (*can_log_func)(int level, void* user_data)
    ctypedef void (*write_log_func)(void* user_data, int level, const char* file, 
                                    int line, const char* func, const char* msg)
    
    ctypedef struct tglog_conf:
        tglog_network network
        char host[256]
        int port
        int update_interval
        int worker_num
        int batching_max_publish_delay
        int batching_max_messages
        int batching_max_size
        int max_pending_messages
        int conn_timeout
        int write_high_watermark
        int socket_send_buffer_size
        int socket_recv_buffer_size
        int send_timeout
        int max_retries
        int stop_timeout
        int max_conn_lifetime
        can_log_func can_log
        write_log_func write_log
        void* user_data
    
    tglog_conf* tglog_conf_create(tglog_network network, const char* host, int port,
                                  can_log_func can_log, write_log_func write_log, void* user_data)
    void tglog_conf_destroy(tglog_conf* conf)

cdef extern from "def.h":
    # Message flags
    char TGLOG_MSG_F_NONE
    char TGLOG_MSG_F_FREE
    char TGLOG_MSG_F_COPY

cdef extern from "client.h":
    ctypedef struct tglog_client:
        pass

    ctypedef struct tglog_metrics:
        pass

    ctypedef void (*tglog_send_callback)(const char* payload, int err_code,
                                         const char* err_msg, void* user_data) noexcept nogil

    void tglog_use_thread()
    tglog_client* tglog_client_create(tglog_conf* conf)
    void tglog_client_destroy(tglog_client* client)
    int tglog_client_send(tglog_client* client, const char* payload,
                          tglog_send_callback callback, void* user_data, char msg_flag) nogil
    int tglog_client_print_metrics(tglog_client* client, char* buffer, size_t size, bint json)

cdef extern from "tglog_atomic.h":
    void tglog_atomic_add_64(uint64_t* target, uint64_t value) nogil

cdef extern from "err.h":
    const char* tglog_get_err_msg(int code)
    
    # Error codes
    int TGLOG_ERR_OK
    int TGLOG_ERR_SEND_TIMEOUT
    int TGLOG_ERR_SEND_FAILED
    int TGLOG_ERR_PRODUCER_CLOSED
    int TGLOG_ERR_SEND_QUEUE_IS_FULL
    int TGLOG_ERR_NO_AVAILABLE_CONN
    int TGLOG_ERR_NEW_CONN_FAILED
    int TGLOG_ERR_CONN_WRITE_FAILED
    int TGLOG_ERR_CONN_READ_FAILED
    int TGLOG_ERR_LOG_TOO_LONG
    int TGLOG_ERR_BAD_LOG
    int TGLOG_ERR_BATCH_SIZE_EXCEEDED
    int TGLOG_ERR_MEMORY_ALLOC_FAILED
    int TGLOG_ERR_WORKER_BUSY
    int TGLOG_ERR_NO_MATCH_REQ_FOR_RSP
    int TGLOG_ERR_CONN_CLOSED_BY_PEER
    int TGLOG_ERR_CONN_EAGAIN
    int TGLOG_ERR_BAD_FLAG
    int TGLOG_ERR_UNKNOWN

# Global initialization
cdef int _initialized = 0
cdef object _init_lock = threading.Lock()

# Export message flags as Python constants
MSG_F_NONE = TGLOG_MSG_F_NONE
MSG_F_FREE = TGLOG_MSG_F_FREE
MSG_F_COPY = TGLOG_MSG_F_COPY

cdef void ensure_initialized():
    """Ensure tglog_use_thread is called once"""
    global _initialized
    if not _initialized:
        with _init_lock:
            if not _initialized:
                tglog_use_thread()
                _initialized = 1

# Fast callback counter for performance testing
cdef struct CallbackCounter:
    uint64_t success_count
    uint64_t error_count
    void* lock  # pthread_mutex_t

cdef void fast_callback(const char* payload, int err_code, const char* err_msg, void* user_data) noexcept nogil:
    """Ultra-fast C callback for performance testing"""
    cdef CallbackCounter* counter = <CallbackCounter*>user_data
    if counter != NULL:
        if err_code == 0:
            tglog_atomic_add_64(&counter.success_count, 1)
        else:
            tglog_atomic_add_64(&counter.error_count, 1)


# High-performance client wrapper
cdef class TglogClient:
    """High-performance tglog client with minimal overhead"""
    cdef:
        tglog_client* client
        tglog_conf* conf
        bytes host_bytes
        CallbackCounter* counter
        object lock
        int destroyed
        
    def __cinit__(self, str protocol="tcp", str host="127.0.0.1", 
                  int port=20001, int workers=8):
        ensure_initialized()
        self.client = NULL
        self.conf = NULL
        self.counter = NULL
        self.lock = threading.Lock()
        self.destroyed = 0
        
        # Convert protocol
        cdef tglog_network net_type = TGLOG_NETWORK_TCP
        if protocol.lower() == "udp":
            net_type = TGLOG_NETWORK_UDP
        
        # Create configuration
        self.host_bytes = host.encode('utf-8')
        self.conf = tglog_conf_create(net_type, PyBytes_AsString(self.host_bytes), 
                                      port, NULL, NULL, NULL)
        if not self.conf:
            raise RuntimeError("Failed to create configuration")
        
        # Set optimal performance settings
        self.conf.worker_num = workers
        self.conf.batching_max_publish_delay = 10
        self.conf.batching_max_messages = 50
        self.conf.batching_max_size = 32 * 1024
        self.conf.max_conn_lifetime = 300
        self.conf.max_pending_messages = 409600
        
        # Create client
        self.client = tglog_client_create(self.conf)
        tglog_conf_destroy(self.conf)
        self.conf = NULL
        
        if not self.client:
            raise RuntimeError("Failed to create client")
        
        # Initialize callback counter
        self.counter = <CallbackCounter*>malloc(sizeof(CallbackCounter))
        if self.counter:
            self.counter.success_count = 0
            self.counter.error_count = 0
            self.counter.lock = NULL
    
    def __dealloc__(self):
        self.destroy()
        if self.counter:
            free(self.counter)
    
    cpdef int send(self, bytes message, bint use_callback=False, char msg_flag=TGLOG_MSG_F_COPY):
        """Send single message with minimal overhead"""
        if self.destroyed or not self.client:
            return -1

        cdef:
            const char* msg_ptr = PyBytes_AsString(message)
            tglog_send_callback cb = <tglog_send_callback>fast_callback if use_callback else <tglog_send_callback>NULL
            void* user_data = <void*>self.counter if use_callback else NULL

        return tglog_client_send(self.client, msg_ptr, cb, user_data, msg_flag)
    
    
    cdef int send_raw(self, const char* message, int length, bint use_callback, char msg_flag=TGLOG_MSG_F_COPY) nogil:
        """Send raw message without any Python overhead (nogil)"""
        if self.destroyed or not self.client:
            return -1

        cdef:
            tglog_send_callback cb = <tglog_send_callback>fast_callback if use_callback else <tglog_send_callback>NULL
            void* user_data = <void*>self.counter if use_callback else NULL

        return tglog_client_send(self.client, message, cb, user_data, msg_flag)
    
    cpdef tuple get_callback_stats(self):
        """Get callback statistics"""
        if self.counter:
            return (self.counter.success_count, self.counter.error_count)
        return (0, 0)
    
    cpdef dict get_metrics(self):
        """Get client metrics"""
        if self.destroyed or not self.client:
            return {}

        cdef:
            char buffer[8192]
            int result_len

        # Call tglog_client_print_metrics with json=True
        result_len = tglog_client_print_metrics(self.client, buffer, 8192, True)

        # Check for error or buffer too small
        if result_len < 0 or result_len > 8192:
            return {}

        try:
            import json
            # Create Python bytes from buffer
            py_bytes = buffer[:result_len]
            result = json.loads(py_bytes.decode('utf-8'))
            return result
        except:
            return {}
    
    cpdef void destroy(self):
        """Destroy client and free resources"""
        with self.lock:
            if not self.destroyed and self.client:
                tglog_client_destroy(self.client)
                self.client = NULL
                self.destroyed = 1
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.destroy()


# Token bucket for rate limiting (Cython optimized)
cdef class TokenBucket:
    """High-performance token bucket"""
    cdef:
        long capacity
        long tokens
        int refill_rate
        double last_check
        object lock
        
    def __init__(self, int capacity, int refill_rate):
        self.capacity = capacity
        self.tokens = capacity
        self.refill_rate = refill_rate
        self.last_check = time.time()
        self.lock = threading.Lock()
    
    cpdef bint consume(self, int count=1):
        """Try to consume tokens"""
        cdef:
            double now = time.time()
            double elapsed = now - self.last_check
            long new_tokens
            
        with self.lock:
            if elapsed > 0:
                new_tokens = <long>(elapsed * self.refill_rate)
                self.tokens = min(self.tokens + new_tokens, self.capacity)
                self.last_check = now
            
            if self.tokens >= count:
                self.tokens -= count
                return True
            return False

# Convenience function for simple use cases
def send_logs(list messages, str protocol="tcp", str host="127.0.0.1", 
              int port=20001, int workers=8):
    """Simple function to send a batch of logs"""
    cdef TglogClient client = TglogClient(protocol, host, port, workers)
    cdef int sent = 0
    cdef bytes msg_bytes
    
    try:
        for msg in messages:
            if isinstance(msg, bytes):
                msg_bytes = msg
            else:
                msg_bytes = msg.encode('utf-8')
            if client.send(msg_bytes, False) == 0:
                sent += 1
        return sent
    finally:
        client.destroy()