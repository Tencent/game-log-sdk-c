/**
 * Statistics class for tglog client.
 * 
 * This class provides detailed statistics about the tglog client's operation,
 * including message counts, connection statistics, queue information, and
 * per-worker statistics that mirror the C library's nested metrics structure.
 * 
 * The statistics are organized in a hierarchical structure:
 * - Top-level stats: Aggregated metrics across all workers
 * - Worker-level stats: Individual metrics for each worker thread
 * 
 * This mirrors the C library's tglog_metrics structure which contains
 * a workers array (struct tglog_metrics **workers).
 */
public class TglogStats {
    
    // Message statistics
    private long requestMsgCount;
    private long enqueuedMsgCount;
    private long dequeuedMsgCount;
    private long succeededMsgCount;
    private long failedMsgCount;
    private long timeoutMsgCount;
    private long retryMsgCount;
    private long droppedMsgCount;
    
    // Connection statistics
    private long openedConnCount;
    private long closedConnCount;
    private long errorConnCount;
    private long eagainErrCount;
    private long rescuedMsgBytes;
    
    // Endpoint statistics
    private long endpointCount;
    private long addEndpointCount;
    private long removeEndpointCount;
    
    // Queue statistics
    private long msgQueueSize;
    private long msgQueueCapacity;
    
    // Worker count and worker-level statistics
    private int workerCount;
    private TglogStats[] workerStats;  // Worker-level statistics array
    
    // Constructor for JNI - with worker stats
    public TglogStats(long requestMsgCount, long enqueuedMsgCount, long dequeuedMsgCount,
                     long succeededMsgCount, long failedMsgCount, long timeoutMsgCount,
                     long retryMsgCount, long droppedMsgCount, long openedConnCount,
                     long closedConnCount, long errorConnCount, long eagainErrCount,
                     long rescuedMsgBytes, long endpointCount, long addEndpointCount,
                     long removeEndpointCount, long msgQueueSize, long msgQueueCapacity,
                     int workerCount, TglogStats[] workerStats) {
        this.requestMsgCount = requestMsgCount;
        this.enqueuedMsgCount = enqueuedMsgCount;
        this.dequeuedMsgCount = dequeuedMsgCount;
        this.succeededMsgCount = succeededMsgCount;
        this.failedMsgCount = failedMsgCount;
        this.timeoutMsgCount = timeoutMsgCount;
        this.retryMsgCount = retryMsgCount;
        this.droppedMsgCount = droppedMsgCount;
        this.openedConnCount = openedConnCount;
        this.closedConnCount = closedConnCount;
        this.errorConnCount = errorConnCount;
        this.eagainErrCount = eagainErrCount;
        this.rescuedMsgBytes = rescuedMsgBytes;
        this.endpointCount = endpointCount;
        this.addEndpointCount = addEndpointCount;
        this.removeEndpointCount = removeEndpointCount;
        this.msgQueueSize = msgQueueSize;
        this.msgQueueCapacity = msgQueueCapacity;
        this.workerCount = workerCount;
        this.workerStats = workerStats;
    }
    
    // Constructor for worker-level stats (without nested workers)
    public TglogStats(long requestMsgCount, long enqueuedMsgCount, long dequeuedMsgCount,
                     long succeededMsgCount, long failedMsgCount, long timeoutMsgCount,
                     long retryMsgCount, long droppedMsgCount, long openedConnCount,
                     long closedConnCount, long errorConnCount, long eagainErrCount,
                     long rescuedMsgBytes, long endpointCount, long addEndpointCount,
                     long removeEndpointCount, long msgQueueSize, long msgQueueCapacity,
                     int workerCount) {
        this(requestMsgCount, enqueuedMsgCount, dequeuedMsgCount, succeededMsgCount,
             failedMsgCount, timeoutMsgCount, retryMsgCount, droppedMsgCount,
             openedConnCount, closedConnCount, errorConnCount, eagainErrCount,
             rescuedMsgBytes, endpointCount, addEndpointCount, removeEndpointCount,
             msgQueueSize, msgQueueCapacity, workerCount, null);
    }
    
    // Getters
    public long getRequestMsgCount() { return requestMsgCount; }
    public long getEnqueuedMsgCount() { return enqueuedMsgCount; }
    public long getDequeuedMsgCount() { return dequeuedMsgCount; }
    public long getSucceededMsgCount() { return succeededMsgCount; }
    public long getFailedMsgCount() { return failedMsgCount; }
    public long getTimeoutMsgCount() { return timeoutMsgCount; }
    public long getRetryMsgCount() { return retryMsgCount; }
    public long getDroppedMsgCount() { return droppedMsgCount; }
    public long getOpenedConnCount() { return openedConnCount; }
    public long getClosedConnCount() { return closedConnCount; }
    public long getErrorConnCount() { return errorConnCount; }
    public long getEagainErrCount() { return eagainErrCount; }
    public long getRescuedMsgBytes() { return rescuedMsgBytes; }
    public long getEndpointCount() { return endpointCount; }
    public long getAddEndpointCount() { return addEndpointCount; }
    public long getRemoveEndpointCount() { return removeEndpointCount; }
    public long getMsgQueueSize() { return msgQueueSize; }
    public long getMsgQueueCapacity() { return msgQueueCapacity; }
    public int getWorkerCount() { return workerCount; }
    public TglogStats[] getWorkerStats() { return workerStats; }
    
    /**
     * Get statistics for a specific worker.
     * 
     * @param workerId Worker ID (0-based index)
     * @return Worker statistics, or null if worker ID is invalid
     */
    public TglogStats getWorkerStats(int workerId) {
        if (workerStats != null && workerId >= 0 && workerId < workerStats.length) {
            return workerStats[workerId];
        }
        return null;
    }
    
    /**
     * Check if this stats object contains worker-level statistics.
     * 
     * @return true if worker statistics are available
     */
    public boolean hasWorkerStats() {
        return workerStats != null && workerStats.length > 0;
    }
    
    // Calculated metrics
    public double getSuccessRate() {
        long total = succeededMsgCount + failedMsgCount;
        return total > 0 ? (double) succeededMsgCount / total : 0.0;
    }
    
    public double getQueueUtilization() {
        return msgQueueCapacity > 0 ? (double) msgQueueSize / msgQueueCapacity : 0.0;
    }
    
    public long getActiveConnCount() {
        return openedConnCount - closedConnCount;
    }
    
    /**
     * Get a summary of worker statistics.
     * 
     * @return Array of worker summary strings
     */
    public String[] getWorkerSummaries() {
        if (!hasWorkerStats()) {
            return new String[0];
        }
        
        String[] summaries = new String[workerStats.length];
        for (int i = 0; i < workerStats.length; i++) {
            TglogStats worker = workerStats[i];
            summaries[i] = String.format(
                "Worker[%d]: requests=%d, succeeded=%d, failed=%d, queued=%d/%d, successRate=%.2f%%",
                i, worker.requestMsgCount, worker.succeededMsgCount, worker.failedMsgCount,
                worker.msgQueueSize, worker.msgQueueCapacity, worker.getSuccessRate() * 100
            );
        }
        return summaries;
    }
    
    /**
     * Get aggregate statistics across all workers.
     * 
     * @return Summary of total vs worker aggregate comparison
     */
    public String getAggregateComparison() {
        if (!hasWorkerStats()) {
            return "No worker statistics available";
        }
        
        long workerTotalRequests = 0;
        long workerTotalSucceeded = 0;
        long workerTotalFailed = 0;
        
        for (TglogStats worker : workerStats) {
            workerTotalRequests += worker.requestMsgCount;
            workerTotalSucceeded += worker.succeededMsgCount;
            workerTotalFailed += worker.failedMsgCount;
        }
        
        return String.format(
            "Total: req=%d, succ=%d, fail=%d | Workers sum: req=%d, succ=%d, fail=%d",
            requestMsgCount, succeededMsgCount, failedMsgCount,
            workerTotalRequests, workerTotalSucceeded, workerTotalFailed
        );
    }
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append(String.format(
            "TglogStats{requests=%d, succeeded=%d, failed=%d, queued=%d/%d, " +
            "activeConns=%d, endpoints=%d, workers=%d, successRate=%.2f%%}",
            requestMsgCount, succeededMsgCount, failedMsgCount, 
            msgQueueSize, msgQueueCapacity, getActiveConnCount(), 
            endpointCount, workerCount, getSuccessRate() * 100
        ));
        
        if (hasWorkerStats()) {
            sb.append("\nWorker breakdown:");
            String[] summaries = getWorkerSummaries();
            for (String summary : summaries) {
                sb.append("\n  ").append(summary);
            }
            sb.append("\n").append(getAggregateComparison());
        }
        
        return sb.toString();
    }
}