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



	exit(0)
}