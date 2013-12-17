#ifndef _BALL_DEF_H
#define _BALL_DEF_H

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(PTR, TYPE, MEMBER) \
        (TYPE *)((char *)(PTR) - offsetof(TYPE, MEMBER))

#endif
