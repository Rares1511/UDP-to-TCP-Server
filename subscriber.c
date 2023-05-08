#include <math.h>

#include "common.h"
#include "helpers.h"

#define SUB "subscribe"
#define UNSUB "unsubscribe"

char data_type_string[4][11] = { "INT", "SHORT_REAL", "FLOAT", "STRING" };

int send_sub_package ( int sockfd, char* topic, uint8_t sf, uint8_t sub ) {
    struct tcp_pkg pakcage;
    memset ( pakcage.topic, 0, sizeof ( pakcage.topic ) );
    strcpy ( pakcage.topic, topic );
    pakcage.data_type = sub + sf;
    int rc = send ( sockfd, &pakcage, sizeof ( pakcage ), 0 );
    return rc;
}

void print_details ( struct udp_details_pkg details ) {
    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = details.ip;
    printf ( "%s:%d - ", inet_ntoa ( serv_addr.sin_addr ), details.port );
}

void print_topic ( struct udp_pkg pkg ) {
    printf ( "%s - %s - ", pkg.topic, data_type_string[pkg.data_type] );
}

int main ( int argc, char** args ) {

    if ( argc != 4 ) {
        printf ( "\n Usage: %s <ID> <ip> <port>\n", args[0] );
        return 1;
    }

    setvbuf ( stdout, NULL, _IONBF, BUFSIZ );

    char* ID = args[1];
    uint16_t port;
    int rc = sscanf ( args[3], "%hu", &port );
    DIE ( rc != 1, "Given port is invalid" );

    int sockfd = socket ( AF_INET, SOCK_STREAM, 0 );
    DIE ( sockfd < 0, "socket" );

    int flag = 1;
    rc = setsockopt ( sockfd, IPPROTO_TCP, TCP_NODELAY, ( char* ) &flag, sizeof ( int ) ); 
    DIE ( rc < 0, "TCP NO DELAY option" );  

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof ( struct sockaddr_in );

    memset ( &serv_addr, 0, socket_len );
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons ( port );
    rc = inet_pton ( AF_INET, args[2], &serv_addr.sin_addr.s_addr );
    DIE ( rc <= 0, "inet_pton" );

    rc = connect ( sockfd, ( struct sockaddr* ) &serv_addr, sizeof ( serv_addr ) );
    DIE ( rc < 0, "connect" );

    rc = send ( sockfd, ID, sizeof ( args[1] ), 0 );
    DIE ( rc < 0, "send" );

    int available = 0;
    rc = recv ( sockfd, &available, sizeof ( available ), 0 );
    DIE ( rc < 0, "recv" );

    if ( available == -1 ) {
        close ( sockfd );
        return 0;
    }

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_clients = 2;
    poll_fds[0].fd = 0;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    struct server_pkg package;

    int ok = 1;

    while ( ok ) {
        rc = poll ( poll_fds, num_clients, -1 );
        DIE ( rc < 0, "poll" );

        if ( poll_fds[0].revents & POLLIN ) {
            char command[100], topic[100];
            uint8_t sf;
            scanf ( "%s", command );
            if ( !strcmp ( command, "exit" ) ) break;
            else if ( !strcmp ( command, SUB ) ) {
                scanf ( "%s %hhd", topic, &sf );
                rc = send_sub_package ( sockfd, topic, sf, 1 );
                DIE ( rc < 0, "send sub package" );
                printf ( "Subscribed to topic.\n" );
            }
            else if ( !strcmp ( command, UNSUB ) ) {
                scanf ( "%s", topic );
                rc = send_sub_package ( sockfd, topic, 0, 0 );
                DIE ( rc < 0, "send unsub package" );
                printf ( "Unsubscribed from topic.\n" );
            }
        }
        if ( poll_fds[1].revents & POLLIN ) {
            rc = recv_all ( sockfd, &package, sizeof ( package ) );
            if ( rc == 0 ) break;
            print_details ( package.details );
            print_topic ( package.udp_pkg );
            if ( package.udp_pkg.data_type == 0 ) {
                uint8_t sign = ( uint8_t ) package.udp_pkg.content[0];
                int64_t number = 0;
                for ( int j = 1; j <= 4; j ++ ) {
                    number = ( number << 8 ) + ( uint8_t ) package.udp_pkg.content[j];
                }

                if ( sign == 1 )
                    number = - number;
                
                printf ( "%ld\n", number );
            }
            else if ( package.udp_pkg.data_type == 1 ) {
                uint16_t number = 0;
                for ( int j = 0; j < 2; j++ ) {
                    number = ( number << 8 ) + ( uint8_t ) package.udp_pkg.content[j];
                }
                printf ( "%0.2f\n", number * 1.0 / 100 );
            }
            else if ( package.udp_pkg.data_type == 2 ) {
                uint8_t sign = package.udp_pkg.content[0];
                int64_t number = 0;
                for ( int j = 1; j <= 4; j ++ ) {
                    number = ( number << 8 ) + ( uint8_t ) package.udp_pkg.content[j];
                }
                uint8_t power = package.udp_pkg.content[5];
                float real_number = number * 1.0 / pow ( 10, power );
                if ( sign == 1 )
                    real_number = - real_number;
                if ( real_number == ( int ) real_number )
                    printf ( "%d\n", ( int ) real_number );
                else
                    printf ( "%0.4f\n", real_number );
            }
            else { 
                printf ( "%s\n", package.udp_pkg.content );
            }
        }
    }

    close ( sockfd );

    return 0;
}