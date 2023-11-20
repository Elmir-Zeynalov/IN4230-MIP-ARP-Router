#include "daemon_routing_utils.h"
#include "mip.h"


/*
 * This message is used as a sort of broadcast to send HELLO messages on all interfaces
 * The difference between this and a reqular broadcast is that here we set the type 
 * as a ROUTING Message because this is strictly a routing specific packet that 
 * should be, when recevied by a MMIP daemon, sent up to its routing daemon for 
 * furhter processing. 
 *
 * In short, this is just to say HELLO, to all the neighbouring routers. 
 * ifs: datastructure to hold all socket connection (fds)
 * src_mip: the source mip address
 * message: the message we are trying to spread HELLO in this case
 * len: length of message
 * cache: the mip cache. 
 *
 */
int disseminate_hello_message(struct ifs_data *ifs, uint8_t *src_mip, char *message, size_t len, struct Cache *cache)
{
    struct ether_frame      frame_hdr;
    struct msghdr   *msg;
    struct iovec    msgvec[3];
    int rc;

    uint8_t dst_addr[] = ETH_BROADCAST;
    memcpy(frame_hdr.dst_addr, dst_addr, 6);
    
    frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
    frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;

    struct mip_header       header;
    //create MIP header with TTL=0x01
    header = construct_mip_header(0xFF, *src_mip, 0x01, sizeof(uint8_t), MIP_ROUTING);

    /* frame header */
    msgvec[0].iov_base      = &frame_hdr;
    msgvec[0].iov_len       = sizeof(struct ether_frame);

    /* MIP header */
    msgvec[1].iov_base      = &header;
    msgvec[1].iov_len       = sizeof(struct mip_header);

    /* MIP SDU */
    msgvec[2].iov_base      = message; 
    msgvec[2].iov_len       = len;
    
    /*
    * We loop over all of the interfaces and send a message on each interface.
    */
    int i;

    for(i = 0; i < ifs->ifn; i++)
    {
        if(debug_flag)
        {       printf("\n\n[<info>] BROADCAST [<info>]");
                printf("\n******");
                printf("\n[<info>] Sending ARP-REQUEST messge on interface: ");
                print_mac_addr(ifs->addr[i].sll_addr, 6);
                printf("[<info>]\n");
        }

                memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
                if(debug_flag)
                {
                        printf("\n[<info>] Source address [<info>]\n");
                        printf("\t");
                        print_mac_addr(frame_hdr.src_addr, 6);
                        printf("\n[<info>] Destination address [<info>]\n");
                        printf("\t");
                        print_mac_addr(frame_hdr.dst_addr, 6);

                        printf("\n[<info>] Source MIP [<info>]\n");
                        printf("\t%d\n", header.src_addr);
                        printf("[<info>] Destination MIP [<info>]\n");
                        printf("\t%d\n", header.dst_addr);

                        printf("\n[<info>] TTL: %d [<info>]\n", header.ttl);
                }

                msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
                msg->msg_name           = &(ifs->addr[i]);
                msg->msg_namelen        = sizeof(struct sockaddr_ll);
                msg->msg_iovlen         = 3;
                msg->msg_iov            = msgvec;

                rc = sendmsg(ifs->rsock, msg, 0);
                if(rc == -1)
                {
                        perror("sendmsg");
                        free(msg);
                        return -1;
                }
                free(msg);
        }
        return rc;  
}

/*
 * This message is used as a sort of broadcast to spread the UPDATE message on all interfaces
 * The difference between this and a reqular broadcast is that here we set the type 
 * as a ROUTING Message because this is strictly a routing specific packet that 
 * should be, when recevied by a MMIP daemon, sent up to its routing daemon for 
 * furhter processing. 
 *
 * Difference here between the HELLO disseminate is that here we pass the entire routing table
 * in the message field. 
 * This is the primary method of how routing daemons spread their routing tables with other nehgibours! 
 *
 * In short, this is just to say HELLO, to all the neighbouring routers. 
 * ifs: datastructure to hold all socket connection (fds)
 * src_mip: the source mip address
 * message: the message we are trying to spread HELLO in this case
 * len: length of message
 * cache: the mip cache. 
 *
 */

int disseminate_update_message(struct ifs_data *ifs, uint8_t *src_mip, char *message, size_t len)
{
    struct ether_frame      frame_hdr;
    struct msghdr   *msg;
    struct iovec    msgvec[3];
    int rc;

    uint8_t dst_addr[] = ETH_BROADCAST;
    memcpy(frame_hdr.dst_addr, dst_addr, 6);
    
    frame_hdr.eth_proto[0] = (htons(0x88B5) >> (8*0)) & 0xff;
    frame_hdr.eth_proto[1] = (htons(0x88B5) >> (8*1)) & 0xff;

    struct mip_header       header;
    //create MIP header with TTL=0x01
    header = construct_mip_header(0xFF, *src_mip, 0x01, sizeof(uint8_t), MIP_ROUTING);

    /* frame header */
    msgvec[0].iov_base      = &frame_hdr;
    msgvec[0].iov_len       = sizeof(struct ether_frame);

    /* MIP header */
    msgvec[1].iov_base      = &header;
    msgvec[1].iov_len       = sizeof(struct mip_header);

    /* MIP SDU */
    msgvec[2].iov_base      = message; 
    msgvec[2].iov_len       = len;
    
    /*
    * We loop over all of the interfaces and send a message on each interface.
    */
    int i;

    for(i = 0; i < ifs->ifn; i++)
    {
        if(debug_flag)
        {       printf("\n\n[<info>] BROADCAST [<info>]");
                printf("\n******");
                printf("\n[<info>] Sending ARP-REQUEST messge on interface: ");
                print_mac_addr(ifs->addr[i].sll_addr, 6);
                printf("[<info>]\n");
        }

                memcpy(frame_hdr.src_addr, ifs->addr[i].sll_addr, 6);
                if(debug_flag)
                {
                        /*
                        printf("\n[<info>] Source address [<info>]\n");
                        printf("\t");
                        print_mac_addr(frame_hdr.src_addr, 6);
                        printf("\n[<info>] Destination address [<info>]\n");
                        printf("\t");
                        print_mac_addr(frame_hdr.dst_addr, 6);

                        printf("\n[<info>] Source MIP [<info>]\n");
                        printf("\t%d\n", header.src_addr);
                        printf("[<info>] Destination MIP [<info>]\n");
                        printf("\t%d\n", header.dst_addr);

                        printf("\n[<info>] TTL: %d [<info>]\n", header.ttl);
                        */
                        printf("FORWARDiNG ROUTE UPDATE\n");
                }

                msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
                msg->msg_name           = &(ifs->addr[i]);
                msg->msg_namelen        = sizeof(struct sockaddr_ll);
                msg->msg_iovlen         = 3;
                msg->msg_iov            = msgvec;

                rc = sendmsg(ifs->rsock, msg, 0);
                if(rc == -1)
                {
                        perror("sendmsg");
                        free(msg);
                        return -1;
                }
                free(msg);
        }

        return rc;  
}
