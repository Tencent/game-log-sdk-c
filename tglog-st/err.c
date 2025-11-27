/* Standard library headers */

/* System headers */

/* Third-party library headers */

/* Project headers */
#include "err.h"

const char *tglog_get_err_msg(int code) {
    switch (code) {
        case TGLOG_ERR_OK:
            return "OK";
        case TGLOG_ERR_SEND_TIMEOUT:
            return "Send timeout";
        case TGLOG_ERR_SEND_FAILED:
            return "Send failed";
        case TGLOG_ERR_PRODUCER_CLOSED:
            return "Producer closed";
        case TGLOG_ERR_SEND_QUEUE_IS_FULL:
            return "Send queue is full";
        case TGLOG_ERR_NO_AVAILABLE_CONN:
            return "No available connection";
        case TGLOG_ERR_NEW_CONN_FAILED:
            return "New connection failed";
        case TGLOG_ERR_CONN_WRITE_FAILED:
            return "Connection write failed";
        case TGLOG_ERR_CONN_READ_FAILED:
            return "Connection read failed";
        case TGLOG_ERR_LOG_TOO_LONG:
            return "Log too long";
        case TGLOG_ERR_BAD_LOG:
            return "Bad log format";
        case TGLOG_ERR_BATCH_SIZE_EXCEEDED:
            return "Batch size exceeded";
        case TGLOG_ERR_MEMORY_ALLOC_FAILED:
            return "Memory allocation failed";
        case TGLOG_ERR_WORKER_BUSY:
            return "Worker busy";
        case TGLOG_ERR_NO_MATCH_REQ_FOR_RSP:
            return "No matching request for response";
        case TGLOG_ERR_CONN_CLOSED_BY_PEER:
            return "Connection closed by peer";
        case TGLOG_ERR_CONN_EAGAIN:
            return "Connection EAGAIN";
        case TGLOG_ERR_UNKNOWN:
            return "Unknown error";
        default:
            return "Unknown error code";
    }
}