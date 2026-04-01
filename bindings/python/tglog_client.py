#!/usr/bin/env python3
"""
Python bindings for tglog C library using ctypes
Provides high-performance client for big data applications
"""

import ctypes
import os
import sys
import json
import threading
from ctypes import POINTER, c_char_p, c_int, c_void_p, c_bool, Structure, CFUNCTYPE, c_uint32


def _get_lib_name():
    if sys.platform == "win32":
        return "tglog.dll"
    elif sys.platform == "darwin":
        return "libtglog.dylib"
    else:
        return "libtglog.so"


# Load the tglog library
_lib_name = _get_lib_name()

if sys.platform == "win32":
    _lib_dirs = [
        os.path.abspath(os.path.join(os.path.dirname(__file__), "../lib")),
        os.path.abspath(os.path.join(os.path.dirname(__file__), "../../lib")),
        os.path.abspath(os.path.dirname(__file__))
    ]
    for _d in _lib_dirs:
        if os.path.exists(_d):
            if hasattr(os, "add_dll_directory"):
                try:
                    os.add_dll_directory(_d)
                except Exception:
                    pass
            os.environ["PATH"] = _d + os.pathsep + os.environ.get("PATH", "")

lib_paths = [
    "../lib/" + _lib_name,
    "./" + _lib_name,
    os.path.join(os.path.dirname(__file__), "../lib/" + _lib_name),
    os.path.join(os.path.dirname(__file__), "../../lib/" + _lib_name),
]

tglog_lib = None
for path in lib_paths:
    try:
        if os.path.exists(path):
            tglog_lib = ctypes.CDLL(path)
            break
    except OSError:
        continue

if not tglog_lib:
    try:
        tglog_lib = ctypes.CDLL(_lib_name)
    except OSError as e:
        raise ImportError(f"Could not load {_lib_name}: {e}")

# Constants
TGLOG_NETWORK_UDP = 0
TGLOG_NETWORK_TCP = 1

# Message flag
TGLOG_MSG_F_NONE = 0x00  # The TGLog client will not copy and free the message
TGLOG_MSG_F_FREE = 0x01  # The TGLog client will free the message after sending
TGLOG_MSG_F_COPY = 0x02  # The TGLog client will copy the message and free it after sending

# Error codes
TGLOG_ERR_OK = 0
TGLOG_ERR_SEND_TIMEOUT = 10001
TGLOG_ERR_SEND_FAILED = 10002
TGLOG_ERR_PRODUCER_CLOSED = 10003
TGLOG_ERR_SEND_QUEUE_IS_FULL = 10004
TGLOG_ERR_NO_AVAILABLE_CONN = 10005
TGLOG_ERR_NEW_CONN_FAILED = 10006
TGLOG_ERR_CONN_WRITE_FAILED = 10007
TGLOG_ERR_CONN_READ_FAILED = 10008
TGLOG_ERR_LOG_TOO_LONG = 10009
TGLOG_ERR_BAD_LOG = 10010
TGLOG_ERR_BATCH_SIZE_EXCEEDED = 10011
TGLOG_ERR_MEMORY_ALLOC_FAILED = 10012
TGLOG_ERR_WORKER_BUSY = 10013
TGLOG_ERR_NO_MATCH_REQ_FOR_RSP = 10014
TGLOG_ERR_CONN_CLOSED_BY_PEER = 10015
TGLOG_ERR_CONN_EAGAIN = 10016
TGLOG_ERR_BAD_FLAG = 10017
TGLOG_ERR_UNKNOWN = 20001

# C structures
class TglogConf(Structure):
    pass

class TglogClient(Structure):
    pass

class TglogMetrics(Structure):
    pass

# Callback type - simplified for performance
SendCallbackFunc = CFUNCTYPE(None, c_char_p, c_int, c_char_p, c_void_p)

# Function prototypes - only what we need for performance
tglog_lib.tglog_use_thread.restype = None
tglog_lib.tglog_use_thread.argtypes = []

tglog_lib.tglog_conf_create.restype = POINTER(TglogConf)
tglog_lib.tglog_conf_create.argtypes = [c_int, c_char_p, c_int, c_void_p, c_void_p, c_void_p]

tglog_lib.tglog_conf_destroy.restype = None
tglog_lib.tglog_conf_destroy.argtypes = [POINTER(TglogConf)]

tglog_lib.tglog_client_create.restype = POINTER(TglogClient)
tglog_lib.tglog_client_create.argtypes = [POINTER(TglogConf)]

tglog_lib.tglog_client_destroy.restype = None
tglog_lib.tglog_client_destroy.argtypes = [POINTER(TglogClient)]

tglog_lib.tglog_client_send.restype = c_int
tglog_lib.tglog_client_send.argtypes = [POINTER(TglogClient), c_char_p, c_void_p, c_void_p, ctypes.c_char]

# Print metrics to buffer
tglog_lib.tglog_client_print_metrics.restype = c_int
tglog_lib.tglog_client_print_metrics.argtypes = [POINTER(TglogClient), c_char_p, ctypes.c_size_t, c_bool]

# Global initialization
_initialized = False
_init_lock = threading.Lock()

# Global high-performance callback counter (shared across instances)
_global_counter = {'sent': 0, 'error': 0}
_global_counter_lock = threading.Lock()

# Pre-create a global fast callback for performance
def _global_callback_func(payload, err_code, err_msg, user_data):
    """Global callback that just counts - no Python overhead"""
    with _global_counter_lock:
        if err_code == 0:
            _global_counter['sent'] += 1
        else:
            _global_counter['error'] += 1

# Create the C callback once at module level
_global_callback = SendCallbackFunc(_global_callback_func)
_global_callback_ptr = ctypes.cast(_global_callback, c_void_p)

def ensure_initialized():
    global _initialized
    if not _initialized:
        with _init_lock:
            if not _initialized:
                tglog_lib.tglog_use_thread()
                _initialized = True

def reset_global_counter():
    """Reset the global counter"""
    with _global_counter_lock:
        _global_counter['sent'] = 0
        _global_counter['error'] = 0

def get_global_counter():
    """Get the global counter values"""
    with _global_counter_lock:
        return _global_counter['sent'], _global_counter['error']

class TglogClientPy:
    """
    Python wrapper for tglog C library client
    Provides high-performance logging for big data applications
    """
    
    def __init__(self, protocol="tcp", host="dev.tglog.com", port=20001, workers=8):
        ensure_initialized()
        
        self._client = None
        self._destroyed = False
        self._lock = threading.Lock()
        
        # Pre-encode host for performance
        host_bytes = host.encode('utf-8')
        network = TGLOG_NETWORK_TCP if protocol.lower() == "tcp" else TGLOG_NETWORK_UDP
        
        # Create configuration with optimal settings
        conf = tglog_lib.tglog_conf_create(
            network, host_bytes, port, None, None, None
        )
        
        if not conf:
            raise RuntimeError("Failed to create configuration")
        
        # Set performance optimizations directly on conf struct
        # Note: We can't modify the struct directly with ctypes,
        # but the C library sets good defaults
        
        # Create client
        self._client = tglog_lib.tglog_client_create(conf)
        tglog_lib.tglog_conf_destroy(conf)
        
        if not self._client:
            raise RuntimeError("Failed to create client")
        
        # Pre-create callback for counting (if needed)
        self._callback_count = {'sent': 0, 'error': 0}
        self._callback_lock = threading.Lock()
        
        # Keep callback alive by storing it as instance variable
        self._callback = None
        self._callback_ptr = None
        
        # Store user callbacks and their wrappers
        self._user_callbacks = {}
        self._callback_wrappers = {}
        self._callback_lock_map = threading.Lock()
    
    def _ensure_callback(self):
        """Create callback if not exists"""
        if self._callback is None:
            def callback_func(payload, err_code, err_msg, user_data):
                with self._callback_lock:
                    if err_code == 0:
                        self._callback_count['sent'] += 1
                    else:
                        self._callback_count['error'] += 1
            
            self._callback = SendCallbackFunc(callback_func)
            self._callback_ptr = ctypes.cast(self._callback, c_void_p)
    
    def send(self, message, callback=None, msg_flag=TGLOG_MSG_F_COPY):
        """
        Send a message with optional callback

        Args:
            message: The message to send (str or bytes)
            callback: Can be:
                - None: Fire and forget (fastest)
                - True: Use internal counter callback
                - "global": Use global high-performance counter
                - callable: Custom Python function(payload, err_code, err_msg)
                - ctypes callback: Raw C callback function
            msg_flag: Message flag (default: TGLOG_MSG_F_COPY for safety)
                - TGLOG_MSG_F_NONE: No memory management (use with caution)
                - TGLOG_MSG_F_FREE: Free the message after sending (not for Python strings)
                - TGLOG_MSG_F_COPY: Copy the message (safest, default)

        Returns:
            0 on success, error code otherwise
        """
        if self._destroyed:
            return -1
        
        # Convert to bytes if string
        if isinstance(message, str):
            message_bytes = message.encode('utf-8')
        else:
            message_bytes = message
        
        callback_ptr = None
        callback_ref = None
        
        if callback is True:
            # Use internal counter
            self._ensure_callback()
            callback_ptr = self._callback_ptr
        elif callback == "global":
            # Use global high-performance counter
            callback_ptr = _global_callback_ptr
        elif callable(callback) and not isinstance(callback, ctypes._CFuncPtr):
            # Custom Python callback - reuse wrapper if callback is the same
            with self._callback_lock_map:
                # Use id of callback function as key for reuse
                callback_id = id(callback)
                
                if callback_id not in self._callback_wrappers:
                    # Create wrapper function only once per unique callback
                    def wrapper(payload, err_code, err_msg, user_data):
                        try:
                            payload_str = payload.decode('utf-8') if payload else ""
                            err_msg_str = err_msg.decode('utf-8') if err_msg else ""
                            callback(payload_str, err_code, err_msg_str)
                        except Exception as e:
                            print(f"Error in callback: {e}", file=sys.stderr)
                    
                    # Create C function pointer
                    callback_ref = SendCallbackFunc(wrapper)
                    
                    # Store for reuse
                    self._user_callbacks[callback_id] = callback
                    self._callback_wrappers[callback_id] = callback_ref
                else:
                    # Reuse existing wrapper
                    callback_ref = self._callback_wrappers[callback_id]
                
                callback_ptr = ctypes.cast(callback_ref, c_void_p)
        elif isinstance(callback, ctypes._CFuncPtr):
            # Direct C callback
            callback_ptr = ctypes.cast(callback, c_void_p)
            
        return tglog_lib.tglog_client_send(
            self._client, message_bytes, callback_ptr, None, ctypes.c_char(msg_flag)
        )
    
    
    def get_stats(self):
        """Get callback statistics"""
        with self._callback_lock:
            return self._callback_count['sent'], self._callback_count['error']
    
    def get_metrics(self):
        """Get client metrics"""
        if self._destroyed or not self._client:
            return {}

        # Create buffer for metrics (8192 bytes for JSON)
        buffer = ctypes.create_string_buffer(8192)
        result = tglog_lib.tglog_client_print_metrics(self._client, buffer, 8192, True)

        if result <= 0:
            return {}

        try:
            # Parse JSON from buffer
            json_str = buffer.value.decode('utf-8')
            return json.loads(json_str)
        except Exception:
            return {}
    
    def destroy(self):
        """Destroy the client"""
        with self._lock:
            if not self._destroyed and self._client:
                tglog_lib.tglog_client_destroy(self._client)
                self._client = None
                self._destroyed = True
                
                # Clear callback references to allow GC
                with self._callback_lock_map:
                    self._user_callbacks.clear()
                    self._callback_wrappers.clear()
    
    def __enter__(self):
        return self
    
    def __exit__(self, *args):
        self.destroy()
    
    def __del__(self):
        self.destroy()



