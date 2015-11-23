#include "a3.h"

int main(int argc, char **argv){
	struct hwa_info *hwa, *tmp;
	struct sockaddr *sa;
	struct sockaddr_un client_addr;
	int cur,i;
	int sockfd;
	int s,r;
	int flag;
	int src_port;
	char tempfile[MAXLINE];
	char sendBuf[MSG_SIZE]="Hello!";
	char recBuf[MSG_SIZE];
	char src_addr[16];
	char buf[MAXLINE];
	fd_set rset;
	struct timeval tv;
	time_t ts;
	

	printf("\n====Client Service====\n\n");

	if((hwa=get_hw_addrs())==NULL)
		err_sys("Getting interface information error!\n");

/*	//get IP address except for the eth0
	sa = hwa->hwa_next->ip_addr;

	//get node information
	i = 0;
	while((Sock_ntop_host(sa, sizeof(*sa)) != vms[i]) && i < 10){
		i++;
	}
	node = i+1;
	printf("\nThis is vm%d \n\n",node);*/

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

	strcpy(tempfile, "fnameXXXXXX");

	//get unique temp filename
	if(mkstemp(tempfile)<0)
		err_sys("Getting unique name error!\n");

	//remove the file
	unlink(tempfile);

	bzero(&client_addr, sizeof(client_addr));
	client_addr.sun_family=AF_LOCAL;
	strcpy(client_addr.sun_path, tempfile);
	printf("\nSun path is %s \n\n", client_addr.sun_path);

	//create UNIX domain socket
	if((sockfd=socket(AF_LOCAL, SOCK_DGRAM, 0))<0)
		err_sys("Creating socket error!");

	if(bind(sockfd, (SA *) &client_addr, sizeof(client_addr))<0)
		err_sys("Bind error!\n");

	while(1) {

    	printf("Please choose VM from 1 to 10 as a server node: \n");
    	
        if(scanf("%d", &s)<0)
        	err_sys("Getting choice error!\n");

        //send request to local ODR
		msg_send(sockfd, vms[s-1], 3000, sendBuf, 0);
        printf("\nclient at node vm %d sending request to server at vm %d \n\n", cur, s);
  	
		//receive reply from local ODR
		printf("...Waiting...\n\n");
		
		RECEIVE:		
		//3 seconds timeout
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);
		tv.tv_sec=3;
    	tv.tv_usec=0;
		
		if((flag = select(sockfd+1, &rset, NULL, NULL, &tv))<0)
			err_sys("Reading from local ODR error!\n");

		if(flag == 0) {	//timeout and retransmit        
            printf("client at node vm %d: timeout on response from vm %d \n\n", cur, s);

            //set force discovery flag
            msg_send(sockfd, vms[s-1], 3000, sendBuf, 1);
            printf("\nclient at node vm %d sending request to server at vm %d \n\n", cur, s);
            goto RECEIVE;
        }else{
        	//receive message	
			msg_recv(sockfd, recBuf, src_addr, &src_port);

			//get source node index of reply
			for(i = 0; i < 10; i++){
				if(strcmp(src_addr, vms[i])==0) {
					r=i+1;
					break;
				}
			}

			//get time stamp
			ts=time(NULL);
			memset(buf,0,sizeof(buf));   
        	snprintf(buf, sizeof(buf), "%.24s\r\n", ctime(&ts));
			printf("client at node vm %d: received from vm %d: <%s>\n", cur, r, buf);
		}
	}

	unlink(client_addr.sun_path);

	return 0; 

}