#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <assert.h>

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "client.h"

/* globals */
char *thisline;  /* XXX: make util.c happy */
board bd;        /* global board */
player *self;

player players[MAXPL]; /* player table */
int nplayers;          /* number of players */
player *pl_lookup(int id, int create);
int local_players(player *found);

int move_count ;       /* number of moves made */

int npkgs_seen;        /* Packages we know about */
double total_wt_seen;  /* Their total weight, for computing averages */
int nbases;            /* total number of bases */
int nknown_bases;       /* Number of bases we know about */

/* Create a TCP connection to host and port.  Returns a file
 * descriptor on success, -1 on error. */
FILE *tcpconnect (char *host, int port)
{
    struct hostent *h;
    struct sockaddr_in sa;
    int s;
    FILE *fp;
    int flag = 1 ;
    
    /* Writing to an unconnected socket will cause a process to receive
     * a SIGPIPE signal.  We don't want to die if this happens, so we
     * ignore SIGPIPE.  */
    // signal (SIGPIPE, SIG_IGN);

    /* Get the address of the host from its hostname. */
    h = gethostbyname (host);
    if (!h || h->h_length != sizeof (struct in_addr)) {
        fprintf(stderr, "%s: no such host\n", host);
        return NULL;
    }

    /* Create a TCP socket. */
    s = socket (AF_INET, SOCK_STREAM, 0);

    /* Use bind to set an address and port number for our end of
     * the TCP connection. */
    bzero (&sa, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons (0);                  /* tells OS to choose a port */
    sa.sin_addr.s_addr = htonl (INADDR_ANY);  /* tells OS to choose IP addr */
    if (bind (s, (struct sockaddr *) &sa, sizeof (sa)) < 0) {
        perror ("bind");
        close (s);
        return NULL;
    }

    /* Now use h to set the destination address. */
    sa.sin_port = htons (port);
    sa.sin_addr = *(struct in_addr *) h->h_addr;

    /* And connect to the server */
    if (connect (s, (struct sockaddr *) &sa, sizeof (sa)) < 0
        && errno != EINPROGRESS) {
        perror (host);
        close (s);
        return NULL;
    }
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    /* Convert to FILE * */
    fp = fdopen(s, "r+");
    if (fp == NULL) {
        perror ("fdopen");
	close (s);
	return NULL;
    }

    return fp;
}

/**********************************************************************/

player *init(FILE *fp, board *bd)
{
    char buf[10000];
    int i, j, id, cap, mon;
    player *pl;

    /* 1. Client: send the string Player */
    fprintf(fp, "Player\n"); fflush(fp);

    /* 2. Server: send board */
    /* 2-1. Server: board dimensions */
    assert(fgets(buf, sizeof(buf), fp));
    assert(sscanf(buf, " %d %d", &bd->w, &bd->h) == 2);

    /* 2-2. Server: board rows 1..n */
    bd->bd = mymalloc((bd->h + 2) * sizeof(char *));
    bd->sq = mymalloc((bd->h + 2) * sizeof(square *));
    for (i = 0; i <= bd->h + 1; i++) {
	bd->bd[i] = mymalloc((bd->w + 2) * sizeof(char));
	memset(bd->bd[i], WALL, (bd->w + 2) * sizeof(char));

	bd->sq[i] = mymalloc((bd->w + 2) * sizeof(square));

	if (i >= 1 && i <= bd->h) {
	    assert(fgets(buf, sizeof(buf), fp));
	    assert(strlen(buf) == bd->w + 1);
	    strncpy(bd->bd[i]+1, buf, bd->w);

	    for (j = 1; j <= bd->w; j++)
		if (bd->bd[i][j] == '@') {
		    bd->sq[i][j].hi = 255;
		    bd->sq[i][j].state |= ST_BASE;
		    nbases++;
		}
	}
    }

    /* 3. Server: send player's configuration */
    assert(fgets(buf, sizeof(buf), fp));
    assert(sscanf(buf, " %d %d %d", &id, &cap, &mon) == 3);
    pl = pl_lookup(id, 1); assert(pl == players);
    pl->capacity = cap;
    pl->money = mon;
    pl->score = 0;
    pl->pkgs = NULL;

    self = pl;

    /* 4. Server: send initial server response (with the initial location of all players) */
    get_response(fp, 1);
    move_count = 0 ;
    exterseconds() ;
    return pl ;
}

/**********************************************************************/

/* Mark a square as visited or dirty */
static void visit(player *pl, int x, int y)
{
    if (pl == self) {
	/* Haven't visited this base yet?  Count it. */
	if (bd.sq[y][x].state & ST_BASE && !(bd.sq[y][x].state & ST_VISITED))
	    nknown_bases++;
	bd.sq[y][x].state |= ST_VISITED;
    } else {
	bd.sq[y][x].state |= ST_DIRTY;
    }
}

/**********************************************************************/

void get_response(FILE *fp, int initial)
{
    char buf[10000], *p;
    int id = 0, x, y;
    player *pl = NULL;
    package *pkg, *list;

    gc_mark();   /* mark all players as dead */
    move_count++ ;
    if (!fgets(buf, sizeof(buf), fp))
	die("server closed connection");
    if (buf[0])
	buf[strlen(buf) - 1] = '\0';
    for (p = strtok(buf, " "); p; p = strtok(NULL, " ")) {
	switch (*p) {
	case '#':
	    id = atoi(p + 1);
	    pl = pl_lookup(id, 1);  /* new players can be added later too... */
	    assert(pl);
	    pl->alive = 1;
	    break;

	case 'X': /* X num: The robots x-coordinate is set to num. */
	    pl->x = atoi(strtok(NULL, " "));
	    break;

	case 'Y': /* Y num: The robots y-coordinate is set to num. */
	    pl->y = atoi(strtok(NULL, " "));
	    visit(pl, pl->x, pl->y);
	    break;

	case 'N': /* N: The robot moved north. */
	    pl->y++;
	    visit(pl, pl->x, pl->y);
	    break;

	case 'S': /* S: The robot moved south. */
	    pl->y--;
	    visit(pl, pl->x, pl->y);
	    break;

	case 'E': /* E: The robot moved east. */
	    pl->x++;
	    visit(pl, pl->x, pl->y);
	    break;

	case 'W': /* W: The robot moved west. */
	    pl->x--;
	    visit(pl, pl->x, pl->y);
	    break;

	case 'P': /* P: The robot picked up package with id id. */
	    id = atoi(strtok(NULL, " "));
	    pkg_add(&pl->pkgs, id);
	    sat_dec(bd.sq[pl->y][pl->x].lo);
	    sat_dec(bd.sq[pl->y][pl->x].hi);

	    /* If we know about the package, fill in details and
               remove it from square's list. */
	    pkg = pkg_find(&bd.sq[pl->y][pl->x].pkgs, id);
	    if (pkg) {
		pl->pkgs->weight = pkg->weight;
		pl->pkgs->destx  = pkg->destx;
		pl->pkgs->desty  = pkg->desty;
		pkg_del(&bd.sq[pl->y][pl->x].pkgs, id);
	    }

	    /* Mark the destination square if we're delivering */
	    if (pl == self) {
		assert(pl->pkgs->weight >= 0);
		bd.sq[pl->pkgs->desty][pl->pkgs->destx].state |= ST_PACKDEST;
	    }

            clearcache() ;
//   printf("#%d: P %d\n", pl->id, id);
	    break;

	case 'D': /* D: The robot dropped package with id id. */
	    id = atoi(strtok(NULL, " "));
	    list = bd.sq[pl->y][pl->x].pkgs;
	    pkg_add(&list, id);
	    sat_inc(bd.sq[pl->y][pl->x].hi);

	    /* If we know more about this package, great! */
	    pkg = pkg_find(&pl->pkgs, id);
	    if (pkg) {
		list->weight = pkg->weight;
		list->destx = pkg->destx;
		list->desty = pkg->desty;
		pkg_del(&pl->pkgs, id);
	    }
	    
	    /* If its our package, clear the destination square flag (maybe) */
	    if (pl == self) {
		assert(list->weight >= 0);
		if (!pkg_findxy(&pl->pkgs, list->destx, list->desty))
		    bd.sq[list->desty][list->destx].state &= ~ST_PACKDEST;
	    } else
		list->state |= PK_WAS_DROPPED;

            drop_hook(pl->x, pl->y) ;
            clearcache() ;
//   printf("#%d: D %d\n", pl->id, id);
	    break;

	    /* XXX: debugging commands! */
	case 'x':
	    x = atoi(strtok(NULL, " "));
	    if (pl->x != x) {
		fprintf(stderr, "MISMATCH: pl %d (%d,%d) X = %d\n",
			pl->id, pl->x, pl->y, x);
		die("inconsistency");
	    }
	    break;

	case 'y':
	    y = atoi(strtok(NULL, " "));
	    if (pl->y != y) {
		fprintf(stderr, "MISMATCH: pl %d (%d,%d) Y = %d\n",
			pl->id, pl->x, pl->y, y);
		die("inconsistency");
	    }
	    break;

	default:
	    die("unexpected response");
	}
    }
    gc_sweep();  /* clean up dead players */
}



/**********************************************************************/

/* local_players: Get list of players within 4 squares of me in any
   direction. */
int local_players(player *found) {
  int count, i, min_x, min_y, max_x, max_y;
  count = 0;
  max_x = self->x + 4;
  min_x = self->x - 4;
  max_y = self->y + 4;
  min_y = self->y - 4;

  for ( i = 0; i < nplayers; i++)
    if( players[i].alive &&
	players[i].id != self->id &&
	players[i].x >= min_x && players[i].x <= max_x &&
	players[i].y >= min_y && players[i].y <= max_y )
      found[count++] = players[i];
  return count;
}

/**********************************************************************/

/* get_packages: Get list of packages that can currently be picked up
   from server. */
package *get_packages(FILE *fp)
{
    package *list;
    int id, x, y, weight, ch, npkgs;

    free_packages(&bd.sq[self->y][self->x].pkgs, 1);

    /* scanf magic in order to read to end-of-line, *but no further* */
    list = NULL;
    for (npkgs = 0; ; npkgs++) {
	fscanf(fp, "%*[^\n0-9]");
	ch = fgetc(fp);
	if (ch == EOF || ch == '\n')
	    break;
	else
	    ungetc(ch, fp);

	assert(fscanf(fp, " %d %d %d %d", &id, &x, &y, &weight) == 4);
	pkg_add(&list, id);
	list->weight = weight;
	list->destx = x;
	list->desty = y;
	if (weight > self->capacity 
	    || (bd.sq[list->desty][list->destx].state & ST_UNREACHABLE)) {
	    list->state |= PK_UNDELIVERABLE;
	    npkgs--;
	    // printf("WOAH: pkg %d undeliverable\n", id);
	} else {
	    npkgs_seen++;
	    total_wt_seen += list->weight;
	}
    }

    /* We now have definite count of packages at this square */
    bd.sq[self->y][self->x].pkgs = list;
    if (npkgs > 255) npkgs = 255;        /* Saturation arthimetic! */
/*    printf("AHOY: npkgs = %d\n", npkgs); */
    bd.sq[self->y][self->x].lo = npkgs;
    bd.sq[self->y][self->x].hi = npkgs;

    return list;
}

/**********************************************************************/

void free_packages(package **list, int dostats)
{
    package *p, *q;

    for (p = *list; p; p = q) {
	q = p->next;
	if (! (p->state & PK_UNDELIVERABLE)) {
	    npkgs_seen--;
	    total_wt_seen -= p->weight;
	}
	free(p);
    }
    *list = NULL;
}

/**********************************************************************/

package *pkg_find(package **list, int id)
{
    package *p;

    for (p = *list; p; p = p->next)
	if (p->id == id)
	    return p;
    return NULL;
}

package *pkg_findxy(package **list, int x, int y)
{
    package *p;

    for (p = *list; p; p = p->next)
	if (p->destx == x && p->desty == y)
	    return p;
    return NULL;
}

/**********************************************************************/

void pkg_add(package **list, int id)
{
    package *pkg;

    pkg = mymalloc(sizeof(package));
    pkg->id = id; pkg->weight = -1;

    pkg->next = *list;
    *list = pkg;
}

/**********************************************************************/

void pkg_del(package **list, int id)
{
    package *p, *q;
    package tmp;

    assert(*list);
    tmp.next = *list;
    for (q = &tmp, p = tmp.next; p; q = p, p = p->next)
	if (p->id == id) {
	    q->next = p->next;
	    free(p);
	    break;
	}
    *list = tmp.next;
}

/**********************************************************************/

/* pl_lookup: Lookup player by id, return pointer if found.
   If not found, create if asked, return NULL otherwise. */
player *pl_lookup(int id, int create)
{
    int i;

    for (i = 0; i < nplayers; i++)
	if (players[i].id == id)
	    return &(players[i]);

    if (create) {
	players[nplayers].id = id;
	return &(players[nplayers++]);
    } else
	return NULL;
}

/**********************************************************************/

/* gc_mark: Phase I of garbage collection.  Mark all players as dead. */
void gc_mark(void)
{
    int i;

    for (i = 0; i < nplayers; i++)
	players[i].alive = 0;
}

/* gc_sweep: Phase II of garbage collection.  Remove all players that
 *  haven't been confirmed alive since last mark.
 */
void gc_sweep(void)
{
    int i;

    for (i = 0; i < nplayers; i++)
	if (!players[i].alive) {
	    free_packages(&players[i].pkgs, 0);
	    memmove(players + i, players + i + 1, 
		    (nplayers - i - 1) * sizeof(player));
	    nplayers--;
	}
}

/**********************************************************************/

double avg_weight(void)
{
    if (npkgs_seen == 0)
	return self->capacity / 3.0;
    else
	return total_wt_seen / npkgs_seen;
}

double avg_pkgs_in_base(void)
{
    if (nknown_bases == 0)
	return 1;
    else
	return (double) npkgs_seen / (double) nknown_bases;
}
