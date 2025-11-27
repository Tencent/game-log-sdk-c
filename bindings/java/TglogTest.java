import java.util.concurrent.atomic.AtomicInteger;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class TglogTest {
    
    // Global sent counter - using atomic operations like C version
    private static final AtomicInteger sent = new AtomicInteger(0);
    
    // Token bucket implementation for rate limiting (same as C version)
    static class TokenBucket {
        private long capacity;
        private long tokens;
        private int refillRate;
        private long lastCheck;
        
        public TokenBucket(int capacity, int refillRate) {
            this.capacity = capacity;
            this.tokens = capacity;
            this.refillRate = refillRate;
            this.lastCheck = System.nanoTime();
        }
        
        public synchronized void refillTokens() {
            long now = System.nanoTime();
            long timePassed = now - lastCheck;
            
            if (timePassed > 0) {
                // Use nanosecond precision like C version's microsecond precision
                long newTokens = (timePassed * refillRate) / 1000000000L;
                tokens = Math.min(tokens + newTokens, capacity);
                lastCheck = now;
            }
        }
        
        public synchronized boolean consumeToken(int tokensNeeded) {
            refillTokens();
            if (tokens >= tokensNeeded) {
                tokens -= tokensNeeded;
                return true;
            }
            return false;
        }
    }
    
    // Callback implementation like C version
    static class TestCallback implements TglogCallback {
        @Override
        public void onSuccess(String payload) {
            sent.incrementAndGet();
        }
        
        @Override
        public void onError(String payload, int errorCode, String errorMessage) {
            // Synchronized output to prevent garbled messages in multi-threaded environment
            synchronized(System.out) {
                System.out.println("callback: " + payload.trim() + ", " + errorCode + ", " + errorMessage);
            }
            sent.incrementAndGet();
        }
    }
    
    
    private static void printUsage(String progName) {
        System.out.println("Usage: " + progName + " [options]");
        System.out.println("Options:");
        System.out.println("  -t, --to-send NUM      Number of messages to send (default: 10000000)");
        System.out.println("  -r, --rate NUM         Token bucket refill rate (default: 1000000)");
        System.out.println("  -p, --protocol PROTO   Protocol type: udp or tcp (default: tcp)");
        System.out.println("  -i, --ip IP_ADDRESS    Target IP address (default: dev.tglog.com)");
        System.out.println("  -P, --port PORT        Target port (default: 20001)");
        System.out.println("  -w, --worker NUM       Number of workers (default: 8)");
        System.out.println("  -d, --direct           Use DirectByteBuffer for better performance (no callbacks)");
        System.out.println("  -D, --direct-async     Use DirectByteBuffer with callbacks");
        System.out.println("  -v, --verbose          Enable debug logging (impacts performance)");
        System.out.println("  -h, --help             Show this help message");
    }
    
    public static void main(String[] args) {
        // Set default values
        int toSend = 10000000;
        int rate = 1000000;
        int protocol = TglogJNI.TGLOG_NETWORK_TCP;
        String ip = "dev.tglog.com";
        int port = 20001;
        int worker = 8;
        boolean useDirect = false;
        boolean useDirectAsync = false;
        boolean verbose = false;
        
        // Parse command line arguments
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "-t":
                case "--to-send":
                    if (i + 1 < args.length) {
                        toSend = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-r":
                case "--rate":
                    if (i + 1 < args.length) {
                        rate = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-p":
                case "--protocol":
                    if (i + 1 < args.length) {
                        String proto = args[++i];
                        if ("udp".equals(proto)) {
                            protocol = TglogJNI.TGLOG_NETWORK_UDP;
                        } else if ("tcp".equals(proto)) {
                            protocol = TglogJNI.TGLOG_NETWORK_TCP;
                        } else {
                            System.err.println("Invalid protocol: " + proto + ". Use 'udp' or 'tcp'.");
                            System.exit(1);
                        }
                    }
                    break;
                case "-i":
                case "--ip":
                    if (i + 1 < args.length) {
                        ip = args[++i];
                    }
                    break;
                case "-P":
                case "--port":
                    if (i + 1 < args.length) {
                        port = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-w":
                case "--worker":
                    if (i + 1 < args.length) {
                        worker = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-d":
                case "--direct":
                    useDirect = true;
                    useDirectAsync = false;
                    break;
                case "-D":
                case "--direct-async":
                    useDirect = false;
                    useDirectAsync = true;
                    break;
                case "-v":
                case "--verbose":
                    verbose = true;
                    break;
                case "-h":
                case "--help":
                    printUsage("java TglogTest");
                    System.exit(0);
                    break;
                default:
                    if (args[i].startsWith("-")) {
                        System.err.println("Unknown option: " + args[i]);
                        printUsage("java TglogTest");
                        System.exit(1);
                    }
                    break;
            }
        }
        
        System.out.println("Starting TgLog test with parameters:");
        System.out.println("  Messages to send: " + toSend);
        System.out.println("  Rate: " + rate + " messages/second");
        System.out.println("  Protocol: " + (protocol == TglogJNI.TGLOG_NETWORK_TCP ? "TCP" : "UDP"));
        System.out.println("  Server: " + ip + ":" + port);
        System.out.println("  Workers: " + worker);
        System.out.println("  Use DirectByteBuffer: " + (useDirect ? "yes (no callbacks)" : (useDirectAsync ? "yes (with callbacks)" : "no")));
        System.out.println("  Debug logging: " + verbose);
        
        TokenBucket bucket = new TokenBucket(rate + 1, rate);
        TestCallback callback = new TestCallback();
        sent.set(0); // Reset global counter
        
        // Initialize tglog (safe to call multiple times)
        TglogJNI.useThread();
        
        long conf;
        if (useDirect) {
            // For DirectByteBuffer, use simplified configuration without debug callbacks
            // This matches the old Fast version for maximum performance
            conf = TglogJNI.createConf(protocol, ip, port, null, null);
        } else {
            // Use configuration without debug logging for fair performance comparison
            TglogConfig config = new TglogConfig(protocol, ip, port)
                    .setWorkerNum(worker)                           // Use command line worker parameter
                    .setMaxConnLifetime(300);                       // Match C version: 300 seconds
            
            // Only enable debug logging if explicitly requested with -v flag
            if (verbose) {
                config.enableDebugLogging(TglogLogLevel.INFO);
            }
            
            conf = TglogJNI.createConfFromConfig(config);
        }
        if (conf == 0) {
            System.err.println("Failed to create configuration");
            System.exit(1);
        }
        
        // Create client
        long client = TglogJNI.createClient(conf);
        // Configuration has been copied to client, destroy it safely
        TglogJNI.destroyConf(conf);
        
        if (client == 0) {
            System.err.println("Failed to create client");
            System.exit(1);
        }
        
        long startTime = System.currentTimeMillis();
        
        String log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n";
        
        // Prepare DirectByteBuffer if needed
        ByteBuffer directBuffer = null;
        byte[] logBytes = null;
        if (useDirect || useDirectAsync) {
            logBytes = log.getBytes(StandardCharsets.UTF_8);
            // Allocate extra byte for null terminator to enable zero-copy
            directBuffer = ByteBuffer.allocateDirect(logBytes.length + 1);
            // Pre-fill the buffer once since we're sending the same message
            directBuffer.put(logBytes);
            directBuffer.put((byte)0); // Add null terminator for zero-copy
        }
        
        int i = 0;
        while (true) {
            // Rate limiting using token bucket (skip if rate is 0 or negative)
            if (rate > 0) {
                if (!bucket.consumeToken(1)) {
                    try {
                        // Sleep for 500 microseconds like C version (usleep(500))
                        // Java sleep(0, 500000) = 500,000 nanoseconds = 500 microseconds
                        Thread.sleep(0, 500000);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                    continue;
                }
            }
            
            int result;
            if (useDirect) {
                // Use DirectByteBuffer for better performance (no callbacks)
                // Buffer is already filled, just reset position for reuse
                directBuffer.position(0);
                result = TglogJNI.sendDirectBuffer(client, directBuffer, 0, logBytes.length, (byte)0x02);
                // Simulate callback for counting
                if (result == TglogJNI.TGLOG_ERR_OK) {
                    sent.incrementAndGet();
                }
            } else if (useDirectAsync) {
                // Use DirectByteBuffer with callbacks for production use
                // Buffer is already filled, just reset position for reuse
                directBuffer.position(0);
                result = TglogJNI.sendDirectBufferAsync(client, directBuffer, 0, logBytes.length, callback, (byte)0x02);
                // No need to increment counter here - callback will do it
            } else {
                // Use traditional string-based sending with callback
                result = TglogJNI.sendAsync(client, log, callback, (byte)0x02);
                // No need to increment counter here - callback will do it
            }
            
            if (result != TglogJNI.TGLOG_ERR_OK) {
                System.err.println("Send failed with error: " + result + " - " + TglogJNI.getErrorMessage(result));
            }
            
            if (++i == toSend) {
                break;
            }
        }
        
        long endTime = System.currentTimeMillis();
        double elapsed = (endTime - startTime);
        System.out.println("Send time elapsed: " + elapsed + " milliseconds");
        
        // Wait for all messages to be sent (like C version)
        while (sent.get() < toSend) {
            try {
                Thread.sleep(1); // 1ms like C version usleep(1000)
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                break;
            }
        }
        
        System.out.println("sent " + sent.get() + " logs");
        endTime = System.currentTimeMillis();
        elapsed = (endTime - startTime);
        System.out.println("done time elapsed: " + elapsed + " milliseconds");
        
        // Get and print metrics
        try {
            Thread.sleep(1000);
            String metrics = TglogJNI.getMetrics(client);
            if (metrics != null) {
                System.out.println("Metrics: " + metrics);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
        
        // Cleanup - call destroy like C version
        System.out.println("Test completed successfully");
        
        // Destroy client
        TglogJNI.destroyClient(client);
        System.out.println("Client destroyed successfully");
    }
    
    // Wrapper main to handle exceptions properly
    public static void realMain(String[] args) {
        long client = 0;
        try {
            main(args);
        } catch (Exception e) {
            System.err.println("Fatal error: " + e.getMessage());
            e.printStackTrace();
            // Try to cleanup if client was created
            if (client != 0) {
                try {
                    TglogJNI.destroyClient(client);
                } catch (Exception ignored) {
                    // Best effort cleanup
                }
            }
            System.exit(1);
        }
    }
}