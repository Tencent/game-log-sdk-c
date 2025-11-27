import java.nio.ByteBuffer;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Performance test program for TglogClient demonstrating all three performance modes.
 * 
 * Performance expectations:
 * - Standard mode with callbacks: ~3.5s for 1M messages
 * - Performance mode without debug: ~3s for 1M messages
 * - DirectByteBuffer mode: ~1s for 1M messages
 */
public class TglogClientTest {
    
    // Test configuration
    private static final String DEFAULT_HOST = "dev.tglog.com";
    private static final int DEFAULT_PORT = 20001;
    private static final int DEFAULT_MESSAGES = 1000000;
    private static final int DEFAULT_WORKERS = 8;
    
    // Sample log message (same as TglogTest.java)
    private static final String LOG_MESSAGE = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n";
    
    // Command line options
    private static class Options {
        String host = DEFAULT_HOST;
        int port = DEFAULT_PORT;
        int messages = DEFAULT_MESSAGES;
        int workers = DEFAULT_WORKERS;
        int protocol = TglogConfig.NETWORK_TCP;
        TestMode mode = TestMode.ALL;
        boolean verbose = false;
    }
    
    private enum TestMode {
        ALL,
        STANDARD,
        PERFORMANCE,
        DIRECT,
        DIRECT_ASYNC  // New mode for DirectByteBuffer with callbacks
    }
    
    public static void main(String[] args) {
        Options options = parseArgs(args);
        
        System.out.println("TglogClient Performance Test");
        System.out.println("============================");
        System.out.println("Server: " + options.host + ":" + options.port);
        System.out.println("Protocol: " + (options.protocol == TglogConfig.NETWORK_TCP ? "TCP" : "UDP"));
        System.out.println("Messages: " + options.messages);
        System.out.println("Workers: " + options.workers);
        System.out.println("Test mode: " + options.mode);
        System.out.println();
        
        try {
            switch (options.mode) {
                case STANDARD:
                    testStandardMode(options);
                    break;
                case PERFORMANCE:
                    testPerformanceMode(options);
                    break;
                case DIRECT:
                    testDirectMode(options);
                    break;
                case DIRECT_ASYNC:
                    testDirectAsyncMode(options);
                    break;
                case ALL:
                    testStandardMode(options);
                    System.out.println();
                    testPerformanceMode(options);
                    System.out.println();
                    testDirectMode(options);
                    System.out.println();
                    testDirectAsyncMode(options);
                    break;
            }
        } catch (Exception e) {
            System.err.println("Test failed: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
    
    /**
     * Test standard mode with callbacks for delivery confirmation.
     */
    private static void testStandardMode(Options options) throws Exception {
        System.out.println("Testing STANDARD mode (with callbacks)");
        System.out.println("---------------------------------------");
        
        TglogConfig config = new TglogConfig(options.protocol, options.host, options.port)
            .setWorkerNum(options.workers)
            .setMaxConnLifetime(300);
        
        if (options.verbose) {
            config.enableDebugLogging(TglogLogLevel.INFO);
        }
        
        try (TglogClient client = new TglogClient(config)) {
            AtomicInteger completed = new AtomicInteger(0);
            AtomicInteger errors = new AtomicInteger(0);
            CountDownLatch latch = new CountDownLatch(options.messages);
            
            TglogCallback callback = new TglogCallback() {
                @Override
                public void onSuccess(String payload) {
                    completed.incrementAndGet();
                    latch.countDown();
                }
                
                @Override
                public void onError(String payload, int errorCode, String errorMessage) {
                    errors.incrementAndGet();
                    latch.countDown();
                    if (errors.get() <= 10) { // Only print first 10 errors
                        System.err.println("Error: " + errorMessage);
                    }
                }
            };
            
            long startTime = System.currentTimeMillis();
            
            // Send messages
            for (int i = 0; i < options.messages; i++) {
                client.sendAsync(LOG_MESSAGE, callback);
                
                if ((i + 1) % 100000 == 0) {
                    System.out.println("  Sent " + (i + 1) + " messages...");
                }
            }
            
            long sendTime = System.currentTimeMillis() - startTime;
            System.out.println("  Send time: " + sendTime + " ms");
            
            // Wait for all callbacks
            boolean finished = latch.await(60, TimeUnit.SECONDS);
            long totalTime = System.currentTimeMillis() - startTime;
            
            if (!finished) {
                System.out.println("  Warning: Timeout waiting for callbacks");
            }
            
            System.out.println("  Total time: " + totalTime + " ms");
            System.out.println("  Completed: " + completed.get());
            System.out.println("  Errors: " + errors.get());
            System.out.println("  Throughput: " + (options.messages * 1000L / totalTime) + " msg/s");
            
            // Print metrics
            if (options.verbose) {
                System.out.println("  Metrics: " + client.getMetrics());
            }
        }
    }
    
    /**
     * Test performance mode without debug callbacks.
     */
    private static void testPerformanceMode(Options options) throws Exception {
        System.out.println("Testing PERFORMANCE mode (no debug callbacks)");
        System.out.println("---------------------------------------------");
        
        TglogConfig config = new TglogConfig(options.protocol, options.host, options.port)
            .setWorkerNum(options.workers)
            .setMaxConnLifetime(300);
        
        // Use performance mode constructor (second parameter = true)
        try (TglogClient client = new TglogClient(config, true)) {
            AtomicInteger completed = new AtomicInteger(0);
            AtomicInteger errors = new AtomicInteger(0);
            CountDownLatch latch = new CountDownLatch(options.messages);
            
            // Callback that counts down for each message
            TglogCallback callback = new TglogCallback() {
                @Override
                public void onSuccess(String payload) {
                    completed.incrementAndGet();
                    latch.countDown();
                }
                
                @Override
                public void onError(String payload, int errorCode, String errorMessage) {
                    errors.incrementAndGet();
                    latch.countDown();
                }
            };
            
            long startTime = System.currentTimeMillis();
            
            // Send messages
            for (int i = 0; i < options.messages; i++) {
                client.sendAsync(LOG_MESSAGE, callback);
                
                if ((i + 1) % 100000 == 0) {
                    System.out.println("  Sent " + (i + 1) + " messages...");
                }
            }
            
            long sendTime = System.currentTimeMillis() - startTime;
            System.out.println("  Send time: " + sendTime + " ms");
            
            // Wait for all callbacks
            boolean finished = latch.await(60, TimeUnit.SECONDS);
            long totalTime = System.currentTimeMillis() - startTime;
            
            if (!finished) {
                System.out.println("  Warning: Timeout waiting for callbacks");
                System.out.println("  Received: " + (completed.get() + errors.get()) + " / " + options.messages);
            }
            
            System.out.println("  Total time: " + totalTime + " ms");
            System.out.println("  Completed: " + completed.get());
            System.out.println("  Errors: " + errors.get());
            System.out.println("  Throughput: " + (options.messages * 1000L / totalTime) + " msg/s");
            
            // Print metrics
            if (options.verbose) {
                System.out.println("  Metrics: " + client.getMetrics());
            }
        }
    }
    
    /**
     * Test DirectByteBuffer mode for maximum performance.
     */
    private static void testDirectMode(Options options) throws Exception {
        System.out.println("Testing DIRECT mode (DirectByteBuffer, no callbacks)");
        System.out.println("----------------------------------------------------");
        
        // Use high-performance constructor
        try (TglogClient client = new TglogClient(options.protocol, options.host, options.port)) {
            
            // Create a reusable DirectByteBuffer
            ByteBuffer buffer = TglogClient.createDirectBuffer(LOG_MESSAGE);
            int messageLength = LOG_MESSAGE.getBytes("UTF-8").length;
            
            long startTime = System.currentTimeMillis();
            
            // Send messages using DirectByteBuffer
            for (int i = 0; i < options.messages; i++) {
                // Reset buffer position for reuse
                buffer.position(0);
                client.sendDirect(buffer, messageLength);
                
                if ((i + 1) % 100000 == 0) {
                    System.out.println("  Sent " + (i + 1) + " messages...");
                }
            }
            
            long totalTime = System.currentTimeMillis() - startTime;
            
            System.out.println("  Total time: " + totalTime + " ms");
            System.out.println("  Sent: " + options.messages);
            System.out.println("  Throughput: " + (options.messages * 1000L / totalTime) + " msg/s");
            
            // Wait a bit for messages to be processed
            Thread.sleep(1000);
            
            // Print final stats
            TglogStats stats = client.getStats();
            if (stats != null) {
                System.out.println("  Stats - Enqueued: " + stats.getEnqueuedMsgCount() + 
                                 ", Succeeded: " + stats.getSucceededMsgCount() +
                                 ", Failed: " + stats.getFailedMsgCount());
            }
            
            // Print metrics
            if (options.verbose) {
                System.out.println("  Metrics: " + client.getMetrics());
            }
        }
    }
    
    /**
     * Test DirectByteBuffer with callback mode for production use.
     */
    private static void testDirectAsyncMode(Options options) throws Exception {
        System.out.println("Testing DIRECT_ASYNC mode (DirectByteBuffer with callbacks)");
        System.out.println("-----------------------------------------------------------");
        
        // Use high-performance constructor
        try (TglogClient client = new TglogClient(options.protocol, options.host, options.port)) {
            AtomicInteger completed = new AtomicInteger(0);
            AtomicInteger errors = new AtomicInteger(0);
            CountDownLatch latch = new CountDownLatch(options.messages);
            
            // Callback for tracking
            TglogCallback callback = new TglogCallback() {
                @Override
                public void onSuccess(String payload) {
                    completed.incrementAndGet();
                    latch.countDown();
                }
                
                @Override
                public void onError(String payload, int errorCode, String errorMessage) {
                    errors.incrementAndGet();
                    latch.countDown();
                    if (errors.get() <= 10) { // Only print first 10 errors
                        System.err.println("Error: " + errorMessage);
                    }
                }
            };
            
            // Create a reusable DirectByteBuffer
            ByteBuffer buffer = TglogClient.createDirectBuffer(LOG_MESSAGE);
            int messageLength = LOG_MESSAGE.getBytes("UTF-8").length;
            
            long startTime = System.currentTimeMillis();
            
            // Send messages using DirectByteBuffer with callbacks
            for (int i = 0; i < options.messages; i++) {
                // Reset buffer position for reuse
                buffer.position(0);
                client.sendDirectAsync(buffer, messageLength, callback);
                
                if ((i + 1) % 100000 == 0) {
                    System.out.println("  Sent " + (i + 1) + " messages...");
                }
            }
            
            long sendTime = System.currentTimeMillis() - startTime;
            System.out.println("  Send time: " + sendTime + " ms");
            
            // Wait for all callbacks
            boolean finished = latch.await(60, TimeUnit.SECONDS);
            long totalTime = System.currentTimeMillis() - startTime;
            
            if (!finished) {
                System.out.println("  Warning: Timeout waiting for callbacks");
                System.out.println("  Received: " + (completed.get() + errors.get()) + " / " + options.messages);
            }
            
            System.out.println("  Total time: " + totalTime + " ms");
            System.out.println("  Completed: " + completed.get());
            System.out.println("  Errors: " + errors.get());
            System.out.println("  Throughput: " + (options.messages * 1000L / totalTime) + " msg/s");
            
            // Print metrics
            if (options.verbose) {
                System.out.println("  Metrics: " + client.getMetrics());
            }
        }
    }
    
    /**
     * Parse command line arguments.
     */
    private static Options parseArgs(String[] args) {
        Options options = new Options();
        
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "-h":
                case "--host":
                    if (i + 1 < args.length) {
                        options.host = args[++i];
                    }
                    break;
                case "-p":
                case "--port":
                    if (i + 1 < args.length) {
                        options.port = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-m":
                case "--messages":
                    if (i + 1 < args.length) {
                        options.messages = Integer.parseInt(args[++i]);
                    }
                    break;
                case "-w":
                case "--workers":
                    if (i + 1 < args.length) {
                        options.workers = Integer.parseInt(args[++i]);
                    }
                    break;
                case "--tcp":
                    options.protocol = TglogConfig.NETWORK_TCP;
                    break;
                case "--udp":
                    options.protocol = TglogConfig.NETWORK_UDP;
                    break;
                case "--standard":
                    options.mode = TestMode.STANDARD;
                    break;
                case "--performance":
                    options.mode = TestMode.PERFORMANCE;
                    break;
                case "--direct":
                    options.mode = TestMode.DIRECT;
                    break;
                case "--direct-async":
                    options.mode = TestMode.DIRECT_ASYNC;
                    break;
                case "-v":
                case "--verbose":
                    options.verbose = true;
                    break;
                case "--help":
                    printUsage();
                    System.exit(0);
                    break;
                default:
                    if (args[i].startsWith("-")) {
                        System.err.println("Unknown option: " + args[i]);
                        printUsage();
                        System.exit(1);
                    }
            }
        }
        
        return options;
    }
    
    /**
     * Print usage information.
     */
    private static void printUsage() {
        System.out.println("Usage: java TglogClientTest [options]");
        System.out.println();
        System.out.println("Options:");
        System.out.println("  -h, --host HOST      Server host (default: " + DEFAULT_HOST + ")");
        System.out.println("  -p, --port PORT      Server port (default: " + DEFAULT_PORT + ")");
        System.out.println("  -m, --messages NUM   Number of messages to send (default: " + DEFAULT_MESSAGES + ")");
        System.out.println("  -w, --workers NUM    Number of worker threads (default: " + DEFAULT_WORKERS + ")");
        System.out.println("  --tcp                Use TCP protocol (default)");
        System.out.println("  --udp                Use UDP protocol");
        System.out.println("  --standard           Test only standard mode with callbacks");
        System.out.println("  --performance        Test only performance mode");
        System.out.println("  --direct             Test only DirectByteBuffer mode (no callbacks)");
        System.out.println("  --direct-async       Test only DirectByteBuffer mode with callbacks");
        System.out.println("  -v, --verbose        Enable verbose output with metrics");
        System.out.println("  --help               Show this help message");
        System.out.println();
        System.out.println("Examples:");
        System.out.println("  java TglogClientTest                    # Test all modes");
        System.out.println("  java TglogClientTest --direct           # Test only DirectByteBuffer mode");
        System.out.println("  java TglogClientTest -m 100000 -v       # Test with 100K messages and metrics");
    }
}