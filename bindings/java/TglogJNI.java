/**
 * Low-level JNI interface for tglog library.
 * 
 * This class provides direct JNI bindings to the tglog C library.
 * For most use cases, consider using the high-level TglogClient class instead.
 * 
 * Thread Safety: All methods in this class are thread-safe and can be called
 * concurrently from multiple threads, except for resource management methods
 * (destroyConf, destroyClient) which should be called only once per handle.
 */
public class TglogJNI {
    
    // Handle constants
    public static final long INVALID_HANDLE = 0;
    
    // Network type constants
    public static final int TGLOG_NETWORK_UDP = 0;
    public static final int TGLOG_NETWORK_TCP = 1;
    
    // Error codes
    public static final int TGLOG_ERR_OK = 0;
    public static final int TGLOG_ERR_SEND_TIMEOUT = 10001;
    public static final int TGLOG_ERR_SEND_FAILED = 10002;
    public static final int TGLOG_ERR_PRODUCER_CLOSED = 10003;
    public static final int TGLOG_ERR_SEND_QUEUE_IS_FULL = 10004;
    public static final int TGLOG_ERR_NO_AVAILABLE_CONN = 10005;
    public static final int TGLOG_ERR_NEW_CONN_FAILED = 10006;
    public static final int TGLOG_ERR_CONN_WRITE_FAILED = 10007;
    public static final int TGLOG_ERR_CONN_READ_FAILED = 10008;
    public static final int TGLOG_ERR_LOG_TOO_LONG = 10009;
    public static final int TGLOG_ERR_BAD_LOG = 10010;
    public static final int TGLOG_ERR_BATCH_SIZE_EXCEEDED = 10011;
    public static final int TGLOG_ERR_MEMORY_ALLOC_FAILED = 10012;
    public static final int TGLOG_ERR_WORKER_BUSY = 10013;
    public static final int TGLOG_ERR_NO_MATCH_REQ_FOR_RSP = 10014;
    public static final int TGLOG_ERR_CONN_CLOSED_BY_PEER = 10015;
    public static final int TGLOG_ERR_CONN_EAGAIN = 10016;
    public static final int TGLOG_ERR_BAD_FLAG = 10017;
    public static final int TGLOG_ERR_UNKNOWN = 20001;
    
    // Load the native library
    static {
        System.loadLibrary("tglog_jni");
    }
    
    // Native methods
    
    /**
     * Initialize thread support for tglog.
     * This is automatically called when TglogClient is loaded.
     * Safe to call multiple times - subsequent calls are ignored.
     */
    public static native void useThread();
    
    /**
     * Create a tglog configuration
     * @param network Network type (TGLOG_NETWORK_UDP or TGLOG_NETWORK_TCP)
     * @param host Server IP/domain name (must not be null)
     * @param port Server port (must be positive)
     * @param canLogCallback Callback to filter debug log messages (can be null for default)
     * @param writeLogCallback Callback to write debug log messages (can be null for default)
     * @return Configuration handle (non-zero on success, INVALID_HANDLE on failure)
     * @throws IllegalArgumentException if host is null or port is invalid
     */
    public static native long createConf(int network, String host, int port, 
                                        TglogCanLogCallback canLogCallback, 
                                        TglogWriteLogCallback writeLogCallback);
    
    /**
     * Create a tglog configuration from TglogConfig object
     * @param config TglogConfig object containing all configuration parameters (must not be null)
     * @return Configuration handle (non-zero on success, INVALID_HANDLE on failure)
     * @throws IllegalArgumentException if config is null or contains invalid parameters
     */
    public static native long createConfFromConfig(TglogConfig config);
    
    /**
     * Create a tglog client
     * @param confHandle Configuration handle (must be valid, non-zero)
     * @return Client handle (non-zero on success, INVALID_HANDLE on failure)
     * @throws IllegalArgumentException if confHandle is INVALID_HANDLE
     */
    public static native long createClient(long confHandle);
    
    /**
     * Destroy a configuration and release its resources.
     * Note: This method should be called only once per handle.
     * @param confHandle Configuration handle (INVALID_HANDLE is ignored)
     */
    public static native void destroyConf(long confHandle);
    
    /**
     * Send a log message asynchronously with callback
     * @param clientHandle Client handle (must be valid, non-zero)
     * @param logPayload Log message payload (must not be null)
     * @param callback Callback object to handle result (can be null for fire-and-forget behavior)
     * @param msgFlag Message flag (0x00=NONE, 0x01=FREE, 0x02=COPY)
     * @return TGLOG_ERR_OK on success (queued), error code on immediate failure
     * @throws IllegalArgumentException if clientHandle is INVALID_HANDLE or logPayload is null
     *
     * Note: When callback is null, the operation becomes fire-and-forget - success/failure
     * notifications will not be delivered, but no resource leaks will occur.
     */
    public static native int sendAsync(long clientHandle, String logPayload, TglogCallback callback, byte msgFlag);
    
    /**
     * Get metrics as JSON string
     * @param clientHandle Client handle (must be valid, non-zero)
     * @return JSON string containing metrics, or null if handle is invalid
     * @throws IllegalArgumentException if clientHandle is INVALID_HANDLE
     */
    public static native String getMetrics(long clientHandle);
    
    /**
     * Destroy a client and release its resources.
     * Note: This method should be called only once per handle.
     * @param clientHandle Client handle (INVALID_HANDLE is ignored)
     */
    public static native void destroyClient(long clientHandle);
    
    /**
     * Get human-readable error message for error code
     * @param errorCode Error code (one of TGLOG_ERR_* constants)
     * @return Error message string (never null, returns "Unknown error" for invalid codes)
     */
    public static native String getErrorMessage(int errorCode);
    
    /**
     * Get detailed client statistics
     * @param clientHandle Client handle (must be valid, non-zero)
     * @return Statistics object with detailed counters, or null if handle is invalid
     * @throws IllegalArgumentException if clientHandle is INVALID_HANDLE
     */
    public static native TglogStats getStats(long clientHandle);
    
    /**
     * Send log using DirectByteBuffer for better performance (zero-copy)
     * @param clientHandle Client handle (must be valid, non-zero)
     * @param buffer DirectByteBuffer containing the log message
     * @param offset Offset in buffer where message starts
     * @param length Length of the message
     * @param msgFlag Message flag (0x00=NONE, 0x01=FREE, 0x02=COPY)
     * @return TGLOG_ERR_OK on success (queued), error code on immediate failure
     * @throws IllegalArgumentException if clientHandle is INVALID_HANDLE or buffer is null
     *
     * Note: This method provides better performance than sendAsync by avoiding string copies
     * but does not provide delivery confirmation. Use sendDirectBufferAsync for callbacks.
     */
    public static native int sendDirectBuffer(long clientHandle, java.nio.ByteBuffer buffer, int offset, int length, byte msgFlag);
    
    /**
     * Send log using DirectByteBuffer with callback support
     * @param clientHandle Client handle (must be valid, non-zero)
     * @param buffer DirectByteBuffer containing the log message
     * @param offset Offset in buffer where message starts
     * @param length Length of the message
     * @param callback Callback for delivery confirmation (can be null for fire-and-forget)
     * @param msgFlag Message flag (0x00=NONE, 0x01=FREE, 0x02=COPY)
     * @return TGLOG_ERR_OK on success (queued), error code on immediate failure
     * @throws IllegalArgumentException if clientHandle is INVALID_HANDLE or buffer is null
     *
     * Note: This method combines the performance benefits of DirectByteBuffer (zero-copy when possible)
     * with the reliability of callbacks for production use. Recommended for high-throughput scenarios
     * where delivery confirmation is needed.
     */
    public static native int sendDirectBufferAsync(long clientHandle, java.nio.ByteBuffer buffer, int offset, int length, TglogCallback callback, byte msgFlag);
}