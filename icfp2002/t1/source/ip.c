/**
 *   Implementation of ip.
 */
#include "client.h"
int *ipdist[MAXIP] ;
ip ips[MAXIP] ;
int nips = 0 ;
static int explortime = 0 ;
extern board bd ;
extern int *adata, *bdata, asize, bsize ;
extern int **update_self_distances() ;
/**
 *   Includes the distance * 4, plus the inverted distance
 *   (say max is 500,000) plus the flag bits in the bottom.
 *   max is 100M!
 *
 *   Note that these priorities here only say whether it goes
 *   in the interesting point list, nothing else.
 *
 *   The low bits reencode PACKDEST and BASE so we can make sure
 *   there are always some of each in the interesting point list.
 */
int baseprior[] = {
   0,                /* nothing interesting */
   50000001,         /* unseen bases */
   0,                /* someone else has visited, so what */
   30000001,         /* a base that someone else has visited */
   0,                /* we visited it, so what */
   20000001,         /* a base that we've already visited */
   0,                /* we've visited, someone else has, so what */
   10000001,         /* we've visited, others have visited ... */
   10000002,         /* a package dest */
   60000003,         /* unseen bases */
   10000002,         /* someone else has visited, so what */
   40000003,         /* a base that someone else has visited */
   10000002,         /* we visited it, so what */
   30000003,         /* a base that we've already visited */
   10000002,         /* we've visited, someone else has, so what */
   20000003          /* we've visited, others have visited ... */
} ;
#define NEARBYBONUS (30000000) // if it's nearby, put it in for sure
#define NEARBYDIST (20)        // nearby is within 20.
int sqpriority(int x, int y) {
   int **mydists = update_self_distances() ;
   square *s = &(bd.sq[y][x]) ;
   int prior = baseprior[s->state & 15] ;
   if (prior == 0) return 0 ;
   if (mydists[y][x] <= NEARBYDIST)
      prior += NEARBYBONUS ;
   return prior + 4 * (500000 - mydists[y][x]) ;
}
static int aptr, bptr, *t ;
static unsigned char **visited ; // more data
int visitmarker = 0 ;
/**
 *   Explore the neighborhood of this interesting point, up until we see
 *   a max of boxes boxes, or finish.
 */
static void explor_dist(ip *ip, int boxes) {
   char **board = bd.bd ;
   int w = bd.w ;
   int h = bd.h ;
   int i, d, c ;
   int togo = boxes ;
   int *dst = ipdist[ip->idx] ;
   square *s ;
   if (ip->total)
      return ; // nothing left to do
   ip->explorboxes = boxes ;
   ip->whenlastexplored = explortime++ ;
   visitmarker++ ;
   if (visitmarker >= 255) {
      visitmarker = 1 ;
      for (i=0; i<h+2; i++)
         memset(visited[i], 0, w + 2) ;
   }
   for (i=0; i<=nips; i++) // undo all our good and holy work
      dst[i] = 1000000000 ;
   aptr = 0 ;
   if (asize == 0) {
      asize = 100 ;
      adata = mymalloc(sizeof(int) * 100) ;
      bsize = 100 ;
      bdata = mymalloc(sizeof(int) * 100) ;
   }
   bptr = 0 ;
   adata[aptr++] = ip->x ;
   adata[aptr++] = ip->y ;
   dst[ip->idx] = 0 ;
   visited[ip->y][ip->x] = visitmarker ;
   for (d=1; aptr>0; d++) {
      for (i=0; i<aptr; i+=2) {
         int x = adata[i] ;
         int y = adata[i+1] ;
         if (bptr + 20 >= bsize) {
            bdata = myrealloc(bdata, (bsize * 2) * sizeof(int)) ;
            bsize *= 2 ;
         } // we can go to '.' and '@' but not '#' or '~'
         x++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip != UNINTERESTING)
               dst[s->ip] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip != UNINTERESTING)
               dst[s->ip] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x++ ;
         y++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip != UNINTERESTING)
               dst[s->ip] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         y -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip != UNINTERESTING)
               dst[s->ip] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
      }
      togo -= aptr / 2 ;
      if (togo <= 0) {
         ip->explordepth = d ;
         return ;
      }
      aptr = bptr ;
      bptr = 0 ;
      i = asize ;
      asize = bsize ;
      bsize = i ;
      t = bdata ;
      bdata = adata ;
      adata = t ;
   }
   ip->explordepth = 1000000 ;
   ip->total = 1 ;
}
void rm_ip(int x, int y) {
   square *s1 = &(bd.sq[y][x]) ;
   int idx = s1->ip ;
   if (idx == UNINTERESTING)
      return ;
   s1->ip = UNINTERESTING ;
   ips[idx].free = 1 ;
}
static int compare(const void *a, const void *b) {
   return *(int *)a - *(int *)b ;
}
/**
 *   Free up some interesting points according to their priority.
 */
void ip_recycle() {
   int prior[MAXIP] ;
   int priors[MAXIP] ;
   int cat[4] ;
   int i, bound, rmed=0 ;
   cat[0] = cat[1] = cat[2] = cat[3] = 0 ;
   printf("[Recycling interesting points]\n") ;
   for (i=1; i<MAXIP; i++) {
      ip *ip = &(ips[i]) ;
      prior[i] = sqpriority(ip->x, ip->y) ;
      cat[prior[i] & 3]++ ;
   }
   if (cat[1] + cat[3] < 2*MAXIP/5) { // keep all cat[1] and cat[3]'s
      for (i=1; i<MAXIP; i++) {
         if (prior[i] & 1)
            prior[i] += 100000000 ;
      }
   }
   if (cat[2] + cat[3] < 2*MAXIP/5) { // keep all cat[2] and cat[3]'s
      for (i=1; i<MAXIP; i++) {
         if (prior[i] & 2)
            prior[i] += 100000000 ;
      }
   }
   memcpy(priors, prior, sizeof(prior)) ;
   qsort(priors+1, MAXIP-1, sizeof(int), &compare) ;
   bound = priors[MAXIP/5] ;
   for (i=1; i<MAXIP; i++)
      if (prior[i] <= bound) {
         ip *ip = &(ips[i]) ;
         rm_ip(ip->x, ip->y) ;
         rmed++ ;
         if (rmed > MAXIP/5)
            return ;
      }
}
int add_ip(int x, int y) {
   square *s1 = &(bd.sq[y][x]) ;
   int idx = 0 ;
   ip *ip ;
   if (s1->ip != UNINTERESTING)
      return s1->ip ;
   if (nips + 1 >= MAXIP) {
      while (1) {
         for (idx=1; idx<MAXIP; idx++)
            if (ips[idx].free)
               break ;
         if (idx >= MAXIP) {
            ip_recycle() ;
         } else
            break ;
      }
   } else {
      idx = ++nips ;
   }
   ips[idx].free = 0 ;
   if ((nips & (nips - 1)) == 0) {
      printf("[%d Interesting Points]\n", nips) ;
   }
   s1->ip = idx ;
   ip = &(ips[idx]) ;
   ip->x = x ;
   ip->y = y ;
   ip->s = s1 ;
   ip->total = 0 ;
   ip->explordepth = 0 ;
   ip->explorboxes = 0 ;
   explor_dist(ip, INITEXPLORBOXES) ;
   return idx ;
}
/**
 *   Check out the immediate neighborhood of self and add any new
 *   interesting squares.  We only need to do this once in a while;
 *   we'll let the main strategy determine when and how deep to go.
 *
 *   We visit at most maxboxes squares, and we add at most
 *   MAXIP/5 new interesting points.  Yet another bfs implementation.
 *
 *   This might be marginally expensive, so we don't want to call it
 *   too much.
 */
int newpts[MAXIP] ;
void visit_neighborhood(int togo) {
   char **board = bd.bd ;
   int w = bd.w ;
   int h = bd.h ;
   int i, d, c ;
   square *s ;
   int newptr = 0 ;
   visitmarker++ ;
   if (visitmarker >= 255) {
      visitmarker = 1 ;
      for (i=0; i<h+2; i++)
         memset(visited[i], 0, w + 2) ;
   }
   aptr = 0 ;
   if (asize == 0) {
      asize = 100 ;
      adata = mymalloc(sizeof(int) * 100) ;
      bsize = 100 ;
      bdata = mymalloc(sizeof(int) * 100) ;
   }
   bptr = 0 ;
   adata[aptr++] = self->x ;
   adata[aptr++] = self->y ;
   visited[self->y][self->x] = visitmarker ;
   for (d=1; aptr>0; d++) {
      for (i=0; i<aptr; i+=2) {
         int x = adata[i] ;
         int y = adata[i+1] ;
         if (bptr + 20 >= bsize) {
            bdata = myrealloc(bdata, (bsize * 2) * sizeof(int)) ;
            bsize *= 2 ;
         } // we can go to '.' and '@' but not '#' or '~'
         x++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip == UNINTERESTING && sqpriority(x, y) > 0) {
               newpts[newptr++] = x ;
               newpts[newptr++] = y ;
            }
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip == UNINTERESTING && sqpriority(x, y) > 0) {
               newpts[newptr++] = x ;
               newpts[newptr++] = y ;
            }
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x++ ;
         y++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip == UNINTERESTING && sqpriority(x, y) > 0) {
               newpts[newptr++] = x ;
               newpts[newptr++] = y ;
            }
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         y -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && visited[y][x] != visitmarker) {
            visited[y][x] = visitmarker ;
            s = &(bd.sq[y][x]) ;
            if (s->ip == UNINTERESTING && sqpriority(x, y) > 0) {
               newpts[newptr++] = x ;
               newpts[newptr++] = y ;
            }
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         togo-- ;
         if (togo < 0 || newptr + 8 >= MAXIP)
            break ;
      }
      if (togo < 0 || newptr + 8 >= MAXIP)
         break ;
      aptr = bptr ;
      bptr = 0 ;
      i = asize ;
      asize = bsize ;
      bsize = i ;
      t = bdata ;
      bdata = adata ;
      adata = t ;
   }
   for (i=0; i<newptr; i+=2) {
      add_ip(newpts[i], newpts[i+1]) ;
   }
}
/**
 *   Spend some time improving our distance estimates.  We explore
 *   a total of approximately 100,000 boxes for each call.
 */
int plan_ip(void) {
   int togo = 100000 ;
   int done = 0 ;
   while (togo > 0) {
      int i ;
      int minexplored = 1000000000 ;
      for (i=1; i<=nips; i++)
         if (ips[i].total == 0 && ips[i].free == 0 &&
             ips[i].explorboxes < minexplored) {
            minexplored = ips[i].explorboxes ;
         }
      if (minexplored == 1000000000)
         return done ;
      done++ ;
      for (i=1; i<=nips; i++)
         if (ips[i].total == 0 && ips[i].free == 0 &&
             ips[i].explorboxes == minexplored) {
            explor_dist(&(ips[i]), minexplored * 2) ;
            // the 4000 is the worst-case amortized cost of reclearing visited
            togo -= minexplored * 2 - 4000 ;
            if (togo <= 0)
               return done ;
         }
   }
   return done ;
}
int ip_dist(int x1, int y1, int x2, int y2) {
   square *s1 = &(bd.sq[y1][x1]) ;
   square *s2 = &(bd.sq[y2][x2]) ;
   int m=0, r = 0 ;
   if (s1->ip != UNINTERESTING && s2->ip != UNINTERESTING) {
      ip *i1 = &(ips[s1->ip]) ;
      ip *i2 = &(ips[s2->ip]) ;
      if (i2->whenlastexplored > i1->whenlastexplored) {
         ip *t = i1 ;
         i1 = i2 ;
         i2 = t ;
      }
      r = ipdist[i1->idx][i2->idx] ;
      if (r <= i1->explordepth) {
         return r ; // valid!
      }
      r = i1->explordepth + 1 ; // has to be greater
   }
   /**
    *   At this point r is a lower bound.  Compare it with the
    *   manhattan distance.
    */
   m = iabs(x1-x2) + iabs(y1-y2) ;
   if (m > r) {
      return m ;
   } else {
      return r ;
   }
}
void ip_init() {
   int i, j ;
   if (bd.h == 0 || bd.w == 0)
      die("Call us *after* the board has been read.") ;
   for (i=0; i<MAXIP; i++) {
      ipdist[i] = mymalloc(sizeof(int) * MAXIP) ;
      ips[i].idx = i ;
      ips[i].free = 1 ;
   }
   visited = mymalloc((bd.h + 2) * sizeof(char *)) ;
   for (i=0; i<bd.h + 2 ; i++)
      visited[i] = mymalloc((bd.w + 2) * sizeof(char)) ;
   nips = 0 ;
   for (i=1; i<=bd.h; i++) for (j=1; j<=bd.w; j++) {
      if (bd.bd[i][j] == '@')
         add_ip(j, i) ;
   }
}
