#include    "unp.h"
#include    <sys/socket.h>
#include <time.h>

#define MSG_SIZE 1000
#define SERV_PATH "serv_addr"
#define SERV_PORT 5000

#define ODR_PATH "odr_addr"
#define ODR_PORT 5050

#define PROTOCOL_NO 0xaacd
#define ADDR_LEN 16

#define PAYLOAD_LEN 100
//抄的
struct interfaceList{
	int idx;
	char ifaceName[STR_SIZE];
	char ifaddr[IP_SIZE];
	char haddr[MAC_SIZE];
	struct interfaceList *next;
};