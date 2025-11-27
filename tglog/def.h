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

/* The TGLog client will not copy and free the message */
#define TGLOG_MSG_F_NONE                    0x00
/* The TGLog client will free the message after sending and invoking the callback using free() */
#define TGLOG_MSG_F_FREE                    0x01
/* The TGLog client will copy the message and free it after sending and invoking the callback */
#define TGLOG_MSG_F_COPY                    0x02

#ifdef __cplusplus
}
#endif

#endif /* TGLOG_DEF_H */