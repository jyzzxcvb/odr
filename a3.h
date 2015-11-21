#include    "unp.h"
#include    <sys/socket.h>
#include <time.h>

#define MSG_SIZE 1000
#define SERV_ADDR "serv_addr"

//抄的
struct interfaceList{
	int idx;
	char ifaceName[STR_SIZE];
	char ifaddr[IP_SIZE];
	char haddr[MAC_SIZE];
	struct interfaceList *next;
};