#include "a3.h"

struct routing_entry {
	char dst_addr[ADDR_LEN];	
	unsigned char next_hop[6];	
	int outgoing_if;
	int hop_cnt;
	struct timeval timestamp;
};

//Ethernet frame
struct eth_frame {
	int type;
	unsigned char dst_mac[6];
	unsigned char src_mac[6];
	unsigned short protocol_no;
	char payload[PAYLOAD_LEN];
};

struct RREQ {
	char src_addr[ADDR_LEN];
	char dst_addr[ADDR_LEN];
	int src_port;
	int broadcast_id;
	int hop_cnt;
	int RREP_flag;
	int flag;
};

struct RREP {
	char src_addr[ADDR_LEN];
	char dst_addr[ADDR_LEN];
	int dst_port;
	int hop_cnt;
	int flag;
};

struct payload {
	char src_addr[ADDR_LEN];
	char dst_addr[ADDR_LEN];
	int src_port;
	int dst_port;
	int hop_cnt;
	int len;
	char message[30];
};

int update_table(char src_addr, int flag, int hop_count, struct eth_frame *frame, int index);
int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port);
int sendPacket(struct eth_frame *frame, struct sockaddr_ll *send_if_addr, void *packet, int type, char dst_addr[], char src_addr[]);

struct sockaddr_ll if_addr[3];
//local routing table
struct routing_entry routing_table[10];
//local port talbe
struct port_entry port_table[10];

int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port){
	int length;
	int frame_len = sizeof(struct eth_frame);
	char buf[frame_len];

	if((length = sprintf(buf, "%s %s %d", msg, src_addr, port)) < 0){
		err_sys("Format error!\n");
		return 0;
	}

	//find the local client the RREP is for
	int i;
	for(i = 0; i <= port_cnt; i++) {		
		if(port_table[i].port == (dst_port)) {
			local_addr->sun_path = port_table[i].sun_path;
			break;
		}
	}
						
	if(sendto(odrfd, buf, length, 0, (SA *) local_addr, sizeof(*local_addr))<0){
		err_sys("Sending error!\n");
		return 0;
	}

	return 1;
}

int sendPacket(struct eth_frame *frame, struct sockaddr_ll *send_if_addr, void *packet, int type, char dst_addr[], char src_addr[]){
	//prepare packet for retransmit

	int temp_index = 0;
	bzero(frame, sizeof(struct eth_frame));

	frame->protocol_no = PROTOCOL_NO;
	frame->type = type;
	int n;
	for(n = 0; n < 10; n++) {
		if(dst_addr == routing_table[n].dst_addr) {
			memcpy(frame->dst_mac, routing_table[n].next_hop, ETH_ALEN);
			temp_index = routing_table[n].outgoing_if-3;
			memcpy(frame->src_mac, if_addr[temp_index].sll_addr, ETH_ALEN);
			break;
		}
	}

	bzero(send_if_addr, sizeof(struct sockaddr_ll));

	send_if_addr->sll_family = PF_PACKET;
	send_if_addr->sll_protocol = PROTOCOL_NO;
	send_if_addr->sll_halen = ETH_ALEN;
	send_if_addr->sll_addr[6] = 0x00;	//not used
	send_if_addr->sll_addr[7] = 0x00;	//not used

	if(type == 2){
		//app payload
		send_if_addr->sll_hatype=ARPHRD_ETHER;
		send_if_addr->sll_pkttype=PACKET_OTHERHOST;
		printf("\nRREP has been prepared for sending.\n\n");
	}else if(type == 1)
	{
		//RREQ
		printf("\nAPP payload has been prepared for sending.\n\n");
	}else if(type == 0){
		frame->dst_mac[0] = 0xff;
		frame->dst_mac[1] = 0xff;
		frame->dst_mac[2] = 0xff;
		frame->dst_mac[3] = 0xff;
		frame->dst_mac[4] = 0xff;
		frame->dst_mac[5] = 0xff;

		send_if_addr->sll_addr[0]=0xff;		
		send_if_addr->sll_addr[1]=0xff;		
		send_if_addr->sll_addr[2]=0xff;
		send_if_addr->sll_addr[3]=0xff;
		send_if_addr->sll_addr[4]=0xff;
		send_if_addr->sll_addr[5]=0xff;

		//RREQ floods
		return 0;

	}else{
		err_sys("Type error!\n");
		return 1;
	}

	for(n=0; n<10; n++){
		if(dst_addr == routing_table[n].dst_addr) {
			temp_index = routing_table[n].outgoing_if;
			send_if_addr->sll_addr = frame->dst_mac;
			send_if_addr->sll_ifindex=temp_index;
			break;
		}
	}

	memcpy((void *)(frame->payload), (void *)packet, sizeof(*packet));
	temp_index = routing_table[n].outgoing_if-3;
	if(sendto(if_fd[temp_index], frame, sizeof(*frame), 0, (SA *) send_if_addr, sizeof(*send_if_addr))<0)
		err_sys("Sending error!\n");
	else
		printf("ODR at node vm%d: %s, sending frame, src:vm%d dest:%s\n\n", cur, src_addr, cur, dst_addr);

	return 0;
}

int main(int argc, char **argv){
	int staleness_time;
	
	/*------------- Check Parameter Starts -------------*/
	
	if (argc<2)
		err_sys("Usage: %s [staleness time]\n",argv[0]);
	if (staleness_time=atoi(argv[1])==0)
		err_sys("invalid staleness time!\n");
	
	/*------------- Check Parameter Ends -------------*/

	

	/*------------- Obtain Interfaces Starts -------------*/
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr; 
	int    i, prflag;
	struct sockaddr_ll addr;
	printf("\n");
	char eth0IP[SIZE];
	struct sockaddr_ll interfaces[VMNUMBER];
	int interfaceNumber=0;
	int interfaceFDs[VMNUMBER];
	fd_set              allset,rset;
	int maxfd;

	FD_ZERO(&rset);
  	FD_ZERO(&allset);
  	maxfd=0;
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		
		printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
		if (strcmp(hwa->if_name, "lo") == 0) 
			continue;

			//		sa = hwa->ip_addr;	
        
        //determine which VM node 
        eth0IP=Sock_ntop_host( (struct sockaddr*)hwa->ip_addr, sizeof(struct sockaddr* ) ); 
        if (strcmp(hwa->if_name, "eth0") == 0 ){
        	for (i=0;i<VMNUMBER;i++)
        	{
        		if (strcmp(vms[i],eth0IP)==0){
        			//currentVM=i;
        			printf("Current VM node is %d, the VM IP is %s\n",i+1,eth0IP);
        			break;
        		}
        	}
        	continue;
        }

		prflag = 0;
		i = 0;
		do {
			if (hwa->if_haddr[i] != '\0') {
				prflag = 1;
				break;
			}
		} while (++i < IF_HADDR);

		if (prflag) {
			printf("         HW addr = ");
			ptr = hwa->if_haddr;
			i = IF_HADDR;
			do {
				printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
			} while (--i > 0);
		}

		printf("\n         interface index = %d\n\n", hwa->if_index);

		interfaces[interfaceNumber].sll_family=PF_PACKET;
		interfaces[interfaceNumber].sll_protocol=htons(PROTOCOL_NOCOL_VALUE);
		interfaces[interfaceNumber].sll_ifindex=hwa->if_index;
		memcpy(interfaces[interfaceNumber].sll_addr, hwa->if_haddr, ETH_ALEN);
		
		//Create a PF_PACKET socket for every interface
		interfaceFDs[interfaceNumber]=Socket(PF_PACKET,SOCK_RAW,htons(PROTOCOL_NOCOL_VALUE));  //set the value in header file ?
		Bind(interfaceFDs[interfaceNumber], (SA *) &interfaces[interfaceNumber], sizeof(SA*));
		FD_SET(interfaceFDs[interfaceNumber], &allset);
		maxfd=interfaceFDs[interfaceNumber]>maxfd?interfaceFDs[interfaceNumber]:maxfd;
		
		interfaceNumber++;
	}

	free_hwa_info(hwahead);
    
	printf("%d sockets are binded for all of the interfaces\n",interfaceNumber);

	/*------------- Obtain Interfaces Ends -------------*/




	/*------------- Create UNIX DOMAIN Socket Starts-------------*/

	int sockFD;
	socklen_t len;
	struct sockaddr_un addr1;
	
	sockfd = Socket(AF_LOCAL, SOCK_STREAM, 0); 
	unlink(ODR_PATH); /* OK if this fails */
	bzero(&addr1, sizeof(addr1));
	addr1.sun_family = AF_LOCAL;
	strncpy(addr1.sun_path, ODR_PATH, sizeof(addr1.sun_path) - 1); 
	Bind(sockfd, (SA *) &addr1, SUN_LEN(&addr1));
	printf("ODR Socket was binded with path name: %s\n",ODR_PATH);
	FD_SET(sockfd, &allset);
	maxfd=sockfd>maxfd?sockfd:maxfd;
	/*------------- Create UNIX DOMAIN Socket ENDS-------------*/


	/*------------- Server Starts -------------*/
	int status;
	char buf[BUF_SIZE];
	struct sockaddr_un addr;
	socklen_t addrlen;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	maxfd++;
	
	for (;;)
	{
		rset= allset;
    	status = select(maxfd , &rset, NULL, NULL, NULL);
    	if (status == -1) {
        	if (errno == EINTR)
            	continue;
        	err_sys("select error");
    	}

    	if (FD_ISSET(sockfd,&rset)){
    		addrlen=sizeof(addr); 
    		Recvfrom(sockfd,buf,BUF_SIZE,0,(SA *) &addr, &addrlen)
    		printf("received new message from local: %s , with path name: %s\n",buf,addr.sun_path);
    		sscanf(buf, "%s %d %s %d", dst_addr, &dst_port, message, &flag);
    		if (addr.sun_path==SRV_PATH)
    		{ 	//local server
    			for (i=0;i<VMNUMBER;i++)
    			{
    				if (strcmp(routing_table[i].dst_addr,dst_addr)==0)
    					break;
    			}
    			struct hdr *header=(struct hdr *) malloc(sizeof(struct hdr));				
				struct payload *load=(struct payload *) malloc(sizeof(struct payload));
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
				//application payload header
				memcpy(header->dst_mac, routing_table[i].next_hop, ETH_ALEN);
				memcpy(header->src_mac, if_addr[routing_table[i].outgoing_if-3].sll_addr, ETH_ALEN);
				header->proto=htons(PROTOCOL_NO);
				header->type=htonl(2);

				//application payload ODR protocol message
				strcpy(load->src_addr, eth0IP);
				strcpy(load->dst_addr, dst_addr);
				load->src_port=htonl(5000); //?? 
				load->dst_port=htonl(dst_port);
				load->hop_cnt=htonl(0);
				load->len=htonl(sizeof(message));
				strcpy(load->message, message);
				
				//application payload Ethernet frame
				memcpy((void *) ether_frame, (void *) header, sizeof(*header));
				memcpy((void *) (ether_frame+sizeof(*header)), (void *) load, sizeof(*load));
				printf("Application payload is ready to be sent out to ");
				ptr=header->dst_mac;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);

				bzero(&send_if_addr, sizeof(struct sockaddr_ll));
				send_if_addr.sll_family=PF_PACKET;
				send_if_addr.sll_protocol=htons(PROTOCOL_NO);
				send_if_addr.sll_ifindex=routing_table[number].outgoing_if;
				send_if_addr.sll_halen=ETH_ALEN;
				memcpy(send_if_addr.sll_addr, routing_table[number].next_hop, ETH_ALEN);
				send_if_addr.sll_addr[6]=0x00;
				send_if_addr.sll_addr[7]=0x00;

				//application payload is sent
				if(sendto(if_fd[routing_table[number].outgoing_if-3], ether_frame, sizeof(*header)+sizeof(*load), 0, 
						(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

					perror("ODR: Failed to send Ethernet frame throught interface socket\n");
					exit(1);
				}

				printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
				ptr=send_if_addr.sll_addr;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);
				
				printf("	ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);

				free(header);
				free(load);
				free(ether_frame);
									

    		}
    		else
    		{	//local client
    			return 0;
    		}

    	}	
    
	


	/*------------- J -------------*/
	for(index=0; index<if_cnt; index++) {
			int i;
			//RRQ, RRP, or payload received from one of the interface socket
			if(FD_ISSET(if_fd[index], &rset)) {
			
				printf("New Ethernet frame was received from interface %d\n", index+3);

				eth_frame *frame=(eth_frame *) malloc(sizeof(struct eth_frame));

				if(recvfrom(if_fd[index], frame, ETH_FRAME_LEN, 0, NULL, NULL)<0) {

					perror("ODR: Failed to receive Ethernet frame througth interface socket\n");
					exit(1);
				}

				memcpy(recv_mac, frame->src_mac, 6);
				printf("receive frame of type %d from ", ntohl(frame->protocol_no));
				//print mac address
				ptr=recv_mac;
				for (i = 0; i < 6; ++i)
				{
					printf("%02x%s", *ptr++ & 0xff);
					if (i < 5){
						printf(":");
					}
					else{
						printf("\n");
					}
				}
        		/*no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);*/

        		//RREQ received				
				if(ntohl(frame->type)==0) {				
					printf("Received new RReq msg\n");
					struct RREQ *req=(struct RREQ *) malloc(sizeof(struct RREQ));
					memcpy((void *) req, (void *) frame->payload), sizeof(*req));
					
					int ind;
					int updateFlag=0;
					
					//add or update 'reverse' path to source node in routing table
					for(i=0; i<10; i++) {
						if(strcmp(req->src_addr, routing_table[i].dst_addr)==0) {
							//new route to source is more efficient
							if(ntohl(req->flag)==1 || routing_table[i].outgoing_if==0 
								|| routing_table[i].hop_cnt >= ntohl(req->hop_cnt)+1) {
								memcpy(routing_table[i].next_hop, frame->src_mac, 6);
								routing_table[i].outgoing_if=index+3;
								routing_table[i].hop_cnt=ntohl(req->hop_cnt)+1;
								printf("Local routing table was updated with new %d hop reverse path to source%d\n", 
											ntohl(req->hop_cnt)+1, i+1);
								updateFlag=1;					
							}
							ind = i;
							break;
						}			
					}
					
					//this node is the destination node				
					if(strcmp(req->dst_addr, vm_addr[node-1])==0) {
					
						printf("This node is the destination node for this RREQ\n");
					
						//reply have not been sent back
						//debug
						if((ntohl(req->RREP_sent)==0 && (routing_table[ind].broadcast_id < ntohl(req->broadcast_id)))
							|| (routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1)) {
						//send rrep

							struct RREP *reply=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(frame, sizeof(struct eth_frame));
							
							//RREP frame
							memcpy(frame->dst_mac, recv_mac, 6);
							memcpy(frame->src_mac, if_addr[routing_table[ind].outgoing_if-3].sll_addr, ETH_ALEN);
							frame->protocol_no=htons(PROTOCOL_NOCOL_NO);
							frame->type=htonl(1);
							
							//RREP ODR protocal message
							strcpy(reply->src_addr, req->dst_addr);
							strcpy(reply->dst_addr, req->src_addr);
							reply->dst_port=htonl(req->src_port);
							reply->hop_cnt=htonl(0);
							reply->flag=htonl(0);
							if (req->flag)
							{
								reply->flag=htonl(1);
							}
														
							//RREP Ethernet frame
  							memcpy((void *) frame->payload, (void *) reply, sizeof(*reply));
  							ptr=frame->dst_mac;
							for (i = 0; i < 6; ++i)
							{
								printf("%02x%s", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NOCOL_NO);
							send_if_addr.sll_ifindex=index+3;
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, header->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
  							
							if(sendto(if_fd[index], frame, sizeof(frame), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								printf("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
							ptr=send_if_addr.sll_addr;
							for (i = 0; i < 6; ++i)
							{
								printf("%02x%s", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
							
							printf("	ODR message type 1, source ip %s, destination ip %s\n", reply->src_addr, reply->dst_addr);
						
							free(reply);	
						}
	
						routing_table[ind].broadcast_id=ntohl(req->broadcast_id);
						
						continue;
					}
					//1122 midnight
					int routeFound = 0;
					//check whether this node has route information to destination node
					for(i =0; i<10; i++) {
						if(strcmp(req->dst_addr, routing_table[i].dst_addr)==0) {
							if(routing_table[i].outgoing_if!=0 && ntohl(req->discovery)==0) {
								routeFound = 1;
								break;
							}
						}
					}
												
					//has route to destination
					if(routeFound) {
						printf("Route to destination found\n");
						
						//reply is sent back
						if((ntohl(req->RREP_sent)==0 && (routing_table[ind].broadcast_id<ntohl(req->broadcast_id) ))
							|| ((routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1))) {
					
							struct RREP *reply=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(frame, ETH_FRAME_LEN);
							
							memcpy(frame->dst_mac, recv_mac, ETH_ALEN);
							memcpy(frame->src_mac, if_addr[routing_table[ind].outgoing_if-3].sll_addr, ETH_ALEN);
							frame->protocol_no=htons(PROTOCOL_NOCOL_NO);
							frame->type=htonl(1);
							
							//prepare RREP
							strcpy(reply->src_addr, req->dst_addr);
							strcpy(reply->dst_addr, req->src_addr);
							reply->dst_port=htonl(req->src_port);
							reply->hop_cnt=htonl(routing_table[count].hop_cnt);
							reply->flag=htonl(0);
							
							//RREP Ethernet frame
  							memcpy((void *) frame->payload, (void *) reply, sizeof(*reply));
  							ptr=frame->dst_mac;
        					for (i = 0; i < 6; ++i)
							{
								printf("%02x%s", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NOCOL_NO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_OTHERHOST;
							send_if_addr.sll_ifindex=index+3;	
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, header->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;	
  							
							if(sendto(if_fd[index], frame, sizeof(frame), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								printf("cannot send prep back\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent frame, source VM%d, destination ", node, node);
							ptr=send_if_addr.sll_addr;
        					for (i = 0; i < 6; ++i)
							{
								printf("%02x%s", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
										
							printf("	ODR message type 1, source ip %s, destination ip %s\n", reply->src_addr, reply->dst_addr);
							
							free(reply);
						}
						
						routing_table[ind].broadcast_id=ntohl(req->broadcast_id);
						
						//continue flooding the req to update other nodes
						if(updateFlag==1) {
						
							printf("\nFlooding preq to allow updates in other nodes\n");
						
							req->RREP_sent=htonl(1);
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							bzero(frame, ETH_FRAME_LEN);
						
							//RREQ header
							frame->dst_mac[0]=0xff;		
							frame->dst_mac[1]=0xff;
							frame->dst_mac[2]=0xff;
							frame->dst_mac[3]=0xff;
							frame->dst_mac[4]=0xff;
							frame->dst_mac[5]=0xff;
							frame->protocol_no=htons(PROTOCOL_NOCOL_NO);
							frame->type=htonl(0);
						
							memcpy((void *) frame->payload, req, sizeof(*req));
					
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NOCOL_NO);
							send_if_addr.sll_halen=ETH_ALEN;
  							send_if_addr.sll_addr[0]=0xff;
  							send_if_addr.sll_addr[1]=0xff;
							send_if_addr.sll_addr[2]=0xff;
							send_if_addr.sll_addr[3]=0xff;
							send_if_addr.sll_addr[4]=0xff;
							send_if_addr.sll_addr[5]=0xff;
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
							
							//flooding (since have a more efficnent path)
							for(i = 0; i < if_cnt; i++) {
								//do not send to incoming interface					
								if(i == index){
									continue;
								}							
								memcpy(frame->src_mac, if_addr[i].sll_addr, ETH_ALEN);
							
								send_if_addr.sll_ifindex=i+3;

								if(sendto(if_fd[i], frame, sizeof(frame), 
										0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
										printf("cannot send preq\n");
										exit(1);
								}
							
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										node);
								printf("ODR message type 0, source ip %s, destination ip %s\n", 
										req->src_addr, req->dst_addr);
							}
						}
					}	
					//current node does not have route information to destination node
					else {
						printf("Current does not have route to destination\n");
						if(routing_table[ind].broadcast_id<ntohl(req->broadcast_id) 
							|| (routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1)) {
						
							printf("RREQ continues to flood\n");
							
							bzero(frame, ETH_FRAME_LEN);
						
							//RREQ header
							frame->dst_mac[0]=0xff;		
							frame->dst_mac[1]=0xff;
							frame->dst_mac[2]=0xff;
							frame->dst_mac[3]=0xff;
							frame->dst_mac[4]=0xff;
							frame->dst_mac[5]=0xff;
							frame->protocol_no=htons(PROTOCOL_NOCOL_NO);
							frame->type=htonl(0);
							
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							memcpy((void *) frame->payload), req, sizeof(*req));
						
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NOCOL_NO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_BROADCAST;
							send_if_addr.sll_halen=ETH_ALEN;
  							send_if_addr.sll_addr[0]=0xff;
  							send_if_addr.sll_addr[1]=0xff;
							send_if_addr.sll_addr[2]=0xff;
							send_if_addr.sll_addr[3]=0xff;
							send_if_addr.sll_addr[4]=0xff;
							send_if_addr.sll_addr[5]=0xff;
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
							
							//flooding, since no rotue was found
							for(i = 0; i < if_cnt; i++) {
						
								if(i == index) continue;
								
								memcpy(header->src_mac, if_addr[i].sll_addr, ETH_ALEN);
								memcpy((void *) ether_frame, header, sizeof(*header));
								
								send_if_addr.sll_ifindex = i+3;

								if(sendto(if_fd[i], ether_frame, sizeof(*header)+sizeof(*req), 
									0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
									printf("cannot send RReq\n");
									exit(1);
								}
								
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										node);
								printf("ODR message type 0, source ip %s, destination ip %s\n", req->src_addr, req->dst_addr);
							}
						}
						
						routing_table[ind].broadcast_id=ntohl(req->broadcast_id);
					}
						
					free(req);
				}
	/*------------- J -------------*/

	/*------------- SOU -------------*/
	else if(frame->type == 1) {	//RREP received
		printf("\n This is a RREP ethernet frame.\n\n");

		struct RREP *rep = (struct RREP *)malloc(sizeof(struct RREP));
		memcpy(rep, (void *)(frame->payload), sizeof(*rep));
		if(update_table(rep->src_addr, rep->flag, rep->hopcnt, frame, index) != 1)
			err_sys("Updating error!\n");

		if(rep->dst_addr != vms[cur-1]){	//this is not destination node
			printf("\nThis is not the destination node of RREP.\n\n");

			rep->hop_cnt += 1;

			//retransmit RREP
			if(sendRREP(frame, send_if_addr, rep) != 1)
				err_sys("Sending error!\n");

			free(rep);


		}else{	//this is destination node

			printf("\nThis is the destination node of RREP.\n\n");

			char msg[7] = "HELLO!";
			int port = 3000;
			if(sendLocal(port_cnt,local_addr, odrfd, msg, port, rep->src_addr, rep->dst_port) != 1)
				err_sys("Sending error!\n");

			printf("RREP has been sent to local client\n");

		}

	}else{	//app payload
		printf("\n This is a payload ethernet frame.\n\n");

		struct payload *pl = (struct payload *)malloc(sizeof(struct payload));
		memcpy(pl, (void *)(frame->payload), sizeof(*pl));

		if(pl->dst_addr != vms[cur-1]){	//This is not destination node
			printf("\nThis is not the destination node of app payload.\n\n");
			if(sendRREP(frame, send_if_addr, pl) != 1)
				err_sys("Sending error!\n");

			free(pl);
		}else{	//This is destination node
			printf("\nThis is the destination node of app payload.\n\n");

			if(sendLocal(port_cnt,local_addr, odrfd, pl->message, pl->src_port, pl->src_addr, pl->dst_port) != 1)
				err_sys("Sending error!\n");

			printf("APP payload has been sent to local client\n");
		}
	}

	free(frame);
}
}
}
	/*------------- SOU -------------*/
	exit(0);
}