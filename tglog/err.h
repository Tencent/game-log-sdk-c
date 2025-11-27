#ifndef TGLOG_ERR_H
#define TGLOG_ERR_H

/* Standard library headers */

/* System headers */

/* Third-party library headers */

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

#define TGLOG_ERR_OK                        0
#define TGLOG_ERR_SEND_TIMEOUT              10001
#define TGLOG_ERR_SEND_FAILED               10002
#define TGLOG_ERR_PRODUCER_CLOSED           10003
#define TGLOG_ERR_SEND_QUEUE_IS_FULL        10004
#define TGLOG_ERR_NO_AVAILABLE_CONN         10005
#define TGLOG_ERR_NEW_CONN_FAILED           10006
#define TGLOG_ERR_CONN_WRITE_FAILED         10007
#define TGLOG_ERR_CONN_READ_FAILED          10008
#define TGLOG_ERR_LOG_TOO_LONG              10009
#define TGLOG_ERR_BAD_LOG                   10010
#define TGLOG_ERR_BATCH_SIZE_EXCEEDED       10011
#define TGLOG_ERR_MEMORY_ALLOC_FAILED       10012
#define TGLOG_ERR_WORKER_BUSY               10013
#define TGLOG_ERR_NO_MATCH_REQ_FOR_RSP      10014
#define TGLOG_ERR_CONN_CLOSED_BY_PEER       10015
#define TGLOG_ERR_CONN_EAGAIN               10016
#define TGLOG_ERR_UNKNOWN                   20001

const char *tglog_get_err_msg(int code);

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_ERR_H */