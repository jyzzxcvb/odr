#include    "unp.h"
#include    <sys/socket.h>
#include <time.h>

#define MSG_SIZE 1000
#define SERV_ADDR "serv_addr"
#define MAXLINE 100

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

//抄的
struct interfaceList{
	int idx;
	char ifaceName[STR_SIZE];
	char ifaddr[IP_SIZE];
	char haddr[MAC_SIZE];
	struct interfaceList *next;
};