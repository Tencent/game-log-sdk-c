/**
 * Callback interface for filtering debug log messages.
 * 
 * This interface corresponds to the can_log_func in the C library.
 * Implementations determine whether a log message of a given level should be written.
 * 
 * IMPORTANT: Implementations MUST be thread-safe as this callback will be
 * called concurrently from multiple tglog worker threads.
 */
@FunctionalInterface
public interface TglogCanLogCallback {
    
    /**
     * Determine whether a log message of the given level should be written.
     * 
     * @param level The log level of the message
     * @return true if the message should be written, false to filter it out
     */
    boolean canLog(TglogLogLevel level);
    
    /**
     * Predefined implementations for common log level filtering.
     */
    class Level {
        
        /**
         * Allow DEBUG level and above (all messages).
         */
        public static final TglogCanLogCallback DEBUG = level -> level.isEnabledFor(TglogLogLevel.DEBUG);
        
        /**
         * Allow INFO level and above.
         */
        public static final TglogCanLogCallback INFO = level -> level.isEnabledFor(TglogLogLevel.INFO);
        
        /**
         * Allow WARN level and above.
         */
        public static final TglogCanLogCallback WARN = level -> level.isEnabledFor(TglogLogLevel.WARN);
        
        /**
         * Allow ERROR level and above.
         */
        public static final TglogCanLogCallback ERROR = level -> level.isEnabledFor(TglogLogLevel.ERROR);
        
        /**
         * Allow only FATAL level.
         */
        public static final TglogCanLogCallback FATAL = level -> level.isEnabledFor(TglogLogLevel.FATAL);
        
        /**
         * Disable all logging.
         */
        public static final TglogCanLogCallback NONE = level -> false;
    }
}