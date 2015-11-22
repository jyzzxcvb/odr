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

int update_table(char src_addr, int flag, int hop_count, struct eth_frame *frame, int index);
int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port);
int sendPacket(struct eth_frame *frame, struct sockaddr_ll *send_if_addr, void *packet, int type, char dst_addr[], char src_addr[]);

struct sockaddr_ll if_addr[3];
//local routing table
struct routing_entry routing_table[10];
//local port talbe
struct port_entry port_table[10];


int main(int argc, char **argv){

	struct eth_frame *frame=(struct eth_frame *)malloc(sizeof(struct eth_frame));
	struct sockaddr_ll *send_if_addr;

	if(frame->type == 1) {	//RREP received
		printf("\n This is a RREP ethernet frame.\n\n");

		struct RREP *rep = (struct RREP *)malloc(sizeof(struct RREP));
		memcpy(rep, (void *)(frame->payload), sizeof(*rep));
		if(update_table(rep->src_addr, rep->flag, rep->hopcnt, frame, index) != 1)
			err_sys("Updating error!\n");

		if(rep->dst_addr != vms[cur-1]){	//this is not destination node
			printf("\nThis is not the destination node of RREP.\n\n");

			rep->hop_cnt += 1;

			//retransmit RREP
			if(sendPacket(frame, send_if_addr, (void *)rep, 1, rep->dst_addr, rep->src_addr) == 1)
				err_sys("Sending error!\n");

			free(rep);


		}else{	//this is destination node

			printf("\nThis is the destination node of RREP.\n\n");

			char msg[7] = "HELLO!";
			int port = 3000;
			if(sendLocal(port_cnt,local_addr, odrfd, msg, port, rep->src_addr, rep->dst_port) == 1)
				err_sys("Sending error!\n");

			printf("RREP has been sent to local client\n");

		}

	}else{	//app payload
		printf("\n This is a payload ethernet frame.\n\n");

		struct payload *pl = (struct payload *)malloc(sizeof(struct payload));
		memcpy(pl, (void *)(frame->payload), sizeof(*pl));

		if(pl->dst_addr != vms[cur-1]){	//This is not destination node
			printf("\nThis is not the destination node of app payload.\n\n");
			if(sendPacket(frame, send_if_addr, (void *)pl, 2, pl->dst_addr, pl->src_addr) == 1)
				err_sys("Sending error!\n");

			free(pl);
		}else{	//This is destination node
			printf("\nThis is the destination node of app payload.\n\n");

			if(sendLocal(port_cnt,local_addr, odrfd, pl->message, pl->src_port, pl->src_addr, pl->dst_port) == 1)
				err_sys("Sending error!\n");

			printf("APP payload has been sent to local client\n");
		}
	}

	free(frame);

	return 1;
}



int update_table(char src_addr, int flag, int hop_count, struct eth_frame *frame, int index){
	for(int i=0; i < 10; i++) {
		if(strcmp(src_addr, routing_table[i].dst_addr)==0) {
		//route tabe need to be updated
			if(flag ==1 || 
				routing_table[i].outgoing_if==0 || 
				routing_table[i].hop_cnt>hop_count+1 || 
				((routing_table[i].hop_cnt==hop_count+1) && (routing_table[i].outgoing_if!=index+3)))
			{
				memcpy(routing_table[i].next_hop, frame->src_mac, ETH_ALEN);
				routing_table[i].outgoing_if=index+3;
				routing_table[i].hop_cnt=hop_count+1;
				struct timeval current_time;
				gettimeofday(&current_time, NULL);
				routing_table[i].timestamp= current_time;
				printf("A better route given forward to destination node VM%d with hop count %d is updated\n", count+1, routing_table[count].hop_cnt);				
			}
							
			break;
		}			
	}
	return 1;
}

int sendLocal(int port_cnt, struct sockaddr_un *local_addr, int odrfd, char msg[], int port, char src_addr[], int dst_port){
	int length;
	int frame_len = sizeof(struct eth_frame);
	char buf[frame_len];

	if((length = sprintf(buf, "%s %s %d", msg, src_addr, port)) < 0){
		err_sys("Format error!\n");
		return 1;
	}

	//find the local client the RREP is for
	for(int i=0; i <= port_cnt; i++) {		
		if(port_table[i].port == (dst_port)) {
			local_addr->sun_path = port_table[i].sun_path;
			break;
		}
	}
						
	if(sendto(odrfd, buf, length, 0, (SA *) local_addr, sizeof(*local_addr))<0){
		err_sys("Sending error!\n");
		return 1;
	}

	return 0;
}

int sendPacket(struct eth_frame *frame, struct sockaddr_ll *send_if_addr, void *packet, int type, char dst_addr[], char src_addr[]){
	//prepare packet for retransmit

	int temp_index = 0;
	bzero(frame, sizeof(struct eth_frame));

	frame->protocol_no = PROTO;
	frame->type = type;

	for(int n=0; n<10; n++) {
		if(dst_addr == routing_table[n].dst_addr) {
			memcpy(frame->dst_mac, routing_table[n].next_hop, ETH_ALEN);
			temp_index = routing_table[n].outgoing_if-3;
			memcpy(frame->src_mac, if_addr[temp_index].sll_addr, ETH_ALEN);
			break;
		}
	}

	bzero(send_if_addr, sizeof(struct sockaddr_ll));

	send_if_addr->sll_family = PF_PACKET;
	send_if_addr->sll_protocol = PROTO;
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
			send_if_addr.sll_addr = frame->dst_mac;
			send_if_addr.sll_ifindex=temp_index;
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