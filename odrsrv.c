#include "a3.h"
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <time.h>

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

struct routing_entry {
	char dst_addr[ADDR_LEN];	
	unsigned char next_hop[6];	
	int outgoing_if;
	int hop_cnt;
	int broadcast_id;
	struct timeval timestamp;
};

struct port_entry {

	int port;
	char sun_path[15];
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
int broadcast_id =0;
int port=6000;
int update_table(char* src_addr, char* src_mac, int flag, int hop_count, int ind);
int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port);
int sendPacket(struct eth_frame *frame, struct sockaddr_ll *send_if_addr, void *packet, int type, char dst_addr[], char src_addr[]);

struct sockaddr_ll if_addr[3];
//local routing table
struct routing_entry routing_table[10];
//local port talbe
struct port_entry port_table[10];

int port_cnt=0;

struct sockaddr_ll interfaces[VMNUMBER];
int interfaceNumber=0;
int interfaceFDs[VMNUMBER];

int currentVM;
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
			strcpy(local_addr->sun_path,port_table[i].sun_path);   //debug 1
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
			strcpy(send_if_addr->sll_addr,frame->dst_mac); //debug 2
			send_if_addr->sll_ifindex=temp_index;
			break;
		}
	}

	memcpy((void *)(frame->payload), (void *)packet, sizeof(*packet));
	temp_index = routing_table[n].outgoing_if-3;
	if(sendto(interfaceFDs[temp_index], frame, sizeof(*frame), 0, (SA *) send_if_addr, sizeof(*send_if_addr))<0) //debug 3
		err_sys("Sending error!\n");
	else
		printf("ODR at node vm%d: %s, sending frame, src:vm%d dest:%s\n\n", currentVM, src_addr, currentVM, dst_addr); //debug 4

	return 0;
}

int update_table(char * src_addr, char* src_mac, int flag, int hop_cnt, int ind){
	//add or update 'forward' path to destination node in routing table
	int i;
	for(i = 0; i < 10; i++) {
		if(strcmp(routing_table[i].dst_addr, src_addr) == 0) {
			//new route to destination is more efficient
			if(routing_table[i].outgoing_if==0
				|| (routing_table[i].hop_cnt==ntohl(hop_cnt)+1
					&& routing_table[i].outgoing_if!=ind+3)
				|| routing_table[i].hop_cnt>ntohl(hop_cnt)+1 || ntohl(flag ) == 1) {
				
				memcpy(routing_table[i].next_hop, src_mac, 6);
				routing_table[i].outgoing_if=ind+3;
				routing_table[i].hop_cnt=ntohl(hop_cnt)+1;

				printf("This RREP gave a better route forward to destination node VM%d with hop count %d\n",i+1, routing_table[i].hop_cnt);
				printf("Local routing table was updated\n\n");					
			}
			break;
		}			
	}
	return 1;
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

	printf("\n");
	char eth0IP[SIZE];
	
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
        strcpy(eth0IP,Sock_ntop_host( (struct sockaddr*)hwa->ip_addr, sizeof(struct sockaddr* ) )); 
        if (strcmp(hwa->if_name, "eth0") == 0 ){
        	for (i=0;i<VMNUMBER;i++)
        	{
        		if (strcmp(vms[i],eth0IP)==0){
        			currentVM=i+1;
        			printf("Current VM node is %d, the VM IP is %s\n",currentVM,eth0IP);
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
		interfaces[interfaceNumber].sll_protocol=htons(PROTOCOL_NO);
		interfaces[interfaceNumber].sll_ifindex=hwa->if_index;
		memcpy(interfaces[interfaceNumber].sll_addr, hwa->if_haddr, ETH_ALEN);
		
		//Create a PF_PACKET socket for every interface
		interfaceFDs[interfaceNumber]=Socket(PF_PACKET,SOCK_RAW,htons(PROTOCOL_NO));  //set the value in header file ?
		Bind(interfaceFDs[interfaceNumber], (SA *) &interfaces[interfaceNumber], sizeof(SA*));
		FD_SET(interfaceFDs[interfaceNumber], &allset);
		maxfd=interfaceFDs[interfaceNumber]>maxfd?interfaceFDs[interfaceNumber]:maxfd;
		
		interfaceNumber++;
	}

	free_hwa_info(hwahead);
    
	printf("%d sockets are binded for all of the interfaces\n",interfaceNumber);

	/*------------- Obtain Interfaces Ends -------------*/




	/*------------- Create UNIX DOMAIN Socket Starts-------------*/

	int sockfd;
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


   

	//initialize each destination node address in routing table
	for(i=0 ; i<VMNUMBER; i++) {
	
		strcpy(routing_table[i].dst_addr, vms[i]);
		routing_table[i].outgoing_if=0;
	}
	printf("ODR local routing table was initialized\n");

	//put server well-known port number in port table	
	port_table[0].port=SERV_PORT_NO;
	strcpy(port_table[0].sun_path, SERV_PATH);



	/*------------- Create UNIX DOMAIN Socket ENDS-------------*/


	/*------------- Server Starts -------------*/
	int status;
	char buf[BUF_SIZE];
	char message[BUF_SIZE];
	struct sockaddr_un addr;
	socklen_t addrlen;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	maxfd++;
	char dst_addr[ADDR_LEN];
	int dst_port;
	int no;
	int j;
	int route_exist;
	int flag;
	struct sockaddr_ll send_if_addr;
	unsigned char recv_mac[ETH_ALEN];
							
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
    		Recvfrom(sockfd,buf,BUF_SIZE,0,(SA *) &addr, &addrlen);
    		printf("received new message from local: %s , with path name: %s\n",buf,addr.sun_path);
    		sscanf(buf, "%s %s %d", message,dst_addr, &dst_port);

    			for(j=0; j<=port_cnt; j++) {

				//client port number already in port table				
				if(strcmp(port_table[j].sun_path, addr.sun_path)==0) break;
			}



    		if (j==0)
    		{ 	//local server
    			for (i=0;i<VMNUMBER;i++)
    			{
    				if (strcmp(routing_table[i].dst_addr,dst_addr)==0)
    					break;
    			}
    			struct eth_frame *frame=(struct eth_frame *) malloc(sizeof(struct eth_frame));				
				struct payload *load=(struct payload *) malloc(sizeof(struct payload));
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
				//application payload header
				memcpy(frame->dst_mac, routing_table[i].next_hop, ETH_ALEN);
				memcpy(frame->src_mac, if_addr[routing_table[i].outgoing_if-3].sll_addr, ETH_ALEN);
				frame->protocol_no=htons(PROTOCOL_NO);
				frame->type=htonl(2);

				//application payload ODR protocol message
				strcpy(load->src_addr, eth0IP);
				strcpy(load->dst_addr, dst_addr);
				load->src_port=htonl(5000); //?? 
				load->dst_port=htonl(dst_port);
				load->hop_cnt=htonl(0);
				load->len=htonl(sizeof(message));
				strcpy(load->message, message);
				
				//application payload Ethernet frame
				
				memcpy((void *) (frame->payload), (void *) load, sizeof(*load));
				printf("Application payload is ready to be sent out to ");
				ptr=frame->dst_mac;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);

				bzero(&send_if_addr, sizeof(struct sockaddr_ll));
				send_if_addr.sll_family=PF_PACKET;
				send_if_addr.sll_protocol=htons(PROTOCOL_NO);
				send_if_addr.sll_ifindex=routing_table[i].outgoing_if;
				send_if_addr.sll_halen=ETH_ALEN;
				memcpy(send_if_addr.sll_addr, routing_table[i].next_hop, ETH_ALEN);
				send_if_addr.sll_addr[6]=0x00;
				send_if_addr.sll_addr[7]=0x00;

				//application payload is sent
				if(sendto(interfaceFDs[routing_table[i].outgoing_if-3], frame, sizeof(*frame), 0, 
						(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

					perror("ODR: Failed to send Ethernet frame throught interface socket\n");
					exit(1);
				}

				printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
				ptr=send_if_addr.sll_addr;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);
				
				printf("	ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);

				free(load);
				free(frame);
									

    		}
    		else
    		{	//local client

    		printf("Received message is local client request\n\n");
			
				//client port number not in port talbe			
				if(j==port_cnt+1) {

					port_table[++port_cnt].port=port++;
					strcpy(port_table[port_cnt].sun_path, addr.sun_path);
				}
	
				//check whether route discovery is needed		
				route_exist=0;
				for(i=0; i<10; i++) {

					if(strcmp(routing_table[i].dst_addr, dst_addr)==0) {

						if(routing_table[i].outgoing_if!=0) route_exist=1; //???flag????
						break;
					}
				}
			
				//route discovery is not needed
				//application payload is sent
				if(route_exist==1) {
			
					printf("Routing information is available for client request\n");

					struct eth_frame *frame=(struct eth_frame *) malloc(sizeof(struct eth_frame));				
					struct payload *load=(struct payload *) malloc(sizeof(struct payload));
				
					
					//application payload header
					memcpy(frame->dst_mac, routing_table[i].next_hop, ETH_ALEN);
					memcpy(frame->src_mac, if_addr[routing_table[i].outgoing_if-3].sll_addr, ETH_ALEN);
					frame->protocol_no=htons(PROTOCOL_NO);
					frame->type=htonl(2);

					//application payload ODR protocol message
					strcpy(load->src_addr, vms[currentVM-1]);
					strcpy(load->dst_addr, dst_addr);
					load->src_port=htonl(port_table[j].port);
					load->dst_port=htonl(SERV_PORT_NO);
					load->hop_cnt=htonl(0);
					load->len=htonl(sizeof(message));
					strcpy(load->message, message);
				
					//application payload Ethernet frame
					
					memcpy((void *) (frame->payload), (void *) load, sizeof(*load));
					printf("Application payload is ready to be sent out to ");
					ptr=frame->dst_mac;
        			no=ETH_ALEN;
					do {
			
            			printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        			} while(--no>0);

					bzero(&send_if_addr, sizeof(struct sockaddr_ll));
					send_if_addr.sll_family=PF_PACKET;
					send_if_addr.sll_protocol=htons(PROTOCOL_NO);
					send_if_addr.sll_ifindex=routing_table[i].outgoing_if;
					send_if_addr.sll_halen=ETH_ALEN;
					memcpy(send_if_addr.sll_addr, routing_table[i].next_hop, ETH_ALEN);
					send_if_addr.sll_addr[6]=0x00;
					send_if_addr.sll_addr[7]=0x00;

					//application payload is sent
					if(sendto(interfaceFDs[routing_table[i].outgoing_if-3], frame, 
							sizeof(*frame), 0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {

						perror("ODR: Failed to send Ethernet frame throught interface socket\n");
						exit(1);
					}

					printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
					ptr=send_if_addr.sll_addr;
        			no=ETH_ALEN;
					do {
			
            			printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        			} while(--no>0);
					
					printf("	ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);

					
					free(load);
					free(frame);
				}
				//route discovery is needed
				//RREQ is sent
				else {
			
					printf("Routing information is not available for client request\n");

					struct eth_frame *frame=(struct eth_frame *) malloc(sizeof(struct eth_frame));						
					struct RREQ *RRQ=(struct RREQ *) malloc(sizeof(struct RREQ));
			
				
					//RREQ header
					frame->dst_mac[0]=0xff;		
					frame->dst_mac[1]=0xff;
					frame->dst_mac[2]=0xff;
					frame->dst_mac[3]=0xff;
					frame->dst_mac[4]=0xff;
					frame->dst_mac[5]=0xff;
					frame->protocol_no=htons(PROTOCOL_NO);
					frame->type=htonl(0);

					//RREQ ODR protocol message
					strcpy(RRQ->src_addr, vms[currentVM-1]);
					strcpy(RRQ->dst_addr, dst_addr);
					RRQ->src_port=htonl(port_table[j].port);
					RRQ->broadcast_id=htonl(++broadcast_id);
					RRQ->hop_cnt=htonl(0);
					RRQ->RREP_flag=htonl(0);
					RRQ->flag=htonl(flag==1?1:0); //??flag ??
					printf("RREQ is ready to be sent out to ff:ff:ff:ff:ff:ff\n");

					bzero(&send_if_addr, sizeof(struct sockaddr_ll));
					send_if_addr.sll_family=PF_PACKET;
					send_if_addr.sll_protocol=htons(htonl(2));
					send_if_addr.sll_halen=ETH_ALEN;
					send_if_addr.sll_addr[0]=0xff;		
					send_if_addr.sll_addr[1]=0xff;		
					send_if_addr.sll_addr[2]=0xff;
					send_if_addr.sll_addr[3]=0xff;
					send_if_addr.sll_addr[4]=0xff;
					send_if_addr.sll_addr[5]=0xff;
					send_if_addr.sll_addr[6]=0x00;
					send_if_addr.sll_addr[7]=0x00;

					//RREQ floods on all interface socket
					for(i=0; i<interfaceNumber; i++) {

						memcpy(frame->src_mac, if_addr[i].sll_addr, ETH_ALEN);
					
					
  						memcpy((void *) (frame->payload), (void *) RRQ, sizeof(*RRQ));
  					
  						send_if_addr.sll_ifindex=i+3;

						if(sendto(interfaceFDs[i], frame, sizeof(*frame), 0, 
								(SA *) &send_if_addr, sizeof(send_if_addr))<0) {

							perror("ODR: Failed to send Ethernet frame through interface socket\n");
							exit(1);
						}
						printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
									currentVM, currentVM);
						printf("	ODR message type 0, source ip %s, destination ip %s\n", RRQ->src_addr, RRQ->dst_addr);
					}

					//free(header);
					free(RRQ);
					free(frame);
				}
    		}

    	}	
    
	
    	int index;

	/*------------- J -------------*/
	for(index=0; index<interfaceNumber; index++) {
			int i;
			//RRQ, RRP, or payload received from one of the interface socket
			if(FD_ISSET(interfaceFDs[index], &rset)) {
			
				printf("New Ethernet frame was received from interface %d\n", index+3);
				struct eth_frame *frame=(struct eth_frame *) malloc(sizeof(struct eth_frame));	
				

				if(recvfrom(interfaceFDs[index], frame, sizeof(*frame), 0, NULL, NULL)<0) {

					perror("ODR: Failed to receive Ethernet frame througth interface socket\n");
					exit(1);
				}

				memcpy(recv_mac, frame->src_mac, 6);
				printf("receive frame of type %d from ", ntohl(frame->protocol_no));
				//print mac address
				ptr=recv_mac;
				for (i = 0; i < 6; ++i)
				{
					printf("%02x", *ptr++ & 0xff);  //debgu
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
					memcpy((void *) req, (void *) (frame->payload), sizeof(*req));
					
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
					if(strcmp(req->dst_addr, vms[currentVM-1])==0) {
					
						printf("This node is the destination node for this RREQ\n");
					
						//reply have not been sent back
						//debug
						if((ntohl(req->RREP_flag)==0 && (routing_table[ind].broadcast_id < ntohl(req->broadcast_id)))  //debug no broadcast_id in routing entry
							|| (routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1)) {
						//send rrep

							struct RREP *reply=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(frame, sizeof(struct eth_frame));
							
							//RREP frame
							memcpy(frame->dst_mac, recv_mac, 6);
							memcpy(frame->src_mac, if_addr[routing_table[ind].outgoing_if-3].sll_addr, ETH_ALEN);
							frame->protocol_no=htons(PROTOCOL_NO);
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
								printf("%02x", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NO);
							send_if_addr.sll_ifindex=index+3;
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, frame->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
  							
							if(sendto(interfaceFDs[index], frame, sizeof(frame), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								printf("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
							ptr=send_if_addr.sll_addr;
							for (i = 0; i < 6; ++i)
							{
								printf("%02x", *ptr++ & 0xff);
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
							if(routing_table[i].outgoing_if!=0 && ntohl(req->flag)==0) {
								routeFound = 1;  
								break;
							}
						}
					}
												
					//has route to destination
					if(routeFound) {
						printf("Route to destination found\n");
						
						//reply is sent back
						if((ntohl(req->RREP_flag)==0 && (routing_table[ind].broadcast_id<ntohl(req->broadcast_id) ))
							|| ((routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1))) {
					
							struct RREP *reply=(struct RREP *) malloc(sizeof(struct RREP));
						
							bzero(frame, sizeof(struct eth_frame));
							
							memcpy(frame->dst_mac, recv_mac, ETH_ALEN);
							memcpy(frame->src_mac, if_addr[routing_table[ind].outgoing_if-3].sll_addr, ETH_ALEN);
							frame->protocol_no=htons(PROTOCOL_NO);
							frame->type=htonl(1);
							
							//prepare RREP
							strcpy(reply->src_addr, req->dst_addr);
							strcpy(reply->dst_addr, req->src_addr);
							reply->dst_port=htonl(req->src_port);
							reply->hop_cnt=htonl(routing_table[i].hop_cnt);
							reply->flag=htonl(0);
							
							//RREP Ethernet frame
  							memcpy((void *) frame->payload, (void *) reply, sizeof(*reply));
  							ptr=frame->dst_mac;
        					for (i = 0; i < 6; ++i)
							{
								printf("%02x", *ptr++ & 0xff);
								if (i < 5){
									printf(":");
								}
								else{
									printf("\n");
								}
							}
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_OTHERHOST;
							send_if_addr.sll_ifindex=index+3;	
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, frame->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;	
  							
							if(sendto(interfaceFDs[index], frame, sizeof(frame), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								printf("cannot send prep back\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent frame, source VM%d, destination ", currentVM, currentVM);
							ptr=send_if_addr.sll_addr;
        					for (i = 0; i < 6; ++i)
							{
								printf("%02x", *ptr++ & 0xff);
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
						
							req->RREP_flag=htonl(1);
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							bzero(frame, sizeof(struct eth_frame));
						
							//RREQ header
							frame->dst_mac[0]=0xff;		
							frame->dst_mac[1]=0xff;
							frame->dst_mac[2]=0xff;
							frame->dst_mac[3]=0xff;
							frame->dst_mac[4]=0xff;
							frame->dst_mac[5]=0xff;
							frame->protocol_no=htons(PROTOCOL_NO);
							frame->type=htonl(0);
						
							memcpy((void *) frame->payload, req, sizeof(*req));
					
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NO);
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
							for(i = 0; i < interfaceNumber; i++) {
								//do not send to incoming interface					
								if(i == index){
									continue;
								}							
								memcpy(frame->src_mac, if_addr[i].sll_addr, ETH_ALEN);
							
								send_if_addr.sll_ifindex=i+3;

								if(sendto(interfaceFDs[i], frame, sizeof(frame), 
										0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
										printf("cannot send preq\n");
										exit(1);
								}
							
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										currentVM);
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
							
							bzero(frame, sizeof(struct eth_frame));
						
							//RREQ header
							frame->dst_mac[0]=0xff;		
							frame->dst_mac[1]=0xff;
							frame->dst_mac[2]=0xff;
							frame->dst_mac[3]=0xff;
							frame->dst_mac[4]=0xff;
							frame->dst_mac[5]=0xff;
							frame->protocol_no=htons(PROTOCOL_NO);
							frame->type=htonl(0);
							
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							memcpy((void *) (frame->payload), req, sizeof(*req));
						
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTOCOL_NO);
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
							for(i = 0; i < interfaceNumber; i++) {
						
								if(i == index) continue;
								
								memcpy(frame->src_mac, if_addr[i].sll_addr, ETH_ALEN);
								
								//debug 
								send_if_addr.sll_ifindex = i+3;

								if(sendto(interfaceFDs[i], frame, sizeof(*frame), 
									0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
									printf("cannot send RReq\n");
									exit(1);
								}
								
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										currentVM);
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
		if(update_table(rep->src_addr,frame->src_mac ,rep->flag, rep->hop_cnt, index) != 1)
			err_sys("Updating error!\n");

		if(rep->dst_addr != vms[currentVM-1]){	//this is not destination node
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
			if(sendLocal(port_cnt,&addr, sockfd, msg, port, rep->src_addr, rep->dst_port) != 1)
				err_sys("Sending error!\n");

			printf("RREP has been sent to local client\n");

		}

	}else{	//app payload
		printf("\n This is a payload ethernet frame.\n\n");

		struct payload *pl = (struct payload *)malloc(sizeof(struct payload));
		memcpy(pl, (void *)(frame->payload), sizeof(*pl));

		if(pl->dst_addr != vms[currentVM-1]){	//This is not destination node
			printf("\nThis is not the destination node of app payload.\n\n");
			if(sendRREP(frame, send_if_addr, pl) != 1)
				err_sys("Sending error!\n");

			free(pl);
		}else{	//This is destination node
			printf("\nThis is the destination node of app payload.\n\n");

			if(sendLocal(port_cnt,&addr, sockfd, pl->message, pl->src_port, pl->src_addr, pl->dst_port) != 1)
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