#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>		/* uint8_t */
#include <unistd.h>		/* size_t */
#include <linux/if_packet.h>	/* struct sockaddr_ll */
#include "mip.h"
#include "ether.h"
#include "cache.h" 
#include "queue.h"

//#define MAX_EVENTS	10
#define MAX_IF		3
#define ETH_BROADCAST	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define MAC_ADDR_LENGTH 6
#define ETH_P_MIP	0x88B5
#define DEFAULT_TTL	8

struct ifs_data {
	struct sockaddr_ll addr[MAX_IF];
	int rsock;
	int ifn;
	int unix_sock;
	int routin_sock;
};

void get_mac_from_interfaces(struct ifs_data *);
void print_mac_addr(uint8_t *, size_t);
void init_ifs(struct ifs_data *, int);
int create_raw_socket(void);
int forwarding_engine(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, struct ifs_data *ifs, uint8_t *my_src_mip);


int handle_arp_packet(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, struct ifs_data *ifs, uint8_t *my_mip_addr, struct sockaddr_ll so_name, struct ether_frame frame_hdr, struct mip_header header, uint8_t *buf);
//int handle_arp_packet(struct Cache *cache, struct Queue *queue, struct ifs_data *ifs, uint8_t *my_src_mip);
int send_arp_response(struct ifs_data *ifs, struct sockaddr_ll *so_name,struct ether_frame frame, struct mip_header *m_header, uint8_t my_src_mip, size_t len);
int send_arp_request(struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, struct Cache *cache);
int send_ping_message(struct CacheEntry *cache_entry, struct ifs_data *ifs, uint8_t *src_mip, uint8_t *dst_mip, char *buf, size_t buf_len, struct Cache *cache, uint8_t ttl);


int send_msg(struct Cache *cache, struct Queue *queue, struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, char *buf, size_t buf_len, uint8_t ttl);

void handle_client(struct Cache *cache, struct Queue *queue, struct Queue *broadcast_queue, int fd, struct ifs_data *local_ifs, uint8_t MIP_address);
int determine_unix_connection(int fd, struct ifs_data *local_ifs);


int forward_packet_to_next_hop(struct ifs_data *ifs, uint8_t *src_mip, uint8_t dst_mip, uint8_t ttl, char *buf, size_t buf_len, struct Cache *cache);

#endif // !DEBUG
