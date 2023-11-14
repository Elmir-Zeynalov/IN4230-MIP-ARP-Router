#ifndef _ROUTING_H
#define _ROUTING_H


#define HELLO_INTERVAL 5000
#define HELLO_TIMEOUT 10000
#define MAX_EVENTS 5
#define MAX_CONNS 5

enum state {
    INIT = 0,
    WAIT,
    EXIT
};

#endif
