#ifndef TGLOG_DEF_H
#define TGLOG_DEF_H

/* Standard library headers */

/* System headers */

/* Third-party library headers */

/* Project headers */

#ifdef __cplusplus
extern "C" {
#endif

#define TGLOG_DELIMITER                     '\n'

#define TGLOG_MAX_UDP_REQ_SIZE_V1           65507
#define TGLOG_MAX_TCP_REQ_SIZE_V1           (512 * 1024)
#define TGLOG_MAX_UDP_REQ_SIZE_V3           64512
#define TGLOG_MAX_TCP_REQ_SIZE_V3           (512 * 1024)

/* Message flags */
typedef enum {
    TGLOG_MSG_F_NONE = 0,
    TGLOG_MSG_F_FREE = 1,
    TGLOG_MSG_F_COPY = 2,
} tglog_msg_flag;
#ifdef __cplusplus
}
#endif

#endif /* TGLOG_DEF_H */