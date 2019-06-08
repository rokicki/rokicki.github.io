/* client data structures and functions */
#include <stdio.h>
#define WATER ('~')
#define BASE ('@')
#define WALL ('#')
#define PLAIN ('.')

#define MAXPL 65000  /* XXX: 256 players is enough? */

typedef struct board board;
typedef struct player player;
typedef struct package package;
typedef struct square square;

typedef unsigned char byte;

/* Flags for player state */
#define ST_BASE     (1 << 0)  /* Originally a base */
#define ST_DIRTY    (1 << 1)  /* Visited by someone else */
#define ST_VISITED  (1 << 2)  /* Visited by us */
#define ST_PACKDEST (1 << 3)  /* Destination of a package we have */
#define ST_UNREACHABLE (1 << 4) /* Square is unreachable */

#define PK_WAS_DROPPED (1 << 0)  /* Package was dropped by someone else */
#define PK_UNDELIVERABLE (1 << 1) /* Package can't be delivered, ever */
#define MYPACK (1<<5)    /* this is my package; we'll clear it later */

/* Flags for package state */
struct square {
    byte lo, hi;  /* bounds on the no. of packages */
    short ip ;    /* interesting point? */
    byte state;   /* base, dirty, visited by us, dest of package? */
    package *pkgs; /* list of packages at this square */
};

struct board {
    int w, h;     /* width and height */
    char **bd;    /* board as char[][], outer edge is apron */
    square **sq;  /* state of board squares, incl. packages */
};

struct package {
    int id;       /* package id */
    int weight;   /* duh */
    int destx, desty; /* where to deliver */
    byte state;
    package *next; /* next in list */
};

struct player {
    int id;       /* server assigned id */
    int x, y;     /* current location */
    int capacity; /* max. weight that can be carried */
    int money;    /* money left */
    int score;    /* current score */
    package *pkgs; /* Packages I'm carrying */
    int alive;    /* Bit used by GC to remove dead players. */
};

extern player *self;   /* ourselves */
extern board bd;       /* globally accessible board */

/* saturation arithmetic */
#define sat_inc(x) if ((x) < 255) (x)++
#define sat_dec(x) if ((x) > 0 && (x) < 255) (x)--

FILE *tcpconnect(char *host, int port);
player *init(FILE *fp, board *bd);
void get_response(FILE *fp, int initial);
package *get_packages(FILE *fp);
void free_packages(package **list, int dostats);
package *pkg_find(package **list, int id);
package *pkg_findxy(package **list, int x, int y);
void pkg_add(package **list, int id);
void pkg_del(package **list, int id);

/* player table */
extern player players[MAXPL];
extern int nplayers;
player *pl_lookup(int id, int create);
int local_players(player *found);

extern int move_count;

/* garbage collection of dead players */
void gc_mark(void);
void gc_sweep(void);

/* statistics */
extern int npkgs_seen;        /* Packages we know about */
extern double total_wt_seen;  /* Their total weight, for computing averages */
extern int nbases;            /* total number of bases */
extern int nknown_bases;       /* Number of bases we know about */

double avg_weight(void);
double avg_pkgs_in_base(void);

/* util */
#include "util.h"

#include "ip.h"

/* hooks */
void clearcache() ;
void drop_hook(int x, int y) ;
