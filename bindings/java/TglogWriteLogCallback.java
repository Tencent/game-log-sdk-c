/**
 * Callback interface for writing debug log messages.
 * 
 * This interface corresponds to the write_log_func in the C library.
 * Implementations handle the actual writing/output of debug log messages.
 * 
 * IMPORTANT: Implementations MUST be thread-safe as this callback will be
 * called concurrently from multiple tglog worker threads.
 */
@FunctionalInterface
public interface TglogWriteLogCallback {
    
    /**
     * Write a debug log message.
     * 
     * @param level The log level of the message
     * @param file The source file name where the log was generated
     * @param line The line number in the source file
     * @param function The function name where the log was generated
     * @param message The formatted log message (already processed from format string)
     */
    void writeLog(TglogLogLevel level, String file, int line, String function, String message);
    
    /**
     * Convenience class providing common log writing implementations.
     */
    class Default {
        
        /**
         * Standard output logger - writes to System.out with timestamp.
         */
        public static final TglogWriteLogCallback STDOUT = (level, file, line, function, message) -> {
            String timestamp = java.time.LocalDateTime.now().toString();
            System.out.printf("[%s] %s %s:%d %s() - %s%n", 
                            timestamp, level, file, line, function, message);
        };
        
        /**
         * Standard error logger - writes to System.err with timestamp.
         */
        public static final TglogWriteLogCallback STDERR = (level, file, line, function, message) -> {
            String timestamp = java.time.LocalDateTime.now().toString();
            System.err.printf("[%s] %s %s:%d %s() - %s%n", 
                            timestamp, level, file, line, function, message);
        };
        
        /**
         * Simple logger - writes to System.out without timestamp.
         */
        public static final TglogWriteLogCallback SIMPLE = (level, file, line, function, message) -> {
            System.out.printf("%s %s:%d %s() - %s%n", 
                            level, file, line, function, message);
        };
        
        /**
         * No-op logger - discards all log messages.
         */
        public static final TglogWriteLogCallback NONE = (level, file, line, function, message) -> {
            // Do nothing
        };
    }
}