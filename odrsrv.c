#include "hw_addrs.h"
#include "API.h"
#include "a3.h"

int
main(int argc, char **argv)
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
		interfaces[interfaceNumber].sll_protocol=htons(PROTOCOL_VALUE);
		interfaces[interfaceNumber].sll_ifindex=hwa->if_index;
		memcpy(interfaces[interfaceNumber].sll_addr, hwa->if_haddr, ETH_ALEN);
		
		//Create a PF_PACKET socket for every interface
		interfaceFDs[interfaceNumber]=Socket(PF_PACKET,SOCK_RAW,htons(PROTOCOL_VALUE));  //set the value in header file ?
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
	struct sockaddr_un addr1, addr2;
	
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
    		
    	}	
    
	}
	exit(0)
}