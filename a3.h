#include "unp.h"
#include <sys/socket.h>
#include <time.h>
#include "hw_addrs.h"
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#define MSG_SIZE 1000
#define SERV_PATH "serv_addr"
#define SERV_PORT_NO 5000

#define ODR_PATH "odr_addr"
#define ODR_PORT_NO 5050

#define PROTOCOL_NO 0xaacd
#define ADDR_LEN 16

#define PAYLOAD_LEN 100
#define VMNUMBER 10
#define SIZE 255
#define BUF_SIZE 255

char* vms[]={
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
