/**
 * Exception thrown by tglog operations.
 * 
 * This exception encapsulates error information from the tglog library,
 * including error codes and descriptive messages.
 */
public class TglogException extends Exception {
    
    private final int errorCode;
    
    /**
     * Create a new TglogException with a message.
     * 
     * @param message Error message
     */
    public TglogException(String message) {
        this(message, TglogJNI.TGLOG_ERR_UNKNOWN);
    }
    
    /**
     * Create a new TglogException with a message and error code.
     * 
     * @param message Error message
     * @param errorCode Tglog error code
     */
    public TglogException(String message, int errorCode) {
        super(message);
        this.errorCode = errorCode;
    }
    
    /**
     * Create a new TglogException with a message and cause.
     * 
     * @param message Error message
     * @param cause Underlying cause
     */
    public TglogException(String message, Throwable cause) {
        this(message, TglogJNI.TGLOG_ERR_UNKNOWN, cause);
    }
    
    /**
     * Create a new TglogException with a message, error code, and cause.
     * 
     * @param message Error message
     * @param errorCode Tglog error code
     * @param cause Underlying cause
     */
    public TglogException(String message, int errorCode, Throwable cause) {
        super(message, cause);
        this.errorCode = errorCode;
    }
    
    /**
     * Get the tglog error code.
     * 
     * @return Error code, or TGLOG_ERR_UNKNOWN if not specified
     */
    public int getErrorCode() {
        return errorCode;
    }
    
    /**
     * Check if this is a timeout error.
     * 
     * @return true if this is a timeout error
     */
    public boolean isTimeout() {
        return errorCode == TglogJNI.TGLOG_ERR_SEND_TIMEOUT;
    }
    
    /**
     * Check if this is a queue full error.
     * 
     * @return true if this is a queue full error
     */
    public boolean isQueueFull() {
        return errorCode == TglogJNI.TGLOG_ERR_SEND_QUEUE_IS_FULL;
    }
    
    /**
     * Check if this is a connection error.
     * 
     * @return true if this is a connection-related error
     */
    public boolean isConnectionError() {
        return errorCode == TglogJNI.TGLOG_ERR_NO_AVAILABLE_CONN ||
               errorCode == TglogJNI.TGLOG_ERR_NEW_CONN_FAILED ||
               errorCode == TglogJNI.TGLOG_ERR_CONN_WRITE_FAILED ||
               errorCode == TglogJNI.TGLOG_ERR_CONN_READ_FAILED ||
               errorCode == TglogJNI.TGLOG_ERR_CONN_CLOSED_BY_PEER;
    }
    
    /**
     * Check if this error is recoverable (retry might succeed).
     * 
     * @return true if the operation might succeed if retried
     */
    public boolean isRecoverable() {
        return errorCode == TglogJNI.TGLOG_ERR_SEND_TIMEOUT ||
               errorCode == TglogJNI.TGLOG_ERR_WORKER_BUSY ||
               errorCode == TglogJNI.TGLOG_ERR_NO_AVAILABLE_CONN ||
               errorCode == TglogJNI.TGLOG_ERR_NEW_CONN_FAILED;
    }
    
    @Override
    public String toString() {
        String errorName = getErrorCodeName(errorCode);
        return String.format("TglogException{code=%d(%s), message='%s'}", 
                           errorCode, errorName, getMessage());
    }
    
    private static String getErrorCodeName(int code) {
        switch (code) {
            case TglogJNI.TGLOG_ERR_OK:
                return "OK";
            case TglogJNI.TGLOG_ERR_SEND_TIMEOUT:
                return "SEND_TIMEOUT";
            case TglogJNI.TGLOG_ERR_SEND_FAILED:
                return "SEND_FAILED";
            case TglogJNI.TGLOG_ERR_PRODUCER_CLOSED:
                return "PRODUCER_CLOSED";
            case TglogJNI.TGLOG_ERR_SEND_QUEUE_IS_FULL:
                return "QUEUE_FULL";
            case TglogJNI.TGLOG_ERR_NO_AVAILABLE_CONN:
                return "NO_AVAILABLE_CONN";
            case TglogJNI.TGLOG_ERR_NEW_CONN_FAILED:
                return "NEW_CONN_FAILED";
            case TglogJNI.TGLOG_ERR_CONN_WRITE_FAILED:
                return "CONN_WRITE_FAILED";
            case TglogJNI.TGLOG_ERR_CONN_READ_FAILED:
                return "CONN_READ_FAILED";
            case TglogJNI.TGLOG_ERR_LOG_TOO_LONG:
                return "LOG_TOO_LONG";
            case TglogJNI.TGLOG_ERR_BAD_LOG:
                return "BAD_LOG";
            case TglogJNI.TGLOG_ERR_BATCH_SIZE_EXCEEDED:
                return "BATCH_SIZE_EXCEEDED";
            case TglogJNI.TGLOG_ERR_MEMORY_ALLOC_FAILED:
                return "MEMORY_ALLOC_FAILED";
            case TglogJNI.TGLOG_ERR_WORKER_BUSY:
                return "WORKER_BUSY";
            case TglogJNI.TGLOG_ERR_NO_MATCH_REQ_FOR_RSP:
                return "NO_MATCH_REQ_FOR_RSP";
            case TglogJNI.TGLOG_ERR_CONN_CLOSED_BY_PEER:
                return "CONN_CLOSED_BY_PEER";
            case TglogJNI.TGLOG_ERR_UNKNOWN:
                return "UNKNOWN";
            default:
                return "UNKNOWN_CODE";
        }
    }
}