#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <netinet/tcp.h>

#define EXIT "exit"
#define MAX_CONNECTIONS 100
#define ID_MAX_LENGTH 10

struct udp_details_pkg {
    uint16_t port;
    uint32_t ip;
};

struct udp_pkg {
    char topic[50];
    uint8_t data_type;
    char content[1500];
};

/*data_type will say hold the information for sf and sub options: 
    0 - unsub
    1 - no sf and sub
    2 - sf and sub
*/ 
struct tcp_pkg {
    char topic[51];
    uint8_t data_type;
};

struct server_pkg {
    struct udp_pkg udp_pkg;
    struct udp_details_pkg details;
};

int recv_all ( int sockfd, void* buffer, int len );