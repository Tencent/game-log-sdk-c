/**
 * Enumeration for tglog debug log levels.
 * 
 * This enum corresponds to the tglog_level enum in the C library.
 */
public enum TglogLogLevel {
    /**
     * Debug level - most verbose logging level
     */
    DEBUG(0),
    
    /**
     * Info level - general information messages
     */
    INFO(1),
    
    /**
     * Warning level - warning messages
     */
    WARN(2),
    
    /**
     * Error level - error messages
     */
    ERROR(3),
    
    /**
     * Fatal level - critical error messages
     */
    FATAL(4);
    
    private final int value;
    
    TglogLogLevel(int value) {
        this.value = value;
    }
    
    /**
     * Get the numeric value of this log level.
     * 
     * @return The numeric value corresponding to the C enum value
     */
    public int getValue() {
        return value;
    }
    
    /**
     * Create a TglogLogLevel from its numeric value.
     * 
     * @param value The numeric value
     * @return The corresponding TglogLogLevel
     * @throws IllegalArgumentException if the value is invalid
     */
    public static TglogLogLevel fromValue(int value) {
        for (TglogLogLevel level : values()) {
            if (level.value == value) {
                return level;
            }
        }
        throw new IllegalArgumentException("Invalid log level value: " + value);
    }
    
    /**
     * Check if this level is enabled for the given minimum level.
     * 
     * @param minLevel The minimum log level
     * @return true if this level should be logged
     */
    public boolean isEnabledFor(TglogLogLevel minLevel) {
        return this.value >= minLevel.value;
    }
    
    @Override
    public String toString() {
        return name();
    }
}