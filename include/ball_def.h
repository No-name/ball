#ifndef _BALL_DEF_H
#define _BALL_DEF_H

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(PTR, TYPE, MEMBER) \
        (TYPE *)((char *)(PTR) - offsetof(TYPE, MEMBER))

#ifndef TRUE
#define TRUE (1 == 1)
#endif

#ifndef FALSE
#define FALSE (1 == 0)
#endif

#endif
