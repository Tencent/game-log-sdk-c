/**
 * Callback interface for asynchronous tglog operations.
 * 
 * This interface is used to handle the results of asynchronous log sending operations.
 * The callback methods will be invoked from background threads, so implementations
 * should be thread-safe.
 */
public interface TglogCallback {
    
    /**
     * Called when a log message is successfully sent.
     * 
     * @param payload The log payload that was sent
     */
    void onSuccess(String payload);
    
    /**
     * Called when a log message fails to send.
     * 
     * @param payload The log payload that failed to send
     * @param errorCode The error code indicating the failure reason
     * @param errorMessage A human-readable error message
     */
    void onError(String payload, int errorCode, String errorMessage);
}