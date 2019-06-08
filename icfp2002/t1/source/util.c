/**
 *   Utility functions.
 */
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
extern char *thisline ;
void die(char *s) {
   if (thisline != 0) {
      fprintf(stderr, "Last line: %s\n", thisline) ;
   }
   fprintf(stderr, "Die: %s\n", s) ;
   exit(10) ;
}
void warn(char *s) {
   if (thisline != 0) {
      fprintf(stderr, "Last line: %s\n", thisline) ;
   }
   fprintf(stderr, "Warn: %s\n", s) ;
}
void *mymalloc(int n) {
   void *r = calloc(1, n) ;
   if (r == 0) die("No memory") ;
   return r ;
}
void *myrealloc(void *p, int n) {
   void *r = realloc(p, n) ;
   if (r == 0) die("No memory") ;
   return r ;
}
void *mystrdup(char *s) {
   char *r = strdup(s) ;
   if (r == 0) die("No memory") ;
   return r ;
}
int mrand(int i) {
   return (int)floor(i * drand48()) ;
}
extern int move_count ; // from client.h
static clock_t last_clockt ;
static int inited = 0 ;
int movespersecond = 1 ;
double totaltime = 0.0 ;
/**
 *   Add a slight time fudge factor (10%) so we use under the required
 *   time.
 */
int exterseconds() {
   if (!inited) {
      last_clockt = clock() ;
      inited = 1 ;
      return 1 ;
   } else {
      clock_t now = clock() ;
      clock_t accum = now - last_clockt ;
      int r ;
      last_clockt = now ;
      totaltime += accum ;
      r = (int)(move_count / (double)movespersecond
                                    - (1.1 * totaltime / CLOCKS_PER_SEC) - 1) ;
      if (move_count < 2 && r < 1)
         return r + 2 ; // give a little time at startup
      return r ;
   }
}
int iabs(int i) {
   if (i < 0) return -i ;
   return i ;
}
