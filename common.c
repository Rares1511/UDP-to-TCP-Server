#include "common.h"

int recv_all ( int sockfd, void* buffer, int len ) {
    size_t bytes_remaining = len;
    char *buff = buffer;

    while ( bytes_remaining > 0 ) {
        size_t bytes_processed = recv ( sockfd, buff, bytes_remaining, 0 );
        if ( bytes_processed == 0 ) return 0;
        bytes_remaining -= bytes_processed;
        buff += bytes_processed;
    }

    return len;
}