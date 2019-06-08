/**
 *   A simple less oblivious client.
 *      Greedily finds packages
 *      Greedily carries them to best place
 *      No great brains.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "client.h"
#include "util.h"
#include "dist.h"
#include "pqueue.h"

int verbose ;
extern int movespersecond ;

/**********************************************************************/
void lessobliv(FILE*) ;
void print_board(void) ;
FILE *savefile ;
int main(int argc, char **argv) {
    char *dest_host;
    int dest_port;
    FILE *fp;
    while (argc > 1 && argv[1][0] == '-') {
       argc-- ;
       argv++ ;
       switch (argv[0][1]) {
case 'v':
          verbose++ ;
          break ;
case 'm':
          argc-- ;
          argv++ ;
          if (sscanf(argv[0], "%d", &movespersecond) != 1)
             die("Bad moves per second parameter") ;
          break ;
case 's':
          argc-- ;
          argv++ ;
          savefile = fopen(argv[0], "w") ;
          if (savefile == 0)
             die("Can't open savefile") ;
          fprintf(savefile, "Player\n") ;
          break ;
default:
          die("Bad argument") ;
       }
    }
    if (argc != 3) {
	printf("usage: %s host port\n", argv[0]);
	exit(1);
    }
    
    dest_host = argv[1];
    dest_port = atoi(argv[2]);
    fp = tcpconnect(dest_host, dest_port);
    assert(fp != NULL);
    lessobliv(fp);
    
    return 0;
}
int **dist, **dist2 ;
int dist2x, dist2y ;
int dist1x, dist1y ;
int **update_self_distances() {
   if (dist1x != self->x || dist1y != self->y) {
      calcdist(bd.bd, dist, bd.w, bd.h, self->x, self->y) ;
      dist1x = self->x ;
      dist1y = self->y ;
   }
   return dist ;
}
void setupctrs(board *bd) {
   int i ;
   dist = mymalloc(sizeof(int *) * (bd->h + 2)) ;
   dist2 = mymalloc(sizeof(int *) * (bd->h + 2)) ;
   for (i=0; i<bd->h+2; i++) {
      dist[i] = mymalloc(sizeof(int) * (bd->w + 2)) ;
      dist2[i] = mymalloc(sizeof(int) * (bd->w + 2)) ;
   }
}
int carryingweight() {
   package *pkg ;
   int holding = 0 ;
   for (pkg=self->pkgs; pkg; pkg = pkg->next)
      holding += pkg->weight ;
   return holding ;
}
char movebuf[100000] ;
/**
 *   Anything good to drop here?
 */
char *checkdrop(package *myp, int x, int y) {
   char *p = 0 ;
   int gotsome = 0 ;
   sprintf(movebuf, "1 Drop") ;
   p = movebuf + strlen(movebuf) ;
   while (myp != 0) {
      if (myp->destx == x && myp->desty == y) {
 if (verbose) printf("Dropping package at %d %d \n", myp->destx, myp->desty) ;
	 sprintf(p, " %d", myp->id) ;
         p += strlen(p) ;
         gotsome++ ;
      }
      myp = myp->next ;
   }
   if (gotsome) {
      clearcache() ;
      return movebuf ;
   } else {
      return 0 ;
   }
}
/**
 *   Under the best of circumstances, can we deliver this package?
 */
int reachable(package *pack) {
   return (dist[pack->desty][pack->destx] + 2 <= self->money &&
           pack->weight <= self->capacity) ;
}
/**
 *   Make it faster:  cache the destination.
 */
int mydestx, mydesty ;
void clearcache() {
   mydestx = mydesty = 0 ;
}
int cachecleared() {
   return (mydestx == 0 && mydesty == 0) ;
}
/**
 *   Anything good to pick up here?  Better than what we have?
 *   We may have to drop first.  We do this in a totally greedy
 *   fashion.   We find the *best* set of packages we could
 *   be carrying, preferring what we are carrying for ties,
 *   and see if it's any different.
 *
 *   For now we just build an array of the deliverable packages,
 *   and sort by distance.  We compare total weights.
 *   
 */
package *considerme[10001] ;
int nconsidered = 0 ;
int cmprpkg(const void *a, const void *b) {
   package *aa = *(package **)a ;
   package *bb = *(package **)b ;
   return dist[aa->desty][aa->destx] - dist[bb->desty][bb->destx] ;
}
char *checkpick(package *avail, package *myp, int x, int y) {
    char *p = 0 ;
    int carrying = 0 ;
    int gotsome = 0 ;
    int liftable = 0 ;
    package *pack ;
    double myval = 0.0 ;
    nconsidered = 0 ;
    sprintf(movebuf, "1 Pick") ;
    p = movebuf + strlen(movebuf) ;
    for (pack=myp; pack; pack = pack->next) {
       carrying += pack->weight ;
       considerme[nconsidered++] = pack ;
       pack->state |= MYPACK ;
       myval += pack->weight / ((double)dist[pack->desty][pack->destx] + 2) ;
 if (verbose) printf(" %d", pack->weight) ;
    }
 if (verbose) printf(" New") ;
    for (pack=avail; pack; pack = pack->next) {
       if (reachable(pack)) {
          liftable += pack->weight ;
          considerme[nconsidered++] = pack ;
          pack->state &= ~MYPACK ;
 if (verbose) printf(" %d", pack->weight) ;
      }
   }
 if (verbose) printf("]\n") ;
   if (liftable + carrying > self->capacity) {
      double addval = myval ;
      double dropaddval = 0.0 ;
      int addleft = self->capacity - carrying ;
      int dropaddleft = self->capacity ;
      int lastdrop = 0 ;
      int addedpacks = 0 ;
      int i, j, x, y ;
      int lookahead = 0 ;
      int nip_added = 0 ;
      int d ;
      package *tmp ;

      // can't pick them all up!
      update_self_distances() ;
      qsort(considerme, nconsidered, sizeof(package *), &cmprpkg) ;
      if (verbose) {
         printf("[!!! Got %d packages !!!]\n", nconsidered) ;
         for (i=0; i<nconsidered; i++) {
            printf(" %d", dist[considerme[i]->desty][considerme[i]->destx]) ;
         }
         printf("\n") ;
      }

      /**
       * Perturb the order of the packages so that we prefer close by
       * destinations to the prior one as opposed to just close to our
       * current location.
       * Total work we want to do is 10000, so we only lookahead 10000/n
       * packages in the array.
       * Note, ip_dist() is used to compute distances, so up to 20
       * distance points will be ip_add()'ed.
       */
      if (nconsidered > 0)
	  lookahead = 10000 / nconsidered;

      for (i = 0; i < nconsidered; i++) {
	  x = considerme[i]->destx;  y = considerme[i]->desty;
	  if (nip_added < 20
	      && bd.sq[y][x].ip == UNINTERESTING) {
	      add_ip(x, y);
	      nip_added++;
	  }
      }

      for (i = 1; i < nconsidered; i++) {
	  x = considerme[i-1]->destx;  y = considerme[i-1]->desty;
	  d = ip_dist(x, y, considerme[i]->destx, considerme[i]->desty);
	  for (j = i + 1; j < nconsidered && j <= i + lookahead; j++) {
	      /* Is [j] closer to [i-1] that [i]? */
	      if (ip_dist(x, y, considerme[j]->destx, considerme[j]->desty)
		  < d) {
		  d = ip_dist(x, y,
			      considerme[j]->destx, considerme[j]->desty);
		  /* swap i and j */
		  tmp = considerme[i];
		  considerme[i] = considerme[j];
		  considerme[j] = tmp;
	      }
	  }
      }

      if (verbose) {
         printf("[!!! Reordered %d packages !!!]\n", nconsidered) ;
         for (i=0; i<nconsidered; i++) {
            printf(" %d", dist[considerme[i]->desty][considerme[i]->destx]) ;
         }
         printf("\n") ;
      }

      /**
       *   We want the close by ones, and then the small ones.  We also
       *   want the ones close by the ones close by.  But we only consider
       *   dropping if there's a significant improvement to be had over
       *   just adding.
       */
      for (i=0; i<nconsidered; i++) {
         pack = considerme[i] ;
         if (dropaddleft >= pack->weight) {
            dropaddleft -= pack->weight ;
            dropaddval +=
                      pack->weight / ((double)dist[pack->desty][pack->destx] + 2) ;
            lastdrop = i + 1 ;
         }
         if ((pack->state & MYPACK) == 0 && addleft >= pack->weight) {
            addleft -= pack->weight ;
            addval += pack->weight / ((double)dist[pack->desty][pack->destx] + 2) ;
            addedpacks++ ;
         }
      }
      if (dropaddval > addval * 1.3) {
         // only drop the ones that we didn't add.
         strcpy(movebuf, "1 Drop") ;
         p = movebuf + strlen(movebuf) ;
         for (i=lastdrop; i<nconsidered; i++) {
            if (considerme[i]->state & MYPACK) {
               sprintf(p, " %d", considerme[i]->id) ;
               gotsome++ ;
               p += strlen(p) ;
            }
         }
         p = movebuf ;
      } else if (addedpacks) {
         strcpy(movebuf, "1 Pick") ;
         p = movebuf + strlen(movebuf) ;
         addleft = self->capacity - carrying ;
         for (i=0; i<nconsidered; i++) {
            pack = considerme[i] ;
            if ((pack->state & MYPACK) == 0 && addleft >= pack->weight) {
               addleft -= pack->weight ;
               sprintf(p, " %d", pack->id) ;
               gotsome++ ;
               p += strlen(p) ;
            }
         }
         p = movebuf ;
      } else {
         p = 0 ;
      }
    } else {
      strcpy(movebuf, "1 Pick") ;
      p = movebuf + strlen(movebuf) ;
      for (pack=avail; pack; pack = pack->next) {
         if (reachable(pack)) {
            sprintf(p, " %d", pack->id) ;
            gotsome++ ;
            p += strlen(p) ;
         }
      }
      p = movebuf ;
    }
    for (pack=myp; pack; pack = pack->next) {
       pack->state &= ~MYPACK ;
    }
    if (p && gotsome) {
       clearcache() ;
       return p ;
    } else {
       return 0 ;
    }
}
char *dir = "NSEW" ;
int dx[] = { 0, 0, 1, -1 } ;
int dy[] = { 1, -1, 0, 0 } ;
char *randommove() {
   int x = self->x ;
   int y = self->y ;
   int i ;
   int mydir = -1 ;
   int nseen = 0 ;
   for (i=0; i<4; i++) {
      int nx = x + dx[i] ;
      int ny = y + dy[i] ;
      char c = bd.bd[ny][nx] ;
      if (c == '.' || c == '@') {
         nseen++ ;
         if (nseen == 1 || mrand(nseen) == 0)
            mydir = i ;
      }
   }
   if (mydir == -1)
      return "1 Pick" ;
   sprintf(movebuf, "1 Move %c", dir[mydir]) ;
   return movebuf ;
}
/**
 *   Find the best place to go to.
 */
char *findbest(package *orig_packages) {
   int bestd = 1000000000 ;
   int bestweight = 0 ;
   package *bestp = 0 ;
   int i, x, y ;
   int ngoodseen = 0 ;
   package *packages = orig_packages ;
   if (mydestx > 0 && mydesty > 0) {
      x = mydestx ;
      y = mydesty ;
      bestd = 1000 ; // signal cached with this.
   } else {
      update_self_distances() ;
      for (; packages; packages = packages->next) {
         if (packages->weight * (double)bestd >
             bestweight * (double)dist[packages->desty][packages->destx]) {
            bestp = packages ;
            bestweight = bestp->weight ;
            bestd = dist[packages->desty][packages->destx] ;
         }
      }
      if (bestp == 0) {
         fprintf(stderr, "Internal error: no good destination???\n") ;
         for (packages=orig_packages; packages; packages = packages->next) {
            fprintf(stderr, "Holding package weight %d dist %d\n",
                  packages->weight, dist[packages->desty][packages->destx]) ;
         }
         return randommove() ;
      }
      x = bestp->destx ;
      y = bestp->desty ;
   }
   mydestx = x ;
   mydesty = y ;
 if (verbose) printf("Our target (to drop off) is %d %d dist %d\n", x, y, bestd) ;
   // now find the best way to get there
   if (dist2x != x || dist2y != y) {
      calcdist(bd.bd, dist2, bd.w, bd.h, x, y) ;
      dist2x = x ;
      dist2y = y ;
   }
   for (i=0; i<4; i++) {
      int nx = self->x + dx[i] ;
      int ny = self->y + dy[i] ;
      if (bd.bd[ny][nx] == PLAIN || bd.bd[ny][nx] == BASE) {
         if (dist2[ny][nx] < dist2[self->y][self->x]) {
            ngoodseen++ ;
            if (ngoodseen == 1 || mrand(ngoodseen) == 0)
               sprintf(movebuf, "1 Move %c", dir[i]) ;
         }
      }
   }
   if (ngoodseen)
      return movebuf ;
   fprintf(stderr, "Can't reach destination; cur dist is %d: ",
                                                dist2[self->y][self->x]) ;
   fprintf(stderr, "Trying to go from %d %d to %d %d\n", self->x, self->y,
                                                                   x, y) ;
   for (i=0; i<4; i++) {
      int nx = self->x + dx[i] ;
      int ny = self->y + dy[i] ;
      fprintf(stderr, "%c: %c ", dir[i], bd.bd[ny][nx]) ;
      if (bd.bd[ny][nx] == PLAIN || bd.bd[ny][nx] == BASE) {
         fprintf(stderr, "%d ", dist2[ny][nx]) ;
      }
   }
   fprintf(stderr, "\nInternal error; can't reach destination?\n") ;
   return randommove() ;
}
int packetdest[MAXIP] ;
char *gotoxy(int x, int y) {
   int i ;
   int ngoodseen = 0 ;
   if (verbose && cachecleared()) {
      square *s = &(bd.sq[y][x]) ;
      printf("[Destination from %d %d now %d %d state %d hi %d lo %d pkgs->state %d pkgdst %d]\n", self->x, self->y, x, y, s->state, s->hi, s->lo, s->pkgs ? s->pkgs->state : -1, s->ip ? packetdest[s->ip] : 0) ;
      if (s->pkgs) {
          printf("Distance to package destination: %d\n",
                dist[s->pkgs->desty][s->pkgs->destx]) ;
          printf("Weight of package %d our capacity %d holding %d\n",
                s->pkgs->weight, self->capacity, carryingweight()) ;
      }
   }
   mydestx = x ;
   mydesty = y ;
 if (verbose) printf("Our target (after planning) is %d %d\n", x, y) ;
   // now find the best way to get there
   if (dist2x != x || dist2y != y) {
      calcdist(bd.bd, dist2, bd.w, bd.h, x, y) ;
      dist2x = x ;
      dist2y = y ;
   }
   for (i=0; i<4; i++) {
      int nx = self->x + dx[i] ;
      int ny = self->y + dy[i] ;
      if (bd.bd[ny][nx] == PLAIN || bd.bd[ny][nx] == BASE) {
         if (dist2[ny][nx] < dist2[self->y][self->x]) {
            ngoodseen++ ;
            if (ngoodseen == 1 || mrand(ngoodseen) == 0)
               sprintf(movebuf, "1 Move %c", dir[i]) ;
         }
      }
   }
   if (ngoodseen)
      return movebuf ;
   fprintf(stderr, "Can't reach destination; cur dist is %d: ",
                                                dist2[self->y][self->x]) ;
   fprintf(stderr, "Trying to go from %d %d to %d %d\n", self->x, self->y,
                                                                   x, y) ;
   for (i=0; i<4; i++) {
      int nx = self->x + dx[i] ;
      int ny = self->y + dy[i] ;
      fprintf(stderr, "%c: %c ", dir[i], bd.bd[ny][nx]) ;
      if (bd.bd[ny][nx] == PLAIN || bd.bd[ny][nx] == BASE) {
         fprintf(stderr, "%d ", dist2[ny][nx]) ;
      }
   }
   fprintf(stderr, "\nInternal error; can't reach destination?\n") ;
   return randommove() ;
}
void addippacks(package *p) {
   while (p) {
      add_ip(p->destx, p->desty) ;
      p = p->next ;
   }
}
/**
 *   Here we try to determine what destination we should shoot for.  This
 *   is only generally called if there is a change in plans due to some
 *   event.
 *
 *   In general, look at what we're carrying and decide where to go.
 *
 *   We do this by only considering the set of interesting points as
 *   possible destinations.  We try to find the sequence of these
 *   interesting points that maximizes (value/length) where length is
 *   the length of the total sequence and value is the total value.
 *
 *   First we assign a first-order value to each destination based on
 *   what we're carrying and what we know about each node.  We enter
 *   them into a priority queue.
 *
 *   We then loop.  For each iteration we:
 *
 *      Find max.  If it's better than max so far, remember it.
 *      Check the time.  If we're out of time, exit.
 *      Try all possible ways of extending this sequence, adding
 *         them to the priority queue.  In order to do this we
 *         need to "resimulate" the sequence, and calculate a new
 *         value for each extension . . .
 *         If we find a better one here we remember this one.
 *      Repeat.
 *
 *   Once we run out of moveext, we continue the search, but we
 *   don't add any further entries to the move queue.
 */
/** this is the deepest we can search must be heapsize and > MAXIP **/
#define MAXEXT (HEAPSIZE)
typedef struct moveext moveext ;
struct moveext {
   struct pqelement pq ;
   int capacity ;
   int x, y, len ; // position
   double value ;
   int idx ;
   struct moveext *prev ;
} ;
#define KNOWLEDGE (20)
#define HOLDING (30)
#define DELIVERY (50)
moveext moveexts[MAXEXT] ;
double ipv[MAXIP] ;
double packetsum[MAXIP] ;
/**
 *   How much is this base worth?  If we've visited, we know pretty much
 *   exactly (sum of remaining packages).  If we haven't, then we
 *   approximate it.  If we know but some else has visited it after
 *   us, we have to devalue that base.
 *
 *   There are four types of packages:  unknown (guess the weight),
 *   seen (we know the weight, and it wasn't dropped), DROPPED
 *   (someone dropped it, maybe delivered), and unknown/dropped.
 */
extern int npkgs_seen;        /* Packages we know about */
extern double total_wt_seen;  /* Their total weight, for computing averages */
extern int nbases;            /* total number of bases */
extern int nknown_bases;       /* Number of bases we know about */
double avgpkgweight() {
   if (npkgs_seen > 0)
      return total_wt_seen / npkgs_seen ;
   else
      return self->capacity / 3.0 ;
}
double avgpktsperbase() {
   if (nknown_bases == 0)
      return 3.0 ;
   else if (npkgs_seen == 0)
      return 2.0 ;
   else
      return npkgs_seen / (double)nknown_bases ;
}
double realsum ;
double baseval(int x, int y, square *s, int cancarry) {
   package *pkg ;
   double totweight = 0.0 ;
   double apw = avgpkgweight() ;
   realsum = 0.0 ;
   if (s->hi == 0) // allow hi of 0 to kill a base.
      return 0 ;
   if (s->state & ST_BASE) {
      if (s->state & ST_VISITED) { // visited, we know what packets
         for (pkg = s->pkgs ; pkg; pkg = pkg->next) {
            if (pkg->weight < 0) {
               if (apw < cancarry) {
                  totweight += apw ;
                  cancarry -= apw ;
               }
               realsum += apw ;
            } else if (pkg->weight <= cancarry) {
               totweight += pkg->weight ; // only what we can carry now
               cancarry -= pkg->weight ;
               realsum += pkg->weight ;
            } else {
               realsum += pkg->weight ;
            }
         }
      } else {
         totweight = avgpktsperbase() * avgpkgweight() ;
         if (totweight > cancarry)
            totweight = cancarry ;
         if (s->state & ST_DIRTY)
            totweight /= 10.0 ;
         realsum += cancarry ;
      }
   } else if (s->hi > 0) { // base created by *someone*
      if (s->pkgs) {
         for (pkg = s->pkgs ; pkg; pkg = pkg->next) {
            if (pkg->weight < 0) {
               totweight += avgpkgweight() ;
               realsum += avgpkgweight() ;
            } else if (pkg->weight <= cancarry) {
               totweight += pkg->weight ; // only what we can carry now
               cancarry -= pkg->weight ;
               realsum += pkg->weight ;
            } else {
               realsum += pkg->weight ;
            }
         }
      } else {
         totweight = avgpkgweight() / 10.0 ;
         realsum = totweight ;
      }
   }
   return totweight ;
}
char *planmove() {
   moveext *mptr ;
   int holding = 0 ;
   int nused = 0 ;
   double bestvalue ;
   int bestlen = -1 ;
   package *pkg ;
   moveext *best = 0 ;
   square *s ;
   int i ;
   int ipc = 0 ;
   ip *ip ;
   if (mydestx != 0 && mydesty != 0)
      return gotoxy(mydestx, mydesty) ;
   if (verbose) printf("[Planning moves . . . %d secs]\n", exterseconds()) ;
   update_self_distances() ;
   if (nips == 0)
      return randommove() ;
   memset(packetdest, 0, sizeof(packetdest)) ;
   for (pkg=self->pkgs; pkg; pkg = pkg->next) {
      int idx = bd.sq[pkg->desty][pkg->destx].ip ;
      holding += pkg->weight ;
      if (idx != UNINTERESTING)
         packetdest[idx] += pkg->weight ;
   }
   for (i=1; i<=nips; i++) {
      ipv[i] = 0 ;
      ip = &(ips[i]) ;
      if (ip->free)
         continue ;
      if (dist[ip->y][ip->x] >= 1000000) // never consider unreachables
         continue ;
      s = &(bd.sq[ip->y][ip->x]) ;
      ipv[i] = packetdest[i] * DELIVERY ;
 //printf("Looking at IP %d (%d %d) s->hi %d pkgs %x\n", i, ip->x, ip->y, s->hi, s->pkgs) ;
      if (s->hi > 0) {
         double pkgweight = baseval(ip->x, ip->y, s, self->capacity-holding) ;
         packetsum[i] = realsum ;
         if ((s->state & ST_VISITED) == 0)
            ipv[i] += KNOWLEDGE * pkgweight ;
         else
            ipv[i] += 0.1 ; // okay, give it something
      } else {
         packetsum[i] = 0 ;
      }
      if (ipv[i] > 0.0)
         ipc++ ;
   }
   if (verbose) printf("[%d interesting points]\n", ipc) ;
   heap_init() ;
   mptr = moveexts ;
   mptr->pq.priority = 0.0 ;
   mptr->prev = 0 ;
   mptr->capacity = self->capacity - holding ;
   mptr->x = self->x ;
   mptr->y = self->y ;
   mptr->value = 0.0 ;
   mptr->len = 1 ;
   heap_insert(&(mptr->pq)) ;
   mptr++ ;
   nused++ ;
   while (1) {
      moveext *this = (moveext *)heap_max() ;
      if (this == 0)
         break ;
      for (i=1; i<=nips; i++) {
         int ddist = 0 ;
         moveext *mp ;
         ip = &(ips[i]) ;
         if (ip->free)
            continue ;
         if (ipv[i] == 0.0)
            continue ;
         // if this is the second time, we don't do it
         for (mp=this; mp; mp = mp->prev)
            if (mp->idx == i)
               break ;
         if (mp)
            continue ;
         ip = &(ips[i]) ;
         if (this->x == self->x && this->y == self->y)
            ddist = dist[ip->y][ip->x] ;
         else
            ddist = ip_dist(this->x, this->y, ip->x, ip->y) ;
         if (ddist == 0)
            continue ;
         mptr->prev = this ;
         mptr->x = ip->x ;
         mptr->y = ip->y ;
         mptr->len = this->len + ddist ;
         mptr->value = this->value + ipv[i] ;
         // how much can we carry
         if (packetsum[i] > 0.0) {
            if (this->capacity > packetsum[i]) {
               mptr->capacity -= packetsum[i] ;
               mptr->value += HOLDING * packetsum[i] ;
            } else {
               mptr->value += HOLDING * mptr->capacity ;
               mptr->capacity = 0 ;
            }
         } else {
            mptr->capacity = this->capacity ;
         }
         if (best == 0 || mptr->value / mptr->len >
                          best->value / best->len + 0.001) {
            best = mptr ;
            if (best && best->prev && best->prev->prev)
               best = best->prev ;
            bestvalue = mptr->value ;
            bestlen = mptr->len ;
         }
         if (nused < MAXEXT + 2) {
            nused++ ;
            mptr->pq.priority = mptr->value / mptr->len ;
            heap_insert((pqelement *)mptr) ;
            mptr++ ;
         } else {
            // don't do anything; just keep iterating
         }
      }
      if (exterseconds() < 1)
         break ;
   }
   while (best && best->prev && best->prev->prev)
      best = best->prev ;
   if (best == 0 || best->prev == 0) {
      return randommove() ;
   } else {
      return gotoxy(best->x, best->y) ;
   }
}
void setunreachables() {
   int x, y ;
   for (y=0; y<bd.h+2; y++) for (x=0; x<bd.w+2; x++)
      if (dist[y][x] >= 1000000)
         bd.sq[y][x].state |= ST_UNREACHABLE ;
}
int dirparsemove(char *p) {
   while (*p > ' ') p++ ;
   while (*p && *p <= ' ') p++ ;
   if (strncmp(p, "Move ", 5) == 0) {
      p += 5 ;
      if (*p == 'N') return 0 ;
      if (*p == 'S') return 1 ;
      if (*p == 'E') return 2 ;
      if (*p == 'W') return 3 ;
      return 4 ;
   }
   return 4 ;
}
extern char *tactics(int deep, int dir, char *mov) ;
extern double valdiff ;
void lessobliv(FILE *fp)
{
    init(fp, &bd);
    setupctrs(&bd) ;
    update_self_distances() ;
    setunreachables() ;
    ip_init() ;
    while (!feof(fp)) {
        package *avail = get_packages(fp) ;
	package *mypackages = 0 ;
        char *move = 0 ;
        char *override = 0 ;
        addippacks(avail) ;
	if (verbose) print_board();
	mypackages = self->pkgs ;
        // if the cache was cleared, we need to plan.
        if (cachecleared() || exterseconds() > 20) {
           int tl ;
           if (exterseconds() > 5) // anything nearby useful?
              visit_neighborhood(2000) ;
           tl = exterseconds() ;
           // leave at least five seconds, and at least 1/3 of the total time
           if (tl > 9)
              tl = 2 * tl / 3 ;
           else
              tl = 5 ;
           while (exterseconds() > tl) { // extend the distance information
              if (plan_ip() == 0)
                 break ;
           }
        }
        // first try to drop scoring packages
        move = checkdrop(mypackages, self->x, self->y) ;
        // next try to pick up a better selection of packages.
        // this might mean we drop some.  We'll see.
        if (move == 0 && avail != 0) {
	   move = checkpick(avail, mypackages, self->x, self->y) ;
        }
        if (move == 0) {
           if (self->x == mydestx && self->y == mydesty) {
              // got here, turns out it's pretty useless, try again
              bd.sq[self->y][self->x].state = 0 ;
              clearcache() ;
           }
           move = planmove() ;
        }
        if (move == 0)
           move = "1 Pick" ;
        if (nplayers != 1) {
           override = tactics(exterseconds() > 2, dirparsemove(move), move) ;
           if (override != 0 && strcmp(override, move) != 0) {
	      if (verbose)
                 printf("Overriding move %d '%s' with '%s' for %g\n",
                                    move_count, move, override, valdiff) ;
              move = override ;
           }
        }
	if (verbose) { fprintf(stdout, "'%s'\n", move); fflush(stdout); }
        if (savefile) {
           fprintf(savefile, "%s\n", move) ;
           fflush(savefile) ;
        }
	fprintf(fp, "%s\n", move); fflush(fp);
	get_response(fp, 0);
    }
}
static char **grid ;
void print_board(void)
{
    int r, c, i;
    int printdist = (self->x == dist1x && self->y == dist1y) ;
    printf("Robot is at %d %d\n", self->x, self->y) ;
    if (grid == 0) {
       grid = mymalloc(sizeof(char *) * (bd.h + 2)) ;
       for (r=0; r<bd.h+2; r++)
          grid[r] = mymalloc(sizeof(char) * (bd.w + 2)) ;
    }
    for (r = 1; r <= bd.h; r++) {
       for (c = 1; c <= bd.w; c++) {
         int d = dist[r][c] ;
         grid[r-1][c-1] = bd.bd[r][c];
         if (printdist) {
            if (d < 10)
               printf("%d", dist[r][c]) ;
            else if (d < 1000000)
               printf("!") ;
            else
               printf("X") ;
          }
       }
       grid[r-1][c-1] = '\0';
       if (printdist) printf("\n") ;
    }
    for (i = 0; i < nplayers; i++)
	if (players[i].x && players[i].y)	    
	    grid[players[i].y-1][players[i].x-1] = '0' + players[i].id % 10;
    putchar('\n');
    for (r = 0; r < bd.h; r++) {
	puts(grid[r]);
    }
    putchar('\n');
}
void drop_hook(int x, int y) {
   add_ip(x, y) ;
}
