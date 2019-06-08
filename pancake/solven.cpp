/**
 *   Exhaustive exploration of pancakes.
 *
 *   Only input argument:  the pancake stack.  This program has been
 *   modified to accept either zero-based or one-based stacks.
 */
#include <cstdio>
#include <stddef.h>
#include <iostream>
#include <cstdlib>
#include <time.h>
using namespace std ;
const int MAXN = 100 ;
int N = 10 ;
class pos {
public:
  pos() {
    for (int i=0; i<=MAXN; i++)
      m[i] = i ;
  }
  inline void flip(int i) {
     i-- ;
     int j = 0 ;
     while (i > j) {
        unsigned char t = m[i] ;
        m[i] = m[j] ;
        m[j] = t ;
        j++ ;
        i-- ;
     }
  }
  inline int breaks() const ;
  inline void shuffle() ;
  unsigned char m[MAXN+1];
} __attribute__ ((aligned(8))) ;
inline int pos::breaks() const {
   int r = 0 ;
   for (int i=0; i<N; i++) {
      int d = m[i]-m[i+1] ;
      if (d < -1 || d > 1)
         r++ ;
   }
   return r ;
}
inline void pos::shuffle() {
   while (breaks() < N) {
      for (int i=N; i>1; i--) {
         int d = (int)(i*drand48()) ;
         int t = m[i-1] ;
         m[i-1] = m[d] ;
         m[d] = t ;
      }
   }
}
int moves[150] ;
int od ;
// take a pancake and solve it using trivial IDA
int trysolv0(pos &p, int togo) {
   if (togo == 0) {
      cout << "SOLUTIONLEN " << od << endl ;
      cout << "SOLUTION" ;
      for (int i=od; i>0; i--) {
         cout << " " << moves[i] ;
      }
      cout << endl ;
      return 1 ;
   }
   int s = p.m[0] ;
   if (p.m[1] == s-1 || p.m[1] == s+1) {
      int s2 = s + s - p.m[1] ;
      int i = 2 ;
      while (i <= N && p.m[i] != s2)
         i++ ;
      if (i <= N && s2 + s2 != s + p.m[i-1]) {
         moves[togo] = i ;
         p.flip(i) ;
         if (trysolv0(p, togo-1))
            return 1 ;
         p.flip(i) ;
      }
   } else {
      for (int i=2; i<=N; i++) {
         moves[togo] = i ;
         if (p.m[i] == s-1 || p.m[i] == s+1) {
            int d2 = p.m[i-1] - p.m[i] ;
            if (d2 < -1 || d2 > 1) {
               p.flip(i) ;
               if (trysolv0(p, togo-1))
                  return 1 ;
               p.flip(i) ;
            }
         }
      }
   }
   return 0 ;
}
int trysolv(pos &p, int togo, int v) {
   if (v == 0)
      return 1 ;
   for (int i=2; i<=N; i++) {
      moves[togo] = i ;
      int d1 = p.m[0] - p.m[i] ;
      int d2 = p.m[i-1] - p.m[i] ;
      d1 = (-1 <= d1 && d1 <= 1) ; // helps
      d2 = (-1 <= d2 && d2 <= 1) ; // hurts
      int diff = v - d1 + d2 - togo + 1 ;
      if (diff < 0) {
         p.flip(i) ;
         if (trysolv(p, togo-1, v - d1 + d2))
            return 1 ;
         p.flip(i) ;
      } else if (diff == 0) {
         p.flip(i) ;
         if (trysolv0(p, togo-1))
            return 1 ;
         p.flip(i) ;
      }
   }
   return 0 ;
}
int main(int argc, char *argv[]) {
   pos p ;
   int seed ;
   if (argc > 3) {
      N = argc - 1 ;
      int least = N ;
      for (int i=0; i<N; i++) {
         if (sscanf(argv[i+1], "%d", &seed) != 1 ||
             seed < 0 || seed > N) {
            cerr << "Parameter " << i << " of " << argv[i+1] <<
                    " illegal or out of range" << endl ;
            exit(10) ;
         }
         p.m[i] = seed ;
         if (seed < least)
            least = seed ;
      }
      for (int i=0; i<N; i++)
         p.m[i] -= least ;
      for (int i=0; i<N; i++) {
         int j ;
         for (j=0; j<N && p.m[j] != i; j++) ;
         if (j >= N) {
            cerr << "Illegal input; missing value " << i << endl ;
            exit(10) ;
         }
      }
   } else if (argc == 3) { // seed-based random
      sscanf(argv[1], "%d", &N) ;
      sscanf(argv[2], "%d", &seed) ;
      srand48(seed) ;
      p.shuffle() ;
   } else if (argc == 2) { // time-based random
      sscanf(argv[1], "%d", &N) ;
      srand48(time(0)) ;
      p.shuffle() ;
   } else {
      cerr << "Illegal number of arguments " << endl ;
      exit(10) ;
   }
   for (int i=0; i<N; i++)
      cout << " " << (int)p.m[i] ;
   cout << endl ;
   int v = p.breaks() ;
   int d = v ;
   while (1) {
      od = d ;
      cout << "Trying to solve at depth " << d << endl ;
      if (trysolv(p, d, v))
         break ;
      d++ ;
   }
   cout << "Got a solution at depth " << d << endl ;
}
