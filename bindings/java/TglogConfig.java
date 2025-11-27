/**
 * Configuration class for tglog client.
 * 
 * This class encapsulates all configuration parameters for the tglog client,
 * providing a clean Java API with reasonable defaults.
 */
public class TglogConfig {
    
    // Network type constants
    public static final int NETWORK_UDP = 0;
    public static final int NETWORK_TCP = 1;
    
    // Configuration fields
    private int network = NETWORK_UDP;
    private String host;
    private int port;
    private int updateInterval = 60;          // seconds
    private int connTimeout = 3000;           // ms
    private int writeHighWatermark = 32 * 1024 * 1024;  // 32MB - stop writing when buffered data exceeds this
    private int socketSendBufferSize = 64 * 1024 * 1024;  // 64MB
    private int socketRecvBufferSize = 16 * 1024;         // 16KB
    private int maxConnLifetime = 300;        // seconds
    private int workerNum = 8;                // threads
    private int batchingMaxPublishDelay = 10; // ms
    private int batchingMaxMessages = 50;     // messages
    private int batchingMaxSize = 32 * 1024;  // 32KB
    private int maxPendingMessages = 409600;  // messages per worker
    private int sendTimeout = 30000;          // ms - how long can a message stay in the client
    private int maxRetries = 3;               // retries
    private int stopTimeout = 3000;           // ms
    
    // Debug logging callbacks
    private TglogCanLogCallback canLogCallback;    // debug log filter callback
    private TglogWriteLogCallback writeLogCallback; // debug log write callback
    
    /**
     * Create a new configuration with required parameters.
     * 
     * @param network Network type (NETWORK_UDP or NETWORK_TCP)
     * @param host Server hostname or IP address
     * @param port Server port number
     * @throws IllegalArgumentException if parameters are invalid
     */
    public TglogConfig(int network, String host, int port) {
        if (network != NETWORK_UDP && network != NETWORK_TCP) {
            throw new IllegalArgumentException("Invalid network type: " + network);
        }
        if (host == null || host.trim().isEmpty()) {
            throw new IllegalArgumentException("Host cannot be null or empty");
        }
        if (port <= 0 || port > 65535) {
            throw new IllegalArgumentException("Port must be between 1 and 65535");
        }
        this.network = network;
        this.host = host;
        this.port = port;
    }
    
    // Getters and setters
    
    public int getNetwork() { return network; }
    public TglogConfig setNetwork(int network) { this.network = network; return this; }
    
    public String getHost() { return host; }
    public TglogConfig setHost(String host) { this.host = host; return this; }
    
    public int getPort() { return port; }
    public TglogConfig setPort(int port) { this.port = port; return this; }
    
    public int getUpdateInterval() { return updateInterval; }
    public TglogConfig setUpdateInterval(int updateInterval) { this.updateInterval = updateInterval; return this; }
    
    public int getConnTimeout() { return connTimeout; }
    public TglogConfig setConnTimeout(int connTimeout) { this.connTimeout = connTimeout; return this; }
    
    public int getWriteHighWatermark() { return writeHighWatermark; }
    public TglogConfig setWriteHighWatermark(int writeHighWatermark) { this.writeHighWatermark = writeHighWatermark; return this; }
    
    public int getSocketSendBufferSize() { return socketSendBufferSize; }
    public TglogConfig setSocketSendBufferSize(int socketSendBufferSize) { this.socketSendBufferSize = socketSendBufferSize; return this; }
    
    public int getSocketRecvBufferSize() { return socketRecvBufferSize; }
    public TglogConfig setSocketRecvBufferSize(int socketRecvBufferSize) { this.socketRecvBufferSize = socketRecvBufferSize; return this; }
    
    public int getMaxConnLifetime() { return maxConnLifetime; }
    public TglogConfig setMaxConnLifetime(int maxConnLifetime) { this.maxConnLifetime = maxConnLifetime; return this; }
    
    public int getWorkerNum() { return workerNum; }
    public TglogConfig setWorkerNum(int workerNum) { this.workerNum = workerNum; return this; }
    
    public int getBatchingMaxPublishDelay() { return batchingMaxPublishDelay; }
    public TglogConfig setBatchingMaxPublishDelay(int batchingMaxPublishDelay) { this.batchingMaxPublishDelay = batchingMaxPublishDelay; return this; }
    
    public int getBatchingMaxMessages() { return batchingMaxMessages; }
    public TglogConfig setBatchingMaxMessages(int batchingMaxMessages) { this.batchingMaxMessages = batchingMaxMessages; return this; }
    
    public int getBatchingMaxSize() { return batchingMaxSize; }
    public TglogConfig setBatchingMaxSize(int batchingMaxSize) { this.batchingMaxSize = batchingMaxSize; return this; }
    
    public int getMaxPendingMessages() { return maxPendingMessages; }
    public TglogConfig setMaxPendingMessages(int maxPendingMessages) { this.maxPendingMessages = maxPendingMessages; return this; }
    
    public int getSendTimeout() { return sendTimeout; }
    public TglogConfig setSendTimeout(int sendTimeout) { this.sendTimeout = sendTimeout; return this; }
    
    public int getMaxRetries() { return maxRetries; }
    public TglogConfig setMaxRetries(int maxRetries) { this.maxRetries = maxRetries; return this; }

    public int getStopTimeout() { return stopTimeout; }
    public TglogConfig setStopTimeout(int stopTimeout) { this.stopTimeout = stopTimeout; return this; }

    public TglogCanLogCallback getCanLogCallback() { return canLogCallback; }
    public TglogConfig setCanLogCallback(TglogCanLogCallback canLogCallback) { this.canLogCallback = canLogCallback; return this; }
    
    public TglogWriteLogCallback getWriteLogCallback() { return writeLogCallback; }
    public TglogConfig setWriteLogCallback(TglogWriteLogCallback writeLogCallback) { this.writeLogCallback = writeLogCallback; return this; }
    
    /**
     * Create a UDP configuration with default settings.
     */
    public static TglogConfig createUdp(String host, int port) {
        return new TglogConfig(NETWORK_UDP, host, port);
    }
    
    /**
     * Create a TCP configuration with default settings.
     */
    public static TglogConfig createTcp(String host, int port) {
        return new TglogConfig(NETWORK_TCP, host, port);
    }
    
    /**
     * Set debug logging configuration with predefined log level filtering.
     * 
     * @param minLevel Minimum log level to write (DEBUG, INFO, WARN, ERROR, FATAL)
     * @param writeCallback Callback to handle log writing
     * @return This config object for method chaining
     */
    public TglogConfig setDebugLogging(TglogLogLevel minLevel, TglogWriteLogCallback writeCallback) {
        this.canLogCallback = level -> level.isEnabledFor(minLevel);
        this.writeLogCallback = writeCallback;
        return this;
    }
    
    /**
     * Set debug logging configuration with custom filtering.
     * 
     * @param canLogCallback Custom callback to determine if a log should be written
     * @param writeCallback Callback to handle log writing
     * @return This config object for method chaining
     */
    public TglogConfig setDebugLogging(TglogCanLogCallback canLogCallback, TglogWriteLogCallback writeCallback) {
        this.canLogCallback = canLogCallback;
        this.writeLogCallback = writeCallback;
        return this;
    }
    
    /**
     * Enable debug logging to standard output with INFO level and above.
     * 
     * @return This config object for method chaining
     */
    public TglogConfig enableDebugLogging() {
        return setDebugLogging(TglogLogLevel.INFO, TglogWriteLogCallback.Default.STDOUT);
    }
    
    /**
     * Enable debug logging to standard output with specified minimum level.
     * 
     * @param minLevel Minimum log level to write
     * @return This config object for method chaining
     */
    public TglogConfig enableDebugLogging(TglogLogLevel minLevel) {
        return setDebugLogging(minLevel, TglogWriteLogCallback.Default.STDOUT);
    }
    
    /**
     * Disable debug logging completely.
     * 
     * @return This config object for method chaining
     */
    public TglogConfig disableDebugLogging() {
        this.canLogCallback = level -> false;
        this.writeLogCallback = TglogWriteLogCallback.Default.NONE;
        return this;
    }
    
    @Override
    public String toString() {
        return String.format("TglogConfig{network=%s, host='%s', port=%d, workers=%d}", 
                            network == NETWORK_UDP ? "UDP" : "TCP", host, port, workerNum);
    }
}