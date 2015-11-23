#include "a3.h"

int msg_send(int sockfd, char *dest_addr, int dest_port, char *msg, int flag) {
    char dat[MSG_SIZE];

    sprintf(dat, "%s;%d;%d;%s", dest_addr, dest_port, flag, msg);
    printf("\nSending Stream : %s", dat);

    struct sockaddr_un addr;
    bzero(&addr, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, ODR_PATH);

    if (sendto(sockfd, dat, strlen(dat), 0, (SA *) &addr, sizeof(addr)) < 0) {
        printf("send error\n");
        exit(1);
    }
    printf("sent  message\n");
    return 0;

}

int msg_recv(int sockfd, char *msg, char *src_addr, int *src_port)  
{

    char dat[MSG_SIZE + 1];

    int len = sizeof(struct sockaddr_un);
//    printf("\nWaiting on receive. \n");
    if (recvfrom(sockfd, dat, MSG_SIZE, 0, 0, 0) < 0)
    {
        printf("recv error");
        exit(1);
    }
    printf("received\n");
    sscanf(dat, "%s %s %d", msg, src_addr, src_port);

    return 0;
} 