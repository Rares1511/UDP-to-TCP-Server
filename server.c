#include "common.h"
#include "helpers.h"

#define SOCKADDR_IN_SIZE sizeof ( struct sockaddr_in )

struct client {
    char ID[10];
    char topics[50][51];
    int subed_topics, real_poz;
    char sf_topics[50][51];
    int no_sf_topics;
    struct server_pkg *pkgs;
    int remaining_pkgs;
};
struct client *online_cl, *clients;
struct pollfd *poll_fds;
int num_online, num_cl, online_cap, client_cap;

int start_socketfd ( enum __socket_type type, struct sockaddr_in servaddr ) {
    int socketfd, rc;

    socketfd = socket ( AF_INET, type, 0 );
    DIE ( socketfd < 0, "socket" );
 
    int enable = 1;
    rc = setsockopt ( socketfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof ( int ) );
    DIE ( rc < 0, "options" );

    if ( type == SOCK_STREAM ) {
        int flag = 1;
        rc = setsockopt ( socketfd, IPPROTO_TCP, TCP_NODELAY, ( char* ) &flag, sizeof ( int ) ); 
        DIE ( rc < 0, "TCP NO DELAY option" );  
    }

    rc = bind ( socketfd, ( const struct sockaddr* ) &servaddr, sizeof ( servaddr ) );
    DIE ( rc < 0, "bind" );  

    if ( type == SOCK_STREAM ) {
        rc = listen ( socketfd, MAX_CONNECTIONS );
        DIE ( rc < 0, "listen" );
    }

    return socketfd;
}

int check_id ( char* ID ) {
    for ( int i = 3; i < num_online; i++ )
        if ( !strcmp ( ID, online_cl[i].ID ) ) 
            return -1;
    return 1;
}

int is_subscribed ( struct client cl, char* topic ) {
    for ( int i = 0; i < cl.subed_topics; i++ ) {
        if ( !strcmp ( cl.topics[i], topic ) ) return 1;
    }
    return 0;
}

int is_subscribed_sf ( struct client cl, char* topic ) {
    for ( int i = 0; i < cl.no_sf_topics; i++ ) {
        if ( !strcmp ( cl.sf_topics[i], topic ) ) return 1;
    }
    return 0;
}

int is_new_client ( char* ID ) {
    for ( int i = 0; i < num_cl; i++ ) {
        if ( !strcmp ( clients[i].ID, ID ) ) return i;
    }
    return -1;
}

int is_online ( struct client cl ) {
    for ( int i = 3; i < num_online; i++ )
        if ( !strcmp ( online_cl[i].ID, cl.ID ) ) return 1;
    return 0;
}

int subscribe ( struct client *cl1, struct client *cl2, char* topic, int type ) {
    if ( is_subscribed ( *cl1, topic ) && type == 0 ) return 1;
    if ( is_subscribed_sf ( *cl1, topic ) ) return 1;
    if ( type == 1 ) {
        strcpy ( cl1->sf_topics[cl1->no_sf_topics], topic );
        cl1->no_sf_topics++;
        strcpy ( cl2->sf_topics[cl2->no_sf_topics], topic );
        cl2->no_sf_topics++;
    }
    strcpy ( cl1->topics[cl1->subed_topics], topic );
    cl1->subed_topics++;
    strcpy ( cl2->topics[cl2->subed_topics], topic );
    cl2->subed_topics++;
    return 2;
}

int unsubscribe ( struct client *cl1, struct client *cl2, char* topic ) {
    if ( !is_subscribed ( *cl1, topic ) ) return 1;
    for ( int i = 0; i < cl1->subed_topics; i++ ) {
        if ( !strcmp ( cl1->topics[i], topic ) ) {
            for ( int j = i; j < cl1->subed_topics - 1; j++ ) {
                strcpy ( cl1->topics[j], cl1->topics[j + 1] );
                strcpy ( cl2->topics[j], cl2->topics[j + 1] );
            }
            break;
        }
    }
    cl1->subed_topics--;
    cl2->subed_topics--;
    if ( !is_subscribed_sf ( *cl1, topic ) ) return 1;
    for ( int i = 0; i < cl1->no_sf_topics; i++ ) {
        if ( !strcmp ( cl1->sf_topics[i], topic ) ) {
            for ( int j = i; j < cl1->no_sf_topics - 1; j++ ) {
                strcpy ( cl1->sf_topics[j], cl1->sf_topics[j + 1] );
                strcpy ( cl2->sf_topics[j], cl2->sf_topics[j + 1] );
            }
            break;
        }
    }
    cl1->no_sf_topics--;
    cl2->no_sf_topics--;
    return 2;
}

void send_udp_msg ( struct server_pkg package ) {
    for ( int i = 3; i < num_online; i++ ) {
        if ( is_subscribed ( online_cl[i], package.udp_pkg.topic ) ) {
            int rc = send ( poll_fds[i].fd, &package, sizeof ( package ), 0 );
            DIE ( rc < 0, "send" );
        }
    }
}

struct server_pkg recv_udp_msg ( int sockfd ) {

    struct server_pkg pkg;
    struct sockaddr_in serv_addr;
    struct udp_details_pkg details;
    socklen_t socket_len = sizeof ( struct sockaddr_in );
    memset ( &serv_addr, 0, socket_len );
    struct udp_pkg package;

    int rc = recvfrom ( sockfd, &package, sizeof ( package ), 0, ( struct sockaddr* ) &serv_addr, &socket_len );
    DIE ( rc < 0, "recvfrom" );

    details.ip = serv_addr.sin_addr.s_addr;
    details.port = ntohs ( serv_addr.sin_port );

    pkg.details = details;
    pkg.udp_pkg = package;

    for ( int i = 0; i < num_cl; i++ ) {
        if ( is_online ( clients[i] ) ) continue;
        if ( !is_subscribed_sf ( clients[i], package.topic ) ) continue;
        clients[i].pkgs[clients[i].remaining_pkgs] = pkg;
        clients[i].remaining_pkgs++;
    }

    return pkg;
}

void accept_connection ( int socket ) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof ( cli_addr );
    int newsockfd = accept ( socket, ( struct sockaddr* ) &cli_addr, &cli_len );
    DIE ( newsockfd < 0, "accept" );

    char ID[ID_MAX_LENGTH];

    int rc = recv ( newsockfd, ID, ID_MAX_LENGTH, 0 );
    DIE ( rc < 0, "recv" );

    int available = check_id ( ID );
    rc = send ( newsockfd, &available, sizeof ( available ), 0 );
    DIE ( rc < 0, "send" );

    if ( available == -1 ) {
        printf ( "Client %s already connected.\n", ID );
        return;
    }

    if ( num_online == online_cap ) {
        poll_fds = ( struct pollfd* ) realloc ( poll_fds, 2 * online_cap * sizeof ( struct pollfd ) );
        online_cl = ( struct client* ) realloc ( online_cl, 2 * online_cap * sizeof ( struct client ) );
        online_cap *= 2;
    }
    poll_fds[num_online].fd = newsockfd;
    poll_fds[num_online].events = POLLIN;

    int poz = is_new_client ( ID );
    strcpy ( online_cl[num_online].ID, ID );
    if ( poz == -1 ) {
        if ( num_cl == client_cap ) {
            clients = ( struct client* ) realloc ( clients, 2 * client_cap * sizeof ( struct client ) );
            client_cap *= 2;
        }
        strcpy ( clients[num_cl].ID, ID );
        online_cl[num_online].subed_topics = 0;
        online_cl[num_online].no_sf_topics = 0;
        clients[num_cl].no_sf_topics = 0;
        clients[num_cl].subed_topics = 0;
        clients[num_cl].remaining_pkgs = 0;
        clients[num_cl].pkgs = calloc ( MAX_CONNECTIONS, sizeof ( struct server_pkg ) );
        online_cl[num_online].real_poz = num_cl;
        num_cl++;
    }
    else {
        for ( int i = 0; i < clients[poz].remaining_pkgs; i++ ) {
            rc = send ( newsockfd, &clients[poz].pkgs[i], sizeof ( clients[poz].pkgs[i] ), 0 );
            DIE ( rc < 0, "sf send" );
        }
        clients[poz].remaining_pkgs = 0;
        online_cl[num_online] = clients[poz];
        online_cl[num_online].real_poz = poz;
    }

    printf ( "New client %s connected from %s:%d.\n", online_cl[num_online].ID, inet_ntoa ( cli_addr.sin_addr ), ntohs ( cli_addr.sin_port ) );
    num_online++;
}

void init ( ) {
    online_cl = calloc ( MAX_CONNECTIONS, sizeof ( struct client ) );
    clients = calloc ( MAX_CONNECTIONS, sizeof ( struct client ) );
    poll_fds = calloc ( MAX_CONNECTIONS, sizeof ( struct pollfd ) );
    online_cap = MAX_CONNECTIONS;
    client_cap = MAX_CONNECTIONS;
}

int main ( int argc, char** args ) {

    if ( argc != 2 ) {
        printf ( "\n Usage: %s <port>\n", args[0] );
        return 1;
    }

    setvbuf ( stdout, NULL, _IONBF, BUFSIZ );
    
    uint16_t port = atoi ( args[1] );

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof ( struct sockaddr_in );

    memset ( &serv_addr, 0, socket_len );
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl ( INADDR_ANY );
    serv_addr.sin_port = htons ( port ); 

    int tcp_sockfd = start_socketfd ( SOCK_STREAM, serv_addr );
    int udp_sockfd = start_socketfd ( SOCK_DGRAM, serv_addr );

    init ( );

    poll_fds[0].fd = 0;             // stdin socket
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = udp_sockfd;    // udp socket 
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = tcp_sockfd;    // tcp socket
    poll_fds[2].events = POLLIN;
    num_online = 3;

    struct tcp_pkg tcp_pkg;
    int ok = 1;

    while ( ok ) {
        int rc = poll ( poll_fds, num_online, -1 );
        DIE ( rc < 0, "poll" );
        for ( int i = 0; i < num_online; i++ ) {
            if ( poll_fds[i].revents & POLLIN ) {
                if ( poll_fds[i].fd == 0 ) {
                    char command[50];
                    scanf ( "%s", command );
                    if ( !strcmp ( command, "exit" ) ) { ok = 0; break; }
                }
                else if ( poll_fds[i].fd == tcp_sockfd ) {
                    accept_connection ( tcp_sockfd );
                }
                else if ( poll_fds[i].fd == udp_sockfd ) {
                    send_udp_msg ( recv_udp_msg ( udp_sockfd ) );
                }
                else {
                    rc = recv ( poll_fds[i].fd, &tcp_pkg, sizeof ( tcp_pkg ), 0 );
                    DIE ( rc < 0, "recv");

                    if ( rc == 0 ) {
                        close ( poll_fds[i].fd );

                        char ID[11];
                        strcpy ( ID, online_cl[i].ID );

                        poll_fds[i].fd = 0;
                        poll_fds[i].events = 0;
                        poll_fds[i].revents = 0;

                        for ( int j = i; j < num_online - 1; j++ ) {
                            poll_fds[j] = poll_fds[j + 1];
                            online_cl[j] = online_cl[j + 1];
                        }

                        num_online--;

                        printf ( "Client %s disconnected.\n", ID );
                    }
                    else {
                        if ( tcp_pkg.data_type == 0 ) {
                            unsubscribe ( &online_cl[i], &clients[online_cl[i].real_poz], tcp_pkg.topic );
                        }
                        else {
                            subscribe ( &online_cl[i], &clients[online_cl[i].real_poz], tcp_pkg.topic, tcp_pkg.data_type - 1 );
                        }
                    }
                }
            }
        }
    }

    close ( udp_sockfd );
    close ( tcp_sockfd );

    return 0;
}