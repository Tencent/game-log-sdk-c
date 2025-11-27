import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * High-level tglog client wrapper.
 * 
 * This class provides a clean, thread-safe Java API for the tglog library.
 * It handles configuration, lifecycle management, and provides both synchronous
 * and asynchronous sending capabilities.
 * 
 * Performance modes:
 * 1. Standard mode with callbacks: ~3.5s for 1M messages
 *    - new TglogClient(config)
 *    - sendAsync(message, callback)
 * 
 * 2. Performance mode without debug: ~3s for 1M messages  
 *    - new TglogClient(config, true)
 *    - sendAsync(message)
 * 
 * 3. DirectByteBuffer mode: ~1s for 1M messages
 *    - new TglogClient(network, host, port)
 *    - sendDirect(buffer, length)
 * 
 * Example usage:
 * <pre>
 * // High-performance mode
 * TglogClient client = new TglogClient(TglogConfig.NETWORK_TCP, "dev.tglog.com", 20001);
 * ByteBuffer buffer = TglogClient.createDirectBuffer("log message\n");
 * for (int i = 0; i < 1000000; i++) {
 *     buffer.position(0);  // Reset for reuse
 *     client.sendDirect(buffer, messageLength);
 * }
 * </pre>
 */
public class TglogClient implements AutoCloseable {

    // Message flags constants
    public static final byte MSG_FLAG_NONE = 0x00;  // The TGLog client will not copy and free the message
    public static final byte MSG_FLAG_FREE = 0x01;  // The TGLog client will free the message after sending
    public static final byte MSG_FLAG_COPY = 0x02;  // The TGLog client will copy the message and free it after sending

    private final TglogConfig config;
    private final long clientHandle;
    private final AtomicBoolean closed = new AtomicBoolean(false);
    private final AtomicLong sendCounter = new AtomicLong(0);
    
    
    // Static initialization
    static {
        // Initialize thread support once
        TglogJNI.useThread();
    }
    
    /**
     * Create a new tglog client with the specified configuration.
     * 
     * @param config Configuration for the client
     * @throws TglogException if client creation fails
     */
    public TglogClient(TglogConfig config) throws TglogException {
        this(config, false);
    }
    
    /**
     * Create a new tglog client with the specified configuration.
     * 
     * @param config Configuration for the client
     * @param performanceMode If true, skip debug callbacks for better performance
     * @throws TglogException if client creation fails
     */
    public TglogClient(TglogConfig config, boolean performanceMode) throws TglogException {
        this.config = config;
        
        long confHandle;
        if (performanceMode) {
            // Use simplified configuration without debug callbacks for maximum performance
            confHandle = TglogJNI.createConf(
                config.getNetwork(),
                config.getHost(), 
                config.getPort(),
                null,  // No canLogCallback
                null   // No writeLogCallback
            );
        } else {
            // Create configuration handle from TglogConfig object with all settings
            confHandle = TglogJNI.createConfFromConfig(config);
        }
        
        if (confHandle == 0) {
            throw new TglogException("Failed to create configuration");
        }
        
        try {
            // Create client handle
            this.clientHandle = TglogJNI.createClient(confHandle);
            if (this.clientHandle == 0) {
                throw new TglogException("Failed to create client");
            }
        } finally {
            // Always destroy the config handle as client makes a copy
            TglogJNI.destroyConf(confHandle);
        }
    }
    
    /**
     * Create a high-performance client with minimal configuration.
     * This constructor is optimized for maximum throughput.
     * 
     * @param network Network type (TglogConfig.NETWORK_TCP or NETWORK_UDP)
     * @param host Server hostname or IP
     * @param port Server port
     * @throws TglogException if client creation fails
     */
    public TglogClient(int network, String host, int port) throws TglogException {
        this.config = new TglogConfig(network, host, port);
        
        // Use simplified configuration for maximum performance
        long confHandle = TglogJNI.createConf(network, host, port, null, null);
        if (confHandle == 0) {
            throw new TglogException("Failed to create configuration");
        }
        
        try {
            // Create client handle
            this.clientHandle = TglogJNI.createClient(confHandle);
            if (this.clientHandle == 0) {
                throw new TglogException("Failed to create client");
            }
        } finally {
            // Always destroy the config handle as client makes a copy
            TglogJNI.destroyConf(confHandle);
        }
    }
    
    /**
     * Send a log message asynchronously with message flags.
     *
     * @param logPayload The log message to send
     * @param callback Callback to handle the result
     * @param msgFlags Message flags (MSG_FLAG_NONE, MSG_FLAG_FREE, or MSG_FLAG_COPY)
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendAsync(String logPayload, TglogCallback callback, byte msgFlags) {
        checkNotClosed();
        
        if (logPayload == null || logPayload.trim().isEmpty()) {
            CompletableFuture<Void> future = new CompletableFuture<>();
            future.completeExceptionally(new IllegalArgumentException("Log payload cannot be null or empty"));
            return future;
        }
        
        // Ensure payload ends with newline
        if (!logPayload.endsWith("\n")) {
            logPayload += "\n";
        }
        
        CompletableFuture<Void> future = new CompletableFuture<>();
        
        TglogCallback wrappedCallback = new TglogCallback() {
            @Override
            public void onSuccess(String payload) {
                if (callback != null) {
                    try {
                        callback.onSuccess(payload);
                    } catch (Exception e) {
                        // Log callback exception but don't fail the future
                        System.err.println("Callback exception: " + e.getMessage());
                    }
                }
                future.complete(null);
            }
            
            @Override
            public void onError(String payload, int errorCode, String errorMessage) {
                if (callback != null) {
                    try {
                        callback.onError(payload, errorCode, errorMessage);
                    } catch (Exception e) {
                        // Log callback exception but don't fail the future
                        System.err.println("Callback exception: " + e.getMessage());
                    }
                }
                future.completeExceptionally(new TglogException(errorMessage, errorCode));
            }
        };
        
        int result = TglogJNI.sendAsync(clientHandle, logPayload, wrappedCallback, msgFlags);
        if (result != TglogJNI.TGLOG_ERR_OK) {
            String errorMsg = TglogJNI.getErrorMessage(result);
            future.completeExceptionally(new TglogException("Send failed immediately: " + errorMsg, result));
        } else {
            sendCounter.incrementAndGet();
        }
        
        return future;
    }
    
    /**
     * Send a log message asynchronously (backward compatibility).
     *
     * @param logPayload The log message to send
     * @param callback Callback to handle the result
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendAsync(String logPayload, TglogCallback callback) {
        return sendAsync(logPayload, callback, MSG_FLAG_COPY);  // Default to COPY for backward compatibility
    }

    /**
     * Send a log message asynchronously without a callback.
     *
     * @param logPayload The log message to send
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendAsync(String logPayload) {
        return sendAsync(logPayload, null, MSG_FLAG_COPY);
    }
    
    /**
     * Send a log message using DirectByteBuffer for better performance.
     * This method provides zero-copy sending when possible.
     * 
     * Note: This is a fire-and-forget method without callbacks for maximum performance.
     * Use sendDirectAsync() if you need delivery confirmation.
     * 
     * @param buffer DirectByteBuffer containing the log message
     * @param offset Offset in buffer where message starts
     * @param length Length of the message
     * @throws TglogException if send fails immediately
     */
    public void sendDirect(ByteBuffer buffer, int offset, int length) throws TglogException {
        checkNotClosed();
        
        if (buffer == null) {
            throw new IllegalArgumentException("Buffer cannot be null");
        }
        
        if (!buffer.isDirect()) {
            throw new IllegalArgumentException("Buffer must be a DirectByteBuffer");
        }
        
        if (offset < 0 || length <= 0 || offset + length > buffer.capacity()) {
            throw new IllegalArgumentException("Invalid offset or length");
        }
        
        int result = TglogJNI.sendDirectBuffer(clientHandle, buffer, offset, length, MSG_FLAG_NONE);
        if (result != TglogJNI.TGLOG_ERR_OK) {
            String errorMsg = TglogJNI.getErrorMessage(result);
            throw new TglogException("Send failed: " + errorMsg, result);
        }
        
        sendCounter.incrementAndGet();
    }
    
    /**
     * Send a log message using DirectByteBuffer with callback support.
     * This method combines zero-copy performance with delivery confirmation.
     * 
     * @param buffer DirectByteBuffer containing the log message
     * @param offset Offset in buffer where message starts
     * @param length Length of the message
     * @param callback Callback to handle the result
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendDirectAsync(ByteBuffer buffer, int offset, int length, TglogCallback callback) {
        checkNotClosed();
        
        if (buffer == null) {
            throw new IllegalArgumentException("Buffer cannot be null");
        }
        
        if (!buffer.isDirect()) {
            throw new IllegalArgumentException("Buffer must be a DirectByteBuffer");
        }
        
        if (offset < 0 || length <= 0 || offset + length > buffer.capacity()) {
            throw new IllegalArgumentException("Invalid offset or length");
        }
        
        CompletableFuture<Void> future = new CompletableFuture<>();
        
        TglogCallback wrappedCallback = new TglogCallback() {
            @Override
            public void onSuccess(String payload) {
                if (callback != null) {
                    try {
                        callback.onSuccess(payload);
                    } catch (Exception e) {
                        // Log callback exception but don't fail the future
                        System.err.println("Callback exception: " + e.getMessage());
                    }
                }
                future.complete(null);
            }
            
            @Override
            public void onError(String payload, int errorCode, String errorMessage) {
                if (callback != null) {
                    try {
                        callback.onError(payload, errorCode, errorMessage);
                    } catch (Exception e) {
                        // Log callback exception but don't fail the future
                        System.err.println("Callback exception: " + e.getMessage());
                    }
                }
                future.completeExceptionally(new TglogException(errorMessage, errorCode));
            }
        };
        
        int result = TglogJNI.sendDirectBufferAsync(clientHandle, buffer, offset, length, wrappedCallback, MSG_FLAG_NONE);
        if (result != TglogJNI.TGLOG_ERR_OK) {
            String errorMsg = TglogJNI.getErrorMessage(result);
            future.completeExceptionally(new TglogException("Send failed immediately: " + errorMsg, result));
        } else {
            sendCounter.incrementAndGet();
        }
        
        return future;
    }
    
    /**
     * Send a log message using DirectByteBuffer with callback support (simplified version).
     * Assumes the message starts at position 0.
     * 
     * @param buffer DirectByteBuffer containing the log message
     * @param length Length of the message
     * @param callback Callback to handle the result
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendDirectAsync(ByteBuffer buffer, int length, TglogCallback callback) {
        return sendDirectAsync(buffer, 0, length, callback);
    }
    
    /**
     * Send a log message using DirectByteBuffer with callback support (no user callback).
     * 
     * @param buffer DirectByteBuffer containing the log message
     * @param offset Offset in buffer where message starts
     * @param length Length of the message
     * @return A CompletableFuture that completes when the message is sent
     */
    public CompletableFuture<Void> sendDirectAsync(ByteBuffer buffer, int offset, int length) {
        return sendDirectAsync(buffer, offset, length, null);
    }
    
    /**
     * Send a log message using DirectByteBuffer (simplified version).
     * Assumes the message starts at position 0.
     * 
     * @param buffer DirectByteBuffer containing the log message
     * @param length Length of the message
     * @throws TglogException if send fails immediately
     */
    public void sendDirect(ByteBuffer buffer, int length) throws TglogException {
        sendDirect(buffer, 0, length);
    }
    
    /**
     * Send a log message using DirectByteBuffer with automatic buffer management.
     * This method creates a DirectByteBuffer internally for better performance.
     * The buffer is NOT reusable - for bulk sending, use sendDirect(ByteBuffer, int, int).
     * 
     * @param logPayload The log message to send
     * @throws TglogException if send fails immediately
     */
    public void sendDirectString(String logPayload) throws TglogException {
        checkNotClosed();
        
        if (logPayload == null || logPayload.trim().isEmpty()) {
            throw new IllegalArgumentException("Log payload cannot be null or empty");
        }
        
        // Ensure payload ends with newline
        if (!logPayload.endsWith("\n")) {
            logPayload += "\n";
        }
        
        byte[] bytes = logPayload.getBytes(StandardCharsets.UTF_8);
        
        // Create DirectByteBuffer with extra byte for null terminator
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length + 1);
        buffer.put(bytes);
        buffer.put((byte)0); // Add null terminator for zero-copy
        buffer.flip();
        
        sendDirect(buffer, bytes.length);
    }
    
    /**
     * Create a reusable DirectByteBuffer for a specific log message.
     * This is useful for sending the same message multiple times.
     * 
     * @param logPayload The log message
     * @return A DirectByteBuffer containing the message, ready for sending
     */
    public static ByteBuffer createDirectBuffer(String logPayload) {
        if (logPayload == null || logPayload.trim().isEmpty()) {
            throw new IllegalArgumentException("Log payload cannot be null or empty");
        }
        
        // Ensure payload ends with newline
        if (!logPayload.endsWith("\n")) {
            logPayload += "\n";
        }
        
        byte[] bytes = logPayload.getBytes(StandardCharsets.UTF_8);
        
        // Create DirectByteBuffer with extra byte for null terminator
        ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.length + 1);
        buffer.put(bytes);
        buffer.put((byte)0); // Add null terminator for zero-copy
        buffer.flip();
        
        return buffer;
    }
    
    /**
     * Get current client statistics.
     * 
     * @return Statistics object
     */
    public TglogStats getStats() {
        checkNotClosed();
        return TglogJNI.getStats(clientHandle);
    }
    
    /**
     * Get metrics as JSON string.
     * 
     * @return JSON string containing metrics
     */
    public String getMetrics() {
        checkNotClosed();
        return TglogJNI.getMetrics(clientHandle);
    }
    
    /**
     * Check if the client is healthy.
     * Note: This is a simplified implementation since detailed health checking
     * has been removed from the simplified JNI interface.
     * 
     * @return true if not closed, false otherwise
     */
    public boolean isHealthy() {
        return !closed.get();
    }
    
    /**
     * Get the number of pending messages in the queue.
     * Note: This is estimated from statistics since detailed pending count
     * has been removed from the simplified JNI interface.
     * 
     * @return Estimated number of pending messages
     */
    public int getPendingCount() {
        checkNotClosed();
        TglogStats stats = TglogJNI.getStats(clientHandle);
        if (stats != null) {
            // Estimate pending as enqueued - (succeeded + failed)
            long pending = stats.getEnqueuedMsgCount() - 
                          (stats.getSucceededMsgCount() + stats.getFailedMsgCount());
            return (int) Math.max(0, pending);
        }
        return 0;
    }
    
    /**
     * Get the total number of messages sent through this client.
     * 
     * @return Total send count
     */
    public long getSendCount() {
        return sendCounter.get();
    }
    
    /**
     * Get the configuration used by this client.
     * 
     * @return Configuration object
     */
    public TglogConfig getConfig() {
        return config;
    }
    
    /**
     * Wait for all pending messages to be sent.
     * 
     * @param timeout Maximum time to wait
     * @param unit Time unit
     * @return true if all messages were sent, false if timeout occurred
     * @throws InterruptedException if interrupted while waiting
     */
    public boolean waitForCompletion(long timeout, TimeUnit unit) throws InterruptedException {
        long endTime = System.nanoTime() + unit.toNanos(timeout);
        
        while (System.nanoTime() < endTime) {
            if (getPendingCount() == 0) {
                return true;
            }
            Thread.sleep(10); // 10ms polling interval
        }
        
        return false;
    }
    
    
    /**
     * Close the client and release all resources.
     */
    @Override
    public void close() {
        if (closed.compareAndSet(false, true)) {
            // Destroy native client
            TglogJNI.destroyClient(clientHandle);
        }
    }
    
    private void checkNotClosed() {
        if (closed.get()) {
            throw new IllegalStateException("Client has been closed");
        }
    }
    
    @Override
    public String toString() {
        return String.format("TglogClient{config=%s, sendCount=%d, pending=%d, healthy=%s}",
                           config, getSendCount(), 
                           closed.get() ? 0 : getPendingCount(),
                           isHealthy());
    }
}