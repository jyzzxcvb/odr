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
// struct hdr {

// 	unsigned char dst_mac[ETH_ALEN];
// 	unsigned char src_mac[ETH_ALEN];
// 	unsigned short proto;
// 	int type;
// };


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

struct frame {

	unsigned char dst_mac[ETH_ALEN];
	unsigned char src_mac[ETH_ALEN];
	unsigned short protocol_no;
	int type;
	char src_addr[ADDR_LEN];
	char dst_addr[ADDR_LEN];
	int hop_cnt;
	int flag;
	int src_port;      
	int broadcast_id;
	int RREP_flag;
	int dst_port;

	int len;
	char message[15];
};


// //Ethernet frame
// struct eth_frame {
// 	int type;
// 	unsigned char dst_mac[6];
// 	unsigned char src_mac[6];
// 	unsigned short protocol_no;
// 	void * payload;
// };
struct frame1{
	int type;
	unsigned char dst_mac[6];
	unsigned char src_mac[6];
	unsigned short protocol_no;
	char src_addr[ADDR_LEN];
	char dst_addr[ADDR_LEN];
	int hop_cnt;
	int flag;
	
	int src_port;      
	int broadcast_id;
	int RREP_flag;
	
	int dst_port;

	int len;
	char message[30];

};
struct RREQ {

	char src_addr[15];
	char dst_addr[15];
	int src_port;
	int broadcast_id;
	int hop_cnt;
	int RREP_flag;
 	int flag;
};

// struct RREQ_frame {
// 	int type;
// 	unsigned char dst_mac[6];
// 	unsigned char src_mac[6];
// 	unsigned short protocol_no;
// 	char src_addr[ADDR_LEN];
// 	char dst_addr[ADDR_LEN];
// 	int src_port;
// 	int broadcast_id;
// 	int hop_cnt;
// 	int RREP_flag;
// 	int flag;
// };

// struct RREP_frame {
// 	int type;
// 	unsigned char dst_mac[6];
// 	unsigned char src_mac[6];
// 	unsigned short protocol_no;
// 	char src_addr[ADDR_LEN];
// 	char dst_addr[ADDR_LEN];
// 	int dst_port;
// 	int hop_cnt;
// 	int flag;
// };

// struct payload_frame {
// 	int type;
// 	unsigned char dst_mac[6];
// 	unsigned char src_mac[6];
// 	unsigned short protocol_no;
// 	char src_addr[ADDR_LEN];
// 	char dst_addr[ADDR_LEN];
// 	int src_port;
// 	int dst_port;
// 	int hop_cnt;
// 	int len;
// 	char message[30];
// };
int broadcast_id =0;
int port=6000;
int update_table(char* src_addr, char* src_mac, int flag, int hop_count, int ind);
int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port);
int sendPacket(struct frame*frame, struct sockaddr_ll *sendingaddr,int type);

//local routing table
struct routing_entry routing_table[10];u
//local port talbe
struct port_entry port_table[10];

int port_cnt=0;

struct sockaddr_ll interfaces[VMNUMBER];
int interfaceNumber=0;
int interfaceFDs[VMNUMBER];

int currentVM;

int printInfo(char * addr){
	char * ptr;
	ptr = addr;
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
	return 1;
}

int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port){
	int length;
	int frame_len = ETH_FRAME_LEN;
	char buf[frame_len];

	if((length = sprintf(buf, "%s %s %d", msg, src_addr, port)) < 0){ 
		printf("Format error\n");
		return -1;
	}

	int i;
	printf("%s %s %d\n", msg, src_addr, dst_port);
	dst_port = ntohl(dst_port);
	printf("again %s %s %d\n", msg, src_addr, dst_port);
	for(i = 0; i <= port_cnt; i++) {		
	printf("index: %d port %d address %s\n", i,port_table[i].port, port_table[i].sun_path);			
		if(port_table[i].port == (dst_port)) {
			strcpy(local_addr->sun_path,port_table[i].sun_path);   //debug 1
			break;
		}
	}			
	printf("%s\n", local_addr->sun_path);			
				
	if(sendto(odrfd, buf, length, 0, (struct sockaddr *) local_addr, sizeof(*local_addr))<0){
		printf("Cannot connect local client/server\n");
		return -1;
	}

	return 0;
}

int send_payload(int index, int po, char* buf){
	struct sockaddr_ll sendingaddr;
	struct frame *load=(struct frame *) malloc(sizeof(struct frame));
	void *ether_frame=(void *) malloc(ETH_FRAME_LEN);

	char * ptr;
	int no;
	//application payload header
	memcpy(load->dst_mac, routing_table[index].next_hop, ETH_ALEN);
	memcpy(load->src_mac, interfaces[routing_table[index].outgoing_if-3].sll_addr, ETH_ALEN);
	load->protocol_no=htons(PROTOCOL_NO);
	load->type=htonl(2);

	//application payload ODR protocol message
	strcpy(load->src_addr, vms[currentVM-1]);
	strcpy(load->dst_addr, routing_table[index].dst_addr);
	load->src_port=htonl(port_table[po].port);
	load->dst_port=htonl(SERV_PORT_NO);
	load->hop_cnt=htonl(0);
	load->len=htonl(sizeof(buf));
	strcpy(load->message, buf);

	//application payload Ethernet frame
	
	memcpy((void *) ether_frame, (void *) load, sizeof(*load));
	printf("Application payload is ready to be sent out to ");
	ptr=load->dst_mac;
	no=ETH_ALEN;
	do {

		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
	} while(--no>0);
	if(printInfo(load->dst_mac))
		err_sys("Printing error!\n");

	bzero(&sendingaddr, sizeof(struct sockaddr_ll));
	sendingaddr.sll_family=PF_PACKET;
	sendingaddr.sll_protocol=htons(PROTOCOL_NO);
	sendingaddr.sll_ifindex=routing_table[index].outgoing_if;
	sendingaddr.sll_halen=ETH_ALEN;
	memcpy(sendingaddr.sll_addr, routing_table[index].next_hop, ETH_ALEN);
	sendingaddr.sll_addr[6]=0x00;
	sendingaddr.sll_addr[7]=0x00;

	//application payload is sent
	printf("index is %d\n", index);
	if(sendto(interfaceFDs[routing_table[index].outgoing_if-3], ether_frame, 
			sizeof(*load), 0, (struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {

		perror("ODR: Failed to send Ethernet frame throught interface socket\n");
		return 1;;
	}

	printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
	ptr=sendingaddr.sll_addr;
	no=ETH_ALEN;
	do {

		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
	} while(--no>0);
	
	printf("ODR message type 2, source ip %s, destination ip %s\n", load->src_addr, load->dst_addr);
	return 0;
}

int sendPacket(struct frame *frame, struct sockaddr_ll *sendingaddr,int type){
	//prepare packet for retransmit

	int temp_index = 0;
	//bzero(frame, sizeof(struct eth_frame));
	void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
	
	frame->protocol_no = PROTOCOL_NO;
	frame->type = type;
	int n;
	for(n = 0; n < 10; n++) {
		if(frame->dst_addr == routing_table[n].dst_addr) {
			memcpy(frame->dst_mac, routing_table[n].next_hop, ETH_ALEN);
			temp_index = routing_table[n].outgoing_if-3;
			memcpy(frame->src_mac, interfaces[temp_index].sll_addr, ETH_ALEN);
			break;
		}
	}

	bzero(sendingaddr, sizeof(struct sockaddr_ll));

	sendingaddr->sll_family = PF_PACKET;
	sendingaddr->sll_protocol = PROTOCOL_NO;
	sendingaddr->sll_halen = ETH_ALEN;
	sendingaddr->sll_addr[6] = 0x00;	//not used
	sendingaddr->sll_addr[7] = 0x00;	//not used

	if(frame->type == 2){
		//app payload
		sendingaddr->sll_hatype=ARPHRD_ETHER;
		sendingaddr->sll_pkttype=PACKET_OTHERHOST;
		printf("\nRREP has been prepared for sending.\n\n");
	}else if(frame->type == 1)
	{
		//RREQ
		printf("\nAPP payload has been prepared for sending.\n\n");
	}else if(frame->type == 0){
		frame->dst_mac[0] = 0xff;
		frame->dst_mac[1] = 0xff;
		frame->dst_mac[2] = 0xff;
		frame->dst_mac[3] = 0xff;
		frame->dst_mac[4] = 0xff;
		frame->dst_mac[5] = 0xff;

		sendingaddr->sll_addr[0]=0xff;		
		sendingaddr->sll_addr[1]=0xff;		
		sendingaddr->sll_addr[2]=0xff;
		sendingaddr->sll_addr[3]=0xff;
		sendingaddr->sll_addr[4]=0xff;
		sendingaddr->sll_addr[5]=0xff;

		//RREQ floods
		return 0;

	}else{
		err_sys("Type error!\n");
		return 1;
	}

	for(n=0; n<10; n++){
		if(frame->dst_addr == routing_table[n].dst_addr) {
			temp_index = routing_table[n].outgoing_if;
			strcpy(sendingaddr->sll_addr,frame->dst_mac); //debug 2
			sendingaddr->sll_ifindex=temp_index;
			break;
		}
	}

	//memcpy((void *)(frame->payload), (void *)packet, sizeof(*packet));
	memcpy((void *) ether_frame, (void *) frame, sizeof(*frame));
	temp_index = routing_table[n].outgoing_if-3;
	if(sendto(interfaceFDs[temp_index], ether_frame, sizeof(*frame), 0, (struct sockaddr *) sendingaddr, sizeof(*sendingaddr))<0) //debug 3
		err_sys("Sending error!\n");
	else
		printf("ODR at node vm%d: %s, sending frame, src:vm%d dest:%s\n\n", currentVM, frame->src_addr, currentVM, frame->dst_addr); //debug 4

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

void obtainIFs()
{
	struct hwa_info	*hwa, *hwahead;
	struct sockaddr	*sa;
	char   *ptr; 
	int    i, prflag;
	char eth0IP[SIZE];
	struct sockaddr_ll tempif;
	for (hwahead = hwa = Get_hw_addrs(); hwa != NULL; hwa = hwa->hwa_next) {
		
		printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
		if((sa=hwa->ip_addr)!=NULL) printf("    IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));
	
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
		
		//save interface information


		if (strcmp(hwa->if_name, "lo") == 0) 
			continue;
		//determine which VM node 
        strcpy(eth0IP,Sock_ntop_host( (struct sockaddr*)hwa->ip_addr, sizeof(struct sockaddr* ) )); 
        if (strcmp(hwa->if_name, "eth0") == 0 ){
        	i =0;
        	while (i<10 && strcmp(vms[i],eth0IP)!=0 )
          		i++;
        	
        	currentVM=i+1;
        	printf("Current VM node is %d, the VM IP is %s\n",currentVM,eth0IP);
        	continue;
        }


		bzero(&tempif, sizeof(struct sockaddr_ll));
		tempif.sll_family=PF_PACKET;
		tempif.sll_protocol=htons(PROTOCOL_NO);
		tempif.sll_ifindex=hwa->if_index;
		memcpy(tempif.sll_addr, hwa->if_haddr, ETH_ALEN);

		interfaces[interfaceNumber]=tempif;
		interfaceNumber++;
	}

	free_hwa_info(hwahead);
    

	

}

void InitRoutingTable()
{	
	int i;
	for(i=0 ; i<VMNUMBER; i++) {
	
		strcpy(routing_table[i].dst_addr, vms[i]);
		routing_table[i].outgoing_if=0;
	}
	printf("routing table was initialized\n");
}

void InitPortTable()
{
	port_table[0].port=SERV_PORT_NO;
	strcpy(port_table[0].sun_path, SERV_PATH);
}

int createLocalFD(){
	int sockfd;
	socklen_t len;
	struct sockaddr_un addr;
	
	sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0); 
	unlink(ODR_PATH); 
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, ODR_PATH, sizeof(addr.sun_path) - 1); 
	Bind(sockfd, (struct sockaddr *) &addr, SUN_LEN(&addr));
	printf("ODR Socket was binded with path name: %s\n",ODR_PATH);
	return sockfd;
}

struct frame* createFrame(int number,char* src_addr,char * dst_addr,int src_port,int dst_port,
	int type,char message[BUF_SIZE-1],int rrep_flag,int flag ,unsigned char dst_mac[6],unsigned char src_mac[6],int hop_cnt,int broadcast_id){
	struct frame *frame=(struct frame *) malloc(sizeof(struct frame));

	if (type==2) {
	memcpy(frame->dst_mac, routing_table[number].next_hop, ETH_ALEN);
	memcpy(frame->src_mac, interfaces[routing_table[number].outgoing_if-3].sll_addr, ETH_ALEN);
	frame->protocol_no=htons(PROTOCOL_NO);
	frame->type=htonl(type);

	//application payframe ODR protocol message
	strcpy(frame->src_addr, src_addr);
	strcpy(frame->dst_addr, dst_addr);
	frame->src_port=htonl(5000); //?? 
	frame->dst_port=htonl(dst_port);
	frame->hop_cnt=htonl(hop_cnt);
	frame->len=htonl(sizeof(message));
	strcpy(frame->message, message);
	}
	else
	if (type==0)
	{

		frame->dst_mac[0]=0xff;		
		frame->dst_mac[1]=0xff;
		frame->dst_mac[2]=0xff;
		frame->dst_mac[3]=0xff;
		frame->dst_mac[4]=0xff;
		frame->dst_mac[5]=0xff;
		frame->protocol_no=htons(PROTOCOL_NO);
		frame->type=htonl(type);

		//RREQ ODR protocol message
		strcpy(frame->src_addr,src_addr);
		strcpy(frame->dst_addr, dst_addr);
		frame->src_port=htonl(src_port);
		frame->broadcast_id=broadcast_id;
		frame->hop_cnt=htonl(hop_cnt);
		frame->RREP_flag=htonl(rrep_flag);
		frame->flag=htonl(flag==1?1:0); //??flag ??


	}
	else
		if (type==1)
	{
		memcpy(frame->dst_mac, dst_mac, 6);
							memcpy(frame->src_mac, src_mac, ETH_ALEN);
							frame->protocol_no=htons(PROTOCOL_NO);
							frame->type=htonl(type);
							
							//RREP ODR protocal message
							strcpy(frame->src_addr, src_addr);
							strcpy(frame->dst_addr, dst_addr);
							frame->dst_port=htonl(dst_port);
							frame->hop_cnt=htonl(hop_cnt);
							frame->flag=htonl(0);
							if (ntohl(flag)==1)
							{
								frame->flag=htonl(1);
							}

	}

	return frame;
}

void setSentAddr(int type,int number,struct sockaddr_ll *addr){
	bzero(addr, sizeof(struct sockaddr_ll));
	addr->sll_halen=ETH_ALEN;
	addr->sll_family=PF_PACKET;
	addr->sll_protocol=htons(PROTOCOL_NO);

	if (type==2){
	memcpy(addr->sll_addr, routing_table[number].next_hop, ETH_ALEN);
	addr->sll_addr[6]=0x00;
	addr->sll_addr[7]=0x00;
	addr->sll_ifindex=routing_table[number].outgoing_if;
	}
	else
	if (type==0)
	{
		addr->sll_addr[0]=0xff;		
					addr->sll_addr[1]=0xff;		
					addr->sll_addr[2]=0xff;
					addr->sll_addr[3]=0xff;
					addr->sll_addr[4]=0xff;
					addr->sll_addr[5]=0xff;
					addr->sll_addr[6]=0x00;
					addr->sll_addr[7]=0x00;
	}
	else
		if (type==1)
		{
			addr->sll_ifindex=number+3;
			addr->sll_addr[6]=0x00;
			addr->sll_addr[7]=0x00;
  							
		}
	
}


int main(int argc, char **argv){
	int staleness_time;
	int request_hold = 0;

	
	if (argc<2)
		err_sys("Usage: %s [staleness time]\n",argv[0]);
	if (staleness_time=atoi(argv[1])==0)
		err_sys("invalid staleness time!\n");
	

	fd_set allset,rset;
	int maxfd;

	FD_ZERO(&rset);
  	FD_ZERO(&allset);
  	maxfd=0;
	obtainIFs();
	int i,j;
	for (i=0;i<interfaceNumber;i++)
	{
		interfaceFDs[i]=Socket(PF_PACKET,SOCK_RAW,htons(PROTOCOL_NO));  //set the value in header file ?
		printf("%d\n", interfaceFDs[i]);
		Bind(interfaceFDs[i], (struct sockaddr *) &interfaces[i], sizeof(struct sockaddr_ll));
		FD_SET(interfaceFDs[i], &allset);
		maxfd=interfaceFDs[i]>maxfd?interfaceFDs[i]:maxfd;
	}
	
	printf("%d sockets are binded for all of the interfaces\n",interfaceNumber);

	//create and bind local fd
	int sockfd=createLocalFD();
	FD_SET(sockfd, &allset);
	maxfd=sockfd>maxfd?sockfd:maxfd;


	InitRoutingTable();
	InitPortTable();

	int status;
	char buf[BUF_SIZE-1];
	char message[BUF_SIZE-1];
	struct sockaddr_un addr;
	socklen_t addrlen;
	bzero(&addr, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	char *ptr;
	
	char dst_addr[ADDR_LEN];
	int dst_port;
	int no;
	int routing_index;
	int route_exist;
	int flag;
	struct sockaddr_ll sendingaddr;
	unsigned char recv_mac[ETH_ALEN];
							
	for (;;)
	{
		rset= allset;
    	status = select(maxfd+1 , &rset, NULL, NULL, NULL);
    	if (status == -1) {
        	if (errno == EINTR)
            	continue;
        	err_sys("select error");
    	}
    	printf("sockfd: %d \n", sockfd);
    	if (FD_ISSET(sockfd,&rset)){
    		addrlen=sizeof(struct sockaddr_un); 
    		
    		Recvfrom(sockfd, buf, BUF_SIZE,0,(struct sockaddr *) &addr, &addrlen);
    		
    		printf("received new message from local: %s , with path name: %s\n",buf,addr.sun_path);
    		sscanf(buf, "%s %d %d %s", dst_addr, &dst_port, &flag, message);

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
    			//struct eth_frame *frame=(struct eth_frame *) malloc(sizeof(struct eth_frame));				
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
				//struct frame *frame=(struct frame *) malloc(sizeof(struct frame));
				struct frame *frame=createFrame(i,vms[currentVM-1],dst_addr,SERV_PORT_NO,dst_port,2,message,-1,-1,NULL,NULL,0,-1);
				
				memcpy((void *) ether_frame, (void *) frame, sizeof(*frame));
			    printf("Application payload is ready to be sent out to ");
				ptr=frame->dst_mac;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);

        		setSentAddr(2,i,&sendingaddr);
				
				//application payload is sent
				Sendto(interfaceFDs[routing_table[i].outgoing_if-3], ether_frame, sizeof(*frame), 0, 
						(struct sockaddr *) &sendingaddr, sizeof(sendingaddr));


				printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
				ptr=sendingaddr.sll_addr;
        		no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);
				
				printf("	ODR message type 2, source ip %s, destination ip %s\n", frame->src_addr, frame->dst_addr);

				free(frame);
				free(ether_frame);
									

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

						if(routing_table[i].outgoing_if!=0 && flag==0) route_exist=1; //???flag????
						break;
					}
				}
			
				//route discovery is not needed
				//application payload is sent
				if(route_exist==1) {
			
					printf("Routing information is available for client request\n");
					send_payload(i,j,message);


					
				}
				//route discovery is needed
				//RREQ is sent
				else {
			
					printf("Routing information is not available for client request\n");

					struct frame *frame=createFrame(i,vms[currentVM-1],dst_addr,port_table[j].port,-1,0,NULL,0,flag,NULL,NULL,0,htonl(++broadcast_id));			
				
					void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
					
					
					printf("RREQ is ready to be sent out to ff:ff:ff:ff:ff:ff\n");

					setSentAddr(0,-1,&sendingaddr);

					//RREQ floods on all interface socket
					for(i=0; i<interfaceNumber; i++) {

					
					
						  						
						memcpy(frame->src_mac, interfaces[i].sll_addr, ETH_ALEN);
					
						memcpy((void *) ether_frame, (void *) frame, sizeof(*frame));
  					
  					

  						sendingaddr.sll_ifindex=i+3;
  						printf("if_fd: %d\n",interfaceFDs[i]);
						if(sendto(interfaceFDs[i], ether_frame, sizeof(*frame), 0, 
								(struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {

							perror("ODR: Failed to send Ethernet frame through interface socket\n");
							exit(1);
						}
						printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ff:ff:ff:ff:ff:ff\n", 
									currentVM, currentVM);
						printf("	ODR message type 0, source ip %s, destination ip %s\n", frame->src_addr, frame->dst_addr);
					}

					//there is s request to be sent
					request_hold = 1;
	
					free(frame);
					free(ether_frame);
				}
    		}

    	}	

    
	
    	int index;
	/*------------- J -------------*/
	for(index=0; index<interfaceNumber; index++) {
			int i;
			//RRQ, RRP, or payload received from one of the interface socket
			//FD_ZERO(&rset);
			if(FD_ISSET(interfaceFDs[index], &rset)) {
				//printf("begin send\n");
				printf("New Ethernet frame was received from interface %d\n", index+3);
				struct frame *recvframe=(struct frame *) malloc(sizeof(struct frame));	
				void *ether_frame=(void *) malloc(ETH_FRAME_LEN);
				//!!!!can't receive!!!!
				//printf("===%d+%d===\n", sizeof(interfaceFDs[index]), sizeof(struct eth_frame));
				if(recvfrom(interfaceFDs[index], ether_frame, ETH_FRAME_LEN, 0, NULL, NULL)<0) {
					printf("bbb\n");
					perror("ODR: Failed to receive Ethernet frame througth interface socket\n");
					exit(1);
				}
				printf("ccc\n");
				memcpy((void *) recvframe, (void *) ether_frame, sizeof(*recvframe));
				memcpy(recv_mac, recvframe->src_mac, ETH_ALEN);
				printf("Received Ethernet frame type %d from ", ntohl(recvframe->type));
				//printf("receive frame of type %d from ", ntohl(frame->protocol_no));
				//print mac address
				
				if(printInfo(recv_mac))
					err_sys("Printing error!\n");

        		/*no=ETH_ALEN;
				do {
			
            		printf("%.2x%s", *ptr++ & 0xff, (no==1)?"\n":":");
        		} while(--no>0);*/

        		//RREQ received				
				if(ntohl(recvframe->type)==0) {
					printf("Received new RReq msg\n");
					//struct RREQ *req=(struct RREQ *) malloc(sizeof(struct RREQ));
					//memcpy((void *) req, (void *) (frame->payload), sizeof(*req));
					
					int ind;
					int updateFlag=0;
					
					//add or update 'reverse' path to source node in routing table
					for(i=0; i<10; i++) {
						if(strcmp(recvframe->src_addr, routing_table[i].dst_addr)==0) {
							//new route to source is more efficient
							if(ntohl(recvframe->flag)==1 || routing_table[i].outgoing_if==0 
								|| routing_table[i].hop_cnt >= ntohl(recvframe->hop_cnt)+1) {
								memcpy(routing_table[i].next_hop, recvframe->src_mac, 6);
								routing_table[i].outgoing_if=index+3;
								routing_table[i].hop_cnt=ntohl(recvframe->hop_cnt)+1;
								printf("Local routing table was updated with new %d hop reverse path to source%d\n", 
											ntohl(recvframe->hop_cnt)+1, i+1);
								updateFlag=1;					
							}
							ind = i;
							break;
						}			
					}
					
					//this node is the destination node				
					if(strcmp(recvframe->dst_addr, vms[currentVM-1])==0) {
					
						printf("This node is the destination node for this RREQ\n");
					
						//reply have not been sent back
						//debug
						if((ntohl(recvframe->RREP_flag)==0 && (routing_table[ind].broadcast_id < ntohl(recvframe->broadcast_id)))  //debug no broadcast_id in routing entry
							|| (routing_table[ind].broadcast_id==ntohl(recvframe->broadcast_id) && updateFlag==1)) {
						//send rrep

							//struct frame *replyframe=(struct frame *) malloc(sizeof(struct frame));
							bzero(ether_frame, ETH_FRAME_LEN);
							struct frame *replyframe=createFrame(i,recvframe->dst_addr,recvframe->src_addr,-1,
								recvframe->src_port,1,NULL,-1,recvframe->flag,recv_mac,interfaces[routing_table[ind].outgoing_if-3].sll_addr,0,-1);
							//bzero(frame, sizeof(struct frame));
											
							//RREP Ethernet frame
  							memcpy((void *) ether_frame, (void *) replyframe, sizeof(*replyframe));
  							
							if(printInfo(replyframe->dst_mac))
								err_sys("Printing error!\n");
  						
  							setSentAddr(1,index,&sendingaddr);
  						
  							memcpy(sendingaddr.sll_addr, replyframe->dst_mac, ETH_ALEN);
  					
  							
							if(sendto(interfaceFDs[index], ether_frame, sizeof(*replyframe), 
								0, (struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {
								
								printf("ODR: Failed to send Ethernet frame through interface socket\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent Ethernet frame, source VM%d, destination ", currentVM, currentVM);
							ptr=sendingaddr.sll_addr;
							
							if(printInfo(sendingaddr.sll_addr))
								err_sys("Printing error!\n");
							
							printf("	ODR message type 1, source ip %s, destination ip %s\n", replyframe->src_addr, replyframe->dst_addr);	
						}
	
						routing_table[ind].broadcast_id=ntohl(recvframe->broadcast_id);
						
						continue;
					}
					//1122 midnight
					int routeFound = 0;
					//check whether this node has route information to destination node
					for(i =0; i<VMNUMBER; i++) {
						if(strcmp(recvframe->dst_addr, routing_table[i].dst_addr)==0) {
							if(routing_table[i].outgoing_if!=0 && ntohl(recvframe->flag)==0) {
								routeFound = 1;  
								break;
							}
						}
					}
												
					//has route to destination
					if(routeFound){
						printf("Route to destination found\n");
						
						//reply is sent back
						if((ntohl(recvframe->RREP_flag)==0 && (routing_table[ind].broadcast_id<ntohl(recvframe->broadcast_id) ))
							|| ((routing_table[ind].broadcast_id==ntohl(recvframe->broadcast_id) && updateFlag==1))) {

						
							struct frame *replyframe=createFrame(i,recvframe->dst_addr,recvframe->src_addr,-1,
				recvframe->src_port,1,NULL,-1,0,recv_mac,interfaces[routing_table[ind].outgoing_if-3].sll_addr,routing_table[i].hop_cnt,-1);
							bzero(ether_frame, ETH_FRAME_LEN);
												
							//RREP Ethernet frame
  							memcpy((void *) ether_frame, (void *) replyframe, sizeof(*replyframe));
  							
							if(printInfo(replyframe->dst_mac))
								err_sys("Printing error!\n");
  						
  							setSentAddr(1,index,&sendingaddr);
  													
						
							sendingaddr.sll_hatype=ARPHRD_ETHER;
							sendingaddr.sll_pkttype=PACKET_OTHERHOST;
							memcpy(sendingaddr.sll_addr, replyframe->dst_mac, ETH_ALEN);
  						
  							
							if(sendto(interfaceFDs[index], ether_frame, sizeof(*replyframe), 
								0, (struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {
								
								printf("cannot send prep back\n");
								exit(1);
							}
							
							printf("\nODR at node VM%d sent frame, source VM%d, destination ", currentVM, currentVM);
							
							if(printInfo(sendingaddr.sll_addr))
								err_sys("Printing error!\n");
										
							printf("	ODR message type 1, source ip %s, destination ip %s\n", replyframe->src_addr, replyframe->dst_addr);
						}
						
						routing_table[ind].broadcast_id=ntohl(recvframe->broadcast_id);
						
						//continue flooding the req to update other nodes
						if(updateFlag==1) {
						
							printf("\nFlooding preq to allow updates in other nodes\n");
							
							recvframe->RREP_flag=htonl(1);
							//hop count plus one
							recvframe->hop_cnt=htonl(ntohl(recvframe->hop_cnt)+1);
							bzero(ether_frame, ETH_FRAME_LEN);
							struct frame *replyframe=createFrame(-1,NULL,NULL,ntohl(recvframe->src_port),-1,
	0,NULL,recvframe->RREP_flag,recvframe->flag ,NULL,NULL,recvframe->hop_cnt,recvframe->broadcast_id);
							
						
					
						
							//memcpy((void *) frame->payload, req, sizeof(*req));
							memcpy((void *) ether_frame, (void *) replyframe, sizeof(*replyframe));

							setSentAddr(0,-1,&sendingaddr);
											
							//flooding (since have a more efficnent path)
							for(i = 0; i < interfaceNumber; i++) {
								//do not send to incoming interface					
								if(i == index){
									continue;
								}							
								memcpy(replyframe->src_mac, interfaces[i].sll_addr, ETH_ALEN);
							
								sendingaddr.sll_ifindex=i+3;

								if(sendto(interfaceFDs[i], ether_frame, sizeof(*replyframe), 
										0, (struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {
									
										printf("cannot send preq\n");
										exit(1);
								}
							
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										currentVM);
								printf("ODR message type 0, source ip %s, destination ip %s\n", 
										recvframe->src_addr,recvframe->dst_addr);
							}
						}
					}
					//current node does not have route information to destination node
					else {
						printf("Current does not have route to destination\n");
						if(routing_table[ind].broadcast_id<ntohl(recvframe->broadcast_id) 
							|| (routing_table[ind].broadcast_id==ntohl(recvframe->broadcast_id) && updateFlag==1)) {
						
							printf("RREQ continues to flood\n");
							
							struct frame *replyframe=createFrame(-1,NULL,NULL,ntohl(recvframe->src_port),-1,
	0,NULL,recvframe->RREP_flag,recvframe->flag ,NULL,NULL,recvframe->hop_cnt,recvframe->broadcast_id);
							bzero(ether_frame, ETH_FRAME_LEN);
						
		
								
							
							//memcpy((void *) frame->payload, req, sizeof(*req));
							memcpy((void *) ether_frame, (void *) replyframe, sizeof(*replyframe));
						
							setSentAddr(0,-1,&sendingaddr);
							sendingaddr.sll_hatype=ARPHRD_ETHER;
							sendingaddr.sll_pkttype=PACKET_BROADCAST;
					
							//flooding, since no rotue was found
							for(i = 0; i < interfaceNumber; i++) {
						
								if(i == index) continue;
								
								memcpy(replyframe->src_mac, interfaces[i].sll_addr, ETH_ALEN);
								
								//debug 
								sendingaddr.sll_ifindex = i+3;

								if(sendto(interfaceFDs[i], ether_frame, sizeof(*replyframe), 
									0, (struct sockaddr *) &sendingaddr, sizeof(sendingaddr))<0) {
									
									printf("cannot send RReq\n");
									exit(1);
								}
								
								printf("ODR at node VM%d sent Ethernet frame to destination ff:ff:ff:ff:ff:ff\n", 
										currentVM);
								printf("ODR message type 0, source ip %s, destination ip %s\n", recvframe->src_addr, recvframe->dst_addr);
							}
						}
						
						routing_table[ind].broadcast_id=ntohl(recvframe->broadcast_id);
					}

				}
	/*------------- J -------------*/

	/*------------- SOU -------------*/
				else if(ntohl(recvframe->type)==1) {	//RREP received
					printf("\n This is a RREP ethernet frame.\n\n");

				//	struct RREP *rep = (struct RREP *)malloc(sizeof(struct RREP));
				//	memcpy(rep, (void *)(frame->payload), sizeof(*rep));
					if(update_table(recvframe->src_addr,recvframe->src_mac ,recvframe->flag, recvframe->hop_cnt, index) != 1)
						err_sys("Updating error!\n");

					if(strcmp(recvframe->dst_addr ,vms[currentVM-1])!=0){	//this is not destination node
						printf("\nThis is not the destination node of RREP.\n\n");

						recvframe->hop_cnt += 1;

						//retransmit RREP
						if(sendPacket(recvframe, &sendingaddr,1) == 1)
							err_sys("Sending error!\n");

						//free(recvframe);


					}else{	//this is destination node

						printf("\nThis is the destination node of RREP.\n\n");
						if (request_hold)
						{
							//send the payload
							printf("Routing information obtained, now sending paylaod through route\n");
							for(i=0; i<10; i++) {

								if(strcmp(routing_table[i].dst_addr, dst_addr)==0) {

									if(routing_table[i].outgoing_if!=0 && flag==0) route_exist=1; //???flag????
									break;
								}
							}
							send_payload(i,j,message);
							request_hold = 0;
						}
					}

				}else{	//app payload
					printf("\n This is a payload ethernet frame.\n\n");

					//struct payload *pl = (struct payload *)malloc(sizeof(struct payload));
					//memcpy(pl, (void *)(frame->payload), sizeof(*pl));

					if(strcmp(recvframe->dst_addr, vms[currentVM-1]) != 0){	//This is not destination node
						printf("\nThis is not the destination node of app payload.\n\n");
						if(sendPacket(recvframe, &sendingaddr,2) == 1)
							err_sys("Sending error!\n");

						//free(pl);
					}else{	
						//This is destination node
						printf("\nThis is the destination node of app payload.\n\n");

						sendLocal(port_cnt,&addr, sockfd, recvframe->message, recvframe->src_port, recvframe->src_addr, recvframe->dst_port);

						printf("APP payload has been sent to local client\n");
					}
				}


				free(recvframe);
			}
		}
	}
	/*------------- SOU -------------*/
	exit(0);
}