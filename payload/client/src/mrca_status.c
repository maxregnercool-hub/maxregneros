#include "mrca.h"

const char *mrca_status_str(int st) {
    switch (st) {
        case MRCA_OK:              return "ok";
        case MRCA_ERR_GENERIC:     return "generic error";
        case MRCA_ERR_NOMEM:       return "out of memory";
        case MRCA_ERR_IO:          return "io error";
        case MRCA_ERR_PROTO:       return "protocol error";
        case MRCA_ERR_AUTH:        return "auth rejected";
        case MRCA_ERR_TIMEOUT:     return "timeout";
        case MRCA_ERR_UNSUPPORTED: return "unsupported";
        case MRCA_ERR_AGAIN:       return "would block";
        case MRCA_ERR_STATE:       return "bad state";
        default:                   return "unknown";
    }
}

const char *mrca_op_str(int op) {
    switch (op) {
        case OP_HELLO:    return "HELLO";
        case OP_WELCOME:  return "WELCOME";
        case OP_INPUT:    return "INPUT";
        case OP_FRAME:    return "FRAME";
        case OP_STAT:     return "STAT";
        case OP_QUALITY:  return "QUALITY";
        case OP_PING:     return "PING";
        case OP_PONG:     return "PONG";
        case OP_BYE:      return "BYE";
        case OP_AUDIO:    return "AUDIO";
        case OP_KEYFRAME: return "KEYFRAME";
        case OP_CLIPBOARD:return "CLIPBOARD";
        case OP_SCROLL_HINT: return "SCROLL_HINT";
        default:          return "?";
    }
}
