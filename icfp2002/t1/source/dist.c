/**
 *   Given a ***char, ***int, a w, a h, and an x/y location, fills in the
 *   distances for all points.  Time is linear in the number of
 *   positions.
 */
#include "client.h"
int asize, bsize ;
int *adata, *bdata ;
static int aptr, bptr, *t ;
void calcdist(char **board, int **dists, int w, int h, int x, int y) {
   int i, j, d, c ;
   if (board[y][x] == WALL || board[y][x] == WATER)
      warn("Bad argument; position is wall or water") ;
   for (i=0; i<=h+1; i++) for (j=0; j<=w+1; j++) dists[i][j] = 1000000000 ;
   aptr = 0 ;
   if (asize == 0) {
      asize = 100 ;
      adata = mymalloc(sizeof(int) * 100) ;
      bsize = 100 ;
      bdata = mymalloc(sizeof(int) * 100) ;
   }
   bptr = 0 ;
   adata[aptr++] = x ;
   adata[aptr++] = y ;
   dists[y][x] = 0 ;
   for (d=1; aptr>0; d++) {
      for (i=0; i<aptr; i+=2) {
         x = adata[i] ;
         y = adata[i+1] ;
         if (bptr + 20 >= bsize) {
            bdata = myrealloc(bdata, (bsize * 2) * sizeof(int)) ;
            bsize *= 2 ;
         } // we can go to '.' and '@' but not '#' or '~'
         x++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && dists[y][x] > d) {
            dists[y][x] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && dists[y][x] > d) {
            dists[y][x] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         x++ ;
         y++ ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && dists[y][x] > d) {
            dists[y][x] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
         y -= 2 ;
         c = board[y][x] ;
         if ((c == PLAIN || c == BASE) && dists[y][x] > d) {
            dists[y][x] = d ;
            bdata[bptr++] = x ;
            bdata[bptr++] = y ;
         }
      }
      aptr = bptr ;
      bptr = 0 ;
      x = asize ;
      asize = bsize ;
      bsize = x ;
      t = bdata ;
      bdata = adata ;
      adata = t ;
   }
}
