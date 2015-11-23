#include "a3.h"

char* vms[] = {
	"130.245.156.21",
	"130.245.156.22",
	"130.245.156.23",
	"130.245.156.24",
	"130.245.156.25",
	"130.245.156.26",
	"130.245.156.27",
	"130.245.156.28",
	"130.245.156.29",
	"130.245.156.20",
};

int main(int argc, const char * argv[]) {
	printf("Server running\n");

	struct hwa_info *hwa, *tmp;
    struct sockaddr *sa;
	int sockfd;
	struct sockaddr_un srvaddr;
	if((hwa = get_hw_addrs())==NULL) {
		printf("get_hw_addrs error\n");
		exit(1);
	}
	int cur;
	int i;
	for (tmp = hwa; tmp != NULL; tmp = tmp->hwa_next) {
		if (!strcmp(tmp->if_name, "eth0") && tmp->ip_addr != NULL) {
		  	sa = tmp->ip_addr;
			for(i=0; i<10; i++) {
				if(strcmp(vms[i], Sock_ntop_host(sa, sizeof(*sa)))==0) {
					cur=i+1;
					break;
				}
			}
			break;
		}
	}	
	printf("Current VM: %d\n", cur);

	if((sockfd=socket(AF_LOCAL, SOCK_DGRAM, 0))<0) {
		printf("cannot create socket\n");
		exit(1);
	}
	bzero(&srvaddr, sizeof(srvaddr));
	srvaddr.sun_family=AF_LOCAL;
	strcpy(srvaddr.sun_path, SERV_PATH);
    unlink(srvaddr.sun_path);
	if(bind(sockfd, (SA *) &srvaddr, sizeof(srvaddr))<0) {
		printf("bind socket error\n");
		exit(1);
	}
	int recv;	
	int src_port;
	char src_addr[16];	
	char recBuf[MSG_SIZE];
	char sendBuf[MSG_SIZE];
	time_t tv;

	while(1) {
		msg_recv(sockfd, recBuf, src_addr, &src_port);
		for(i=0; i<10; i++) {
			if(strcmp(vms[i], src_addr) == 0) {
				recv=i+1;
				break;
			}
		}
		printf("Server at VM%d received client msg from VM%d\n", cur, recv);
		tv=time(NULL);       
        snprintf(sendBuf, sizeof(sendBuf), "%.24s\r\n", ctime(&tv));
		msg_send(sockfd, vms[recv-1], src_port, sendBuf, 0);
		printf("Reply sent from VM%d to VM%d\n", cur, recv);
	}
	return 0; 
}