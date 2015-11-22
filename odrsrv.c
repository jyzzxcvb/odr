#include "hw_addrs.h"
#include "API.h"
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
	unsigned char dst_mac[ETH_ALEN];
	unsigned char src_mac[ETH_ALEN];
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

int main(int argc, char **argv)
{
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

	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		
		printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		
		if ( (sa = hwa->ip_addr) != NULL)
			printf("         IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));


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

		addr.sll_family=PF_PACKET;
		addr.sll_protocol=htons(PROTOCOL_VALUE);
		addr.sll_ifindex=hwa->if_index;
		memcpy(addr.sll_addr, hwa->if_haddr, ETH_ALEN);
		
		//Create a PF_PACKET socket for every interface
		sockFD=Socket(PF_PACKET,SOCK_RAW,htons(PROTOCOL_VALUE));  //set the value in header file ?
		Bind(sockFD, (SA *) addr, sizeof(addr));
	}

	free_hwa_info(hwahead);

	/*------------- Obtain Interfaces Ends -------------*/

	/*------------- J -------------*/
			for(index=0; index<if_cnt; index++) {

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
				for (int i = 0; i < 6; ++i)
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
					for(int i=0; i<10; i++) {
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
							for (int i = 0; i < 6; ++i)
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
							send_if_addr.sll_protocol=htons(PROTOCOL_NO);
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
							for (int i = 0; i < 6; ++i)
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
					
					//check whether this node has route information to destination node
					for(count=0; count<10; count++) {

						if(strcmp(req->dst_addr, routing_table[count].dst_addr)==0) {
								
							if(routing_table[count].outgoing_if!=0 && ntohl(req->discovery)==0) break;
						}
					}
												
					//this node has route information to destination node
					if(count<10) {
						
						printf("This node has route information to destination\n");
						
						//reply is sent back
						if(ntohl(req->RREP_sent)==0 && (routing_table[ind].broadcast_id<ntohl(req->broadcast_id) 
							|| (routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1))) {
					
							struct RREP *reply=(struct RREP *) malloc(sizeof(struct RREP));
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
							
							//RREP header
							memcpy(header->dst_mac, recv_mac, ETH_ALEN);
							memcpy(header->src_mac, if_addr[routing_table[ind].outgoing_if-3].sll_addr, ETH_ALEN);
							header->proto=htons(PROTO);
							header->type=htonl(1);
							
							//RREP ODR protocal message
							strcpy(reply->src_addr, req->dst_addr);
							strcpy(reply->dst_addr, req->src_addr);
							reply->dst_port=htonl(req->src_port);
							reply->hop_cnt=htonl(routing_table[count].hop_cnt);
							reply->discovery=htonl(0);
							
							//RREP Ethernet frame
							memcpy((void *) ether_frame, (void *) header, sizeof(*header));
  							memcpy((void *) (ether_frame+sizeof(*header)), (void *) reply, sizeof(*reply));
  							printf("RREP is ready to be sent out to ");
  							ptr=header->dst_mac;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
  						
  							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_hatype=ARPHRD_ETHER;
							send_if_addr.sll_pkttype=PACKET_OTHERHOST;
							send_if_addr.sll_ifindex=index+3;	
							send_if_addr.sll_halen=ETH_ALEN;
  							memcpy(send_if_addr.sll_addr, header->dst_mac, ETH_ALEN);
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;	
  							
							if(sendto(if_fd[index], ether_frame, sizeof(*header)+sizeof(*reply), 
								0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
								
								perror("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", node, node);
							ptr=send_if_addr.sll_addr;
        					no=ETH_ALEN;
							do {
			
            					printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        					} while(--no>0);
										
							printf("	ODR message type 1, source ip %s, destination ip %s\n", reply->src_addr, reply->dst_addr);
							
							free(reply);
						}
						
						routing_table[ind].broadcast_id=ntohl(req->broadcast_id);
						
						//continue flooding the req
						if(updateFlag==1) {
						
							printf("\nThis node continued to flood RREQ after it sent RREP\n");
						
							req->RREP_sent=htonl(1);
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
						
							//RREQ header
							header->dst_mac[0]=0xff;		
							header->dst_mac[1]=0xff;
							header->dst_mac[2]=0xff;
							header->dst_mac[3]=0xff;
							header->dst_mac[4]=0xff;
							header->dst_mac[5]=0xff;
							header->proto=htons(PROTO);
							header->type=htonl(0);
						
							memcpy((void *) (ether_frame+sizeof(*header)), req, sizeof(*req));
					
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
							send_if_addr.sll_halen=ETH_ALEN;
  							send_if_addr.sll_addr[0]=0xff;
  							send_if_addr.sll_addr[1]=0xff;
							send_if_addr.sll_addr[2]=0xff;
							send_if_addr.sll_addr[3]=0xff;
							send_if_addr.sll_addr[4]=0xff;
							send_if_addr.sll_addr[5]=0xff;
  							send_if_addr.sll_addr[6]=0x00;
							send_if_addr.sll_addr[7]=0x00;
							
							//flood on all interfaces, except the one that RREQ came in
							for(count=0; count<if_cnt; count++) {
					
								if(count==index) continue;
							
								memcpy(header->src_mac, if_addr[count].sll_addr, ETH_ALEN);
								memcpy((void *) ether_frame, header, sizeof(*header));
							
								send_if_addr.sll_ifindex=count+3;

								if(sendto(if_fd[count], ether_frame, sizeof(*header)+sizeof(*req), 
										0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
										perror("ODR: Failed to send Ethernet frame through interface socket\n");
										exit(1);
								}
							
								printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
										node, node);
								printf("	ODR message type 0, source ip %s, destination ip %s\n", 
										req->src_addr, req->dst_addr);
							}
						}
					}	
					//this node is not destination node and doesn't have route information to destination node
					else {
					
						printf("This node is not the destination node for this RREQ and it doesn't have route to destination\n");
						
						if(routing_table[ind].broadcast_id<ntohl(req->broadcast_id) 
							|| (routing_table[ind].broadcast_id==ntohl(req->broadcast_id) && updateFlag==1)) {
						
							printf("RREQ continues to flood\n");
							
							bzero(ether_frame, ETH_FRAME_LEN);
							bzero(header, sizeof(struct hdr));
						
							//RREQ header
							header->dst_mac[0]=0xff;		
							header->dst_mac[1]=0xff;
							header->dst_mac[2]=0xff;
							header->dst_mac[3]=0xff;
							header->dst_mac[4]=0xff;
							header->dst_mac[5]=0xff;
							header->proto=htons(PROTO);
							header->type=htonl(0);
							
							//hop count plus one
							req->hop_cnt=htonl(ntohl(req->hop_cnt)+1);
							
							memcpy((void *) (ether_frame+sizeof(*header)), req, sizeof(*req));
						
							bzero(&send_if_addr, sizeof(struct sockaddr_ll));
							send_if_addr.sll_family=PF_PACKET;
							send_if_addr.sll_protocol=htons(PROTO);
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
							
							//flood on all interfaces, except the one that RREQ came in
							for(count=0; count<if_cnt; count++) {
						
								if(count==index) continue;
								
								memcpy(header->src_mac, if_addr[count].sll_addr, ETH_ALEN);
								memcpy((void *) ether_frame, header, sizeof(*header));
								
								send_if_addr.sll_ifindex=count+3;

								if(sendto(if_fd[count], ether_frame, sizeof(*header)+sizeof(*req), 
									0, (SA *) &send_if_addr, sizeof(send_if_addr))<0) {
									
									perror("ODR: Failed to send Ethernet frame through interface socket\n");
									exit(1);
								}
								
								printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
										node, node);
								printf("	ODR message type 0, source ip %s, destination ip %s\n", req->src_addr, req->dst_addr);
							}
						}
						
						routing_table[ind].broadcast_id=ntohl(req->broadcast_id);
					}
						
					free(req);
				}
	/*------------- J -------------*/


	exit(0)
}