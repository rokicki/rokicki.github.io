/**
 *   Given a local situation, do a *quick* simulation of just the local
 *   effect of different moves, and use this information to decide what
 *   to do.
 *
 *   The map passed in should contain chars:
 *      . nothing
 *      ~ water
 *      # wall
 *      R robots
 *      U us
 *
 *   In addition, a bitmap of NSEW indicates which of the four directions
 *   get us closer to our goal, and we also get the number of packets
 *   we currently hold.
 *
 *   We run all possible orderings of the R's, with all directions the
 *   R's can participate in and collect under each of our actions the
 *   probability of:
 *
 *      Dying
 *      Killing someone (anyone but us)
 *      Ending up in square X
 *      Dropping N packets
 *
 *   We look for differences.  If there is a clear advantage to a direction,
 *   or to a bidding/direction combination, we use it.
 *   The size of the map is 9x9 (just to pick a value).
 *
 *   If the closest robot is more than two away, we return immediately and
 *   save our energy.
 *
 *   We always select the closest bots.  A parameter tells us if we can
 *   proceed longer or not; if this parameter isn't given, we do at most
 *   3 additional neighbors, else we can do 4.  For a total of N bots,
 *   there are 5^N * N! possible futures that we consider.
 *
 *   Any robot that falls off the edge of the board, simply falls off the
 *   edge of the board.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"
typedef struct frobot frobot ;
/**
 *   We're always the first one.
 */
struct frobot {
   int x, y, d ;
   int pushed ;
   int died ;
   int dir ;
   int idx ;
   int hasmoved ;
   int didwork ;
   int bidorder ;
   int offboard ;
} allrobots[100] ;
int nbots ;
int botlook ;
unsigned char smallbd[9][9] ;
unsigned char closer[9][9] ;
int deathcount ;
struct results {
   int total ;
   int death ;
   int neardeath ;
   int otherdead ;
   int didwork ;
   int pushed ;
   int closer ;
} results[11][5] ;
extern int iabs(int n) ;
extern int verbose ;
void checkbot(int i, int j, int d) {
   if (j < 0 || j > 8 || i < 0 || i > 8)
      return ;
   if (smallbd[i][j] == 'R' || smallbd[i][j] == 'U') {
      frobot *fr = &(allrobots[nbots]) ;
      smallbd[i][j] = 128 + nbots ;
      memset(fr, 0, sizeof(struct frobot)) ;
      fr->x = i ;
      fr->y = j ;
      fr->d = d ;
      fr->idx = nbots++ ;
   }
}
int staticeval() {
   frobot *fr = allrobots ;
   int x = fr->x ;
   int y = fr->y ;
   int n, s, e, w ;
   int r = 0 ;
   if (x < 1 || y < 1 || x > 7 || y > 7)
      return 0 ;
   n = smallbd[x][y+1] ;
   s = smallbd[x][y-1] ;
   e = smallbd[x+1][y] ;
   w = smallbd[x-1][y] ;
   if (n == '~') {
      if (s >= 128)
         r += 5 ;
      else if (s != '~' && s != '#')
         r += 1 ;
   }
   if (s == '~') {
      if (n >= 128)
         r += 5 ;
      else if (n != '~' && n != '#')
         r += 1 ;
   }
   if (e == '~') {
      if (w >= 128)
         r += 5 ;
      else if (w != '~' && w != '#')
         r += 1 ;
   }
   if (w == '~') {
      if (e >= 128)
         r += 5 ;
      else if (e != '~' && e != '#')
         r += 1 ;
   }
   return r ;
}
void addone(int i, int mult, int neardeath) {
   frobot *fr = allrobots ;
   struct results *r = &(results[1+fr->bidorder][i]) ;
   r->total += mult ;
   if (fr->died) {
      r->death += mult ;
   } else {
      r->neardeath += neardeath * mult ;
      r->otherdead += deathcount * mult ;
      r->didwork += fr->didwork * mult ;
      r->pushed += mult * fr->pushed ;
      if (closer[fr->x][fr->y])
         r->closer += mult ;
   }
}
void report(int mult) {
   int i ;
   int v2 = staticeval() ;
   frobot *fr = allrobots ;
   i = fr->dir ;
   if (i == 5) {
      for (i=0; i<5; i++)
         addone(i, mult, v2) ;
   } else {
      addone(i, mult, v2) ;
   }
}
int dxa[] = { 0, 0, 1, -1, 0 } ;
int dya[] = { 1, -1, 0, 0, 0 } ;
void dosim(int stilltogo, int weight) {
   int i, j ;
   frobot *fr ;
   if (stilltogo == 0) {
      report(weight) ;
      return ;
   }
   for (i=0; i<botlook; i++) {
      fr = &(allrobots[i]) ;
      if (fr->hasmoved)
         continue ;
      fr->bidorder = stilltogo-1 ;
      fr->hasmoved++ ;
      if (fr->pushed || fr->died || fr->offboard) {
                    // this bot can't do anything, so just call recursion
         fr->dir = 5 ;
         dosim(stilltogo-1, weight * 5) ;
         fr->dir = 0 ;
      } else {
         int d = 0 ;
         fr->didwork++ ;
         for (d=0; d<5; d++) {
            // do we move or do we push?
            // no one will ever choose to go into the water or the wall
            frobot *botseq[20] ;
            int moved = 0 ;
            int dx = dxa[d] ;
            int dy = dya[d] ;
            int x = fr->x ;
            int y = fr->y ;
            int bs = 0 ;
            int cc ;
            fr->dir = d ;
            if (dx + x < 0 || dx + x > 8 || dy + y < 0 || dy + y > 8)
               continue ;
            cc = smallbd[x+dx][y+dy] ;
            if (cc == '#' || cc == '~') {
               if (fr->idx != 0)
                  continue ;
               fr->died++ ; // kill us if we do something stupid
               dosim(stilltogo-1, weight) ;
               fr->died-- ;
               continue ;
            }
            if (dx || dy) {
               while (1) {
                  int c ;
                  if (x < 0 || y < 0 || x > 8 || y > 8) {
                     moved = 1 ;
                     break ;
                  }
                  c = smallbd[x][y] ;
                  if (c == '#')
                     break ;
                  if (c == '~' || c == '.') {
                     moved = 1 ;
                     break ;
                  }
                  c -= 128 ;
                  if (bs != 0)
                     allrobots[c].pushed++ ;
                  botseq[bs++] = &(allrobots[c]) ;
                  x += dx ;
                  y += dy ;
               }
            }
            botseq[bs] = 0 ;
            if (moved) {
               for (j=0; botseq[j]; j++) ;
               for (j--; j>=0; j--) {
                  frobot *or = botseq[j] ;
                  int xx = or->x + dx ;
                  int yy = or->y + dy ;
                  or->x = xx ;
                  or->y = yy ;
                  if (xx < 0 || yy < 0 || xx > 8 || yy > 8) {
                     or->offboard++ ;
                  } else {
                     int c = smallbd[xx][yy] ;
                     if (c == '~') {
                        or->died++ ;
                        deathcount++ ;
                     } else {
                        smallbd[xx][yy] = 128 + or->idx ;
                     }
                  }
               }
               smallbd[fr->x-dx][fr->y-dy] = '.' ;
            }
            dosim(stilltogo-1, weight) ;
            for (j=0; botseq[j]; j++) {
               frobot *or = botseq[j] ;
               if (j != 0) or->pushed-- ;
               if (moved) {
                  int xx = or->x ;
                  int yy = or->y ;
                  or->x = xx - dx ;
                  or->y = yy - dy ;
                  smallbd[or->x][or->y] = 128 + or->idx ;
                  if (xx < 0 || yy < 0 || xx > 8 || yy > 8) {
                     or->offboard-- ;
                  } else {
                     int c = smallbd[xx][yy] ;
                     if (c == '~') {
                        or->died-- ;
                        deathcount-- ;
                     } else {
                        smallbd[xx][yy] = '.' ;
                     }
                  }
               }
            }
            smallbd[fr->x][fr->y] = 128 + fr->idx ;
            fr->d = 0 ;
         }
         fr->didwork-- ;
      }
      fr->hasmoved-- ;
   }
}
int go(int depth) {
   /** fill in allrobots in order of distance */
   int d, i, j ;
   nbots = 0 ;
   deathcount = 0 ;
/*
   for (j=0; j<=8; j++) {
      for (i=0; i<=8; i++) {
         d = smallbd[i][j] ;
         if (d >= 128)
            printf("%c", d - 128 + '0') ;
         else
            printf("%c", d) ;
      }
      printf("\n") ;
   }
 */
   for (d=0; d<=8; d++) {
      for (i=0; i<=8; i++) {
         if (iabs(i-4) > d)
            continue ;
         checkbot(i, 4+(d-iabs(i-4)), d) ;
         if (iabs(i-4) != d)
            checkbot(i, 4-(d-iabs(i-4)), d) ;
      }
   }
   if (nbots < 2 || allrobots[1].d > 5)
      return 0 ;
   if (depth > nbots)
      depth = nbots ;
   botlook = depth ;
   memset(results, 0, sizeof(results[0][0]) * 5 * (depth + 1)) ;
// printf("Cleared %d (%d total)\n", 5*(depth+1), sizeof(results[0][0])*5*(depth+1)) ;
   dosim(depth, 1) ;
   // now accumulate for the different bid orders
   for (i=0; i<5; i++) {
      struct results *rs = &(results[0][i]) ;
      for (j=1; j<=depth; j++) {
         struct results *r1 = &(results[j][i]) ;
// printf("Adding %d to %d for i %d j %d\n", r1->total, rs->total, i, j) ;
         rs->total += r1->total ;
         rs->death += r1->death ;
         rs->neardeath += r1->death ;
         rs->otherdead += r1->otherdead ;
         rs->didwork += r1->didwork ;
         rs->closer += r1->closer ;
         rs->pushed += r1->pushed ;
      }
   }
   return 1 ;
}
char newmovebuf[100000] ;
extern int **dist2 ;
void sumexpo(int dst, int start, int end, int delta) {
   int mm = 1 ;
   int i, j ;
   for (i=0; i<5; i++) {
      struct results *rs = &(results[0][i]) ;
      memset(rs, 0, sizeof(results[0][0])) ;
      for (j=start; ; j += delta, mm *= 2) {
         struct results *r1 = &(results[j][i]) ;
         rs->total += r1->total * mm ;
         rs->death += r1->death * mm ;
         rs->neardeath += r1->death * mm ;
         rs->otherdead += r1->otherdead * mm ;
         rs->didwork += r1->didwork * mm ;
         rs->closer += r1->closer * mm ;
         rs->pushed += r1->pushed * mm ;
         if (j == end)
            break ;
      }
   }
}
double valdiff ;
char *tactics(int deep, int dir, char *move) {
                                   // dir 0..5 for is N, S, E, W, or A (action)
   int x, y, i ;
   int nowdist = dist2[self->y][self->x] ;
   for (x=-4; x<=4; x++) for (y=-4; y<=4; y++) {
      int rx = self->x + x ;
      int ry = self->y + y ;
      if (rx < 1 || ry < 1 || rx > bd.w || ry > bd.h) {
         smallbd[x+4][y+4] = '#' ;
         closer[x+4][y+4] = 0 ;
      } else {
         int c = bd.bd[ry][rx] ;
         if (c == '#' || c == '.' || c == '~') {
            smallbd[x+4][y+4] = c ;
         } else if (c == '@') {
            smallbd[x+4][y+4] = '.' ;
         }
         if (dist2[ry][rx] < nowdist)
            closer[x+4][y+4] = 1 ;
         else
            closer[x+4][y+4] = 0 ;
      }
   }
   for (i=0; i<nplayers; i++) {
      player *p = &(players[i]) ;
      if (p->alive && p->id != self->id) {
         x = p->x - self->x ;
         y = p->y - self->y ;
         if (x >= -4 && x <= 4 && y >= -4 && y <= 4) {
            smallbd[x+4][y+4] = 'R' ;
         }
      }
   }
   smallbd[4][4] = 'U' ;
   if (go(deep ? 5 : 4)) {
      // now we evaluate all of our alternatives
      // and calculate a score for each
      // and pick the one with the best score
      // first we see what we can do without bidding.
      double best = -10000000000000000.0 ;
      int besti = 0 ;
      double loscore, hiscore ;
      int bidlow = 0 ;
      int bid = 0 ;
      struct results *rs ;
#define DEATHCOST (double)(-10000000)
#define KILLCOST  (double)  (1000000)
#define PUSHCOST  (double)   (-10000)
 // either did work if appropriate, or nearer if action is a movement
#define PROGRESS  (double)    (10000)
 // note this has a multiplier; neardeath is kind of like 5x or so
#define NEARDEATH (double)   (-10000)
      valdiff = 10000000000000000.0 ;
      for (i=0; i<5; i++) {
         struct results *rs = &(results[0][i]) ;
         double tscore = rs->death * DEATHCOST +
                         rs->pushed * PUSHCOST +
                         rs->neardeath * NEARDEATH +
                         rs->otherdead * KILLCOST ;
         if (dir == 4) { // action
	    if (i == 4)
	       tscore += rs->didwork * PROGRESS ;
         } else {
            tscore += rs->closer * PROGRESS ;
         }
         tscore /= rs->total ;
         if (dir == i)
            tscore += 100 ; // tiebreaker
// printf("Orig %d this %d death %d pushed %d neardeath %d otherdead %d didwork %d closer %d total %d final %g\n", dir, i, rs->death, rs->pushed, rs->neardeath, rs->otherdead, rs->didwork, rs->closer, rs->total, tscore) ;
         if (tscore > best) {
	    best = tscore ;
            besti = i ;
         }
         if (i == dir)
            valdiff = tscore ;
      }
      rs = &(results[0][besti]) ;
      // can we enhance things through selective bidding?  Do an
      // exponential sum with hi, and lo, to see.
      sumexpo(0, botlook, 1, -1) ;
// printf("Hi %d this %d death %d pushed %d neardeath %d otherdead %d didwork %d closer %d total %d final\n", dir, i, rs->death, rs->pushed, rs->neardeath, rs->otherdead, rs->didwork, rs->closer, rs->total) ;
      loscore = rs->death * DEATHCOST +
                rs->pushed * PUSHCOST +
                rs->neardeath * NEARDEATH +
                rs->otherdead * KILLCOST ;
      if (dir == 4) { // action
         if (besti == 4)
	    loscore += rs->didwork * PROGRESS ;
      } else {
         loscore += rs->closer * PROGRESS ;
      }
      loscore /= rs->total ;
      sumexpo(0, 1, botlook, 1) ;
// printf("Lo %d this %d death %d pushed %d neardeath %d otherdead %d didwork %d closer %d total %d final\n", dir, i, rs->death, rs->pushed, rs->neardeath, rs->otherdead, rs->didwork, rs->closer, rs->total) ;
      hiscore = rs->death * DEATHCOST +
                rs->pushed * PUSHCOST +
                rs->neardeath * NEARDEATH +
                rs->otherdead * KILLCOST ;
      if (dir == 4) { // action
	 if (besti == 4)
	    hiscore += rs->didwork * PROGRESS ;
      } else {
         hiscore += rs->closer * PROGRESS ;
      }
      hiscore /= rs->total ;
//  printf("orig %g best %g hi %g lo %g\n", valdiff, best, hiscore, loscore) ;
      if (hiscore < loscore) {
         bidlow = 1 ;
	 hiscore = loscore ;
      }
      if (verbose && hiscore != best)
         printf("Bidding is worth %g\n", hiscore-best) ;
      if (hiscore > 300000 + best) { // very high bid
	 bid = self->money / 1800 ; 
         if (bid < 10 && self->money >= 10)
	    bid = 10 ;
         valdiff -= hiscore ;
         if (verbose) printf("[Bidding HIGH]\n") ;
      } else if (hiscore > 900 + best) { // high bid
	 bid = self->money / 18000 ; 
         if (bid < 3 && self->money >= 3)
	    bid = 3 ;
         if (verbose) printf("[Bidding high]\n") ;
         valdiff -= hiscore ;
      } else { // normal bid, and do not override
	 bid = 1 ;
         valdiff -= best ;
      }
      valdiff = - valdiff ;
      if (bid == 0)
	 bid = 1 ;
      if (bidlow)
	 bid = - bid ;
      // now we need to build a new move.
      if (besti < 4) { // just move
	 sprintf(newmovebuf, "%d Move %c", bid, "NSEW"[besti]) ;
      } else {
	 if (dir < 4) { // override a move with inaction
	    sprintf(newmovebuf, "%d Pick", bid) ;
         } else { // do the action
            char *p = move ;
            while (*p > ' ') p++ ;
            while (*p && *p <= ' ') p++ ;
            sprintf(newmovebuf, "%d %s", bid, p) ;
         }
      }
      return newmovebuf ;
   } else {
      return 0 ; // move is okay as-is
   }
}
