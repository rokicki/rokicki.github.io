/**
 *   Exhaustive exploration of pancakes
 *
 *   No arguments.  Compile defining the preprocessor symbol N.
 */
#define SGI_HASH_NAMESPACE __gnu_cxx
#include <cstdio>
#include <stddef.h>
#include <iostream>
using namespace SGI_HASH_NAMESPACE ;
using namespace std ;
#ifndef N
#define N 3
#endif
int bc[1<<N] ;
long long fact[N+1] ;
class pos {
public:
  pos() {
    for (int i=0; i<N; i++)
      m[i] = i ;
  }
  void flip(int i) {
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
  long long tocardinal() const {
     long long r = m[0] ;
     int bitset = 1 << m[0] ;
     for (int i=1; i<N-1; i++) {
        int bit = 1 << m[i] ;
        r = r * (N-i) + (m[i] - bc[bitset & (bit - 1)]) ;
        bitset |= bit ;
     }
     return r ;
  }
  inline int breaks() const ;
  size_t hash() const ;
  unsigned char m[N];
} ;
inline int pos::breaks() const {
   int r = 0 ;
   for (int i=0; i+1<N; i++) {
      int d = m[i]-m[i+1] ;
      if (d < -1 || d > 1)
         r++ ;
   }
   if (m[N-1] != N-1)
      r++ ;
   return r ;
}
typedef unsigned char uchar ;
int *bits ;
int ctrs[101][101] ;
long long seen = 0 ;
int d ;
void xplor(const pos &p, int togo) {
   long long c = p.tocardinal() ;
   int t = (bits[c>>4] >> (2 * (c & 15))) & 3 ;
   if (togo == 0) {
      if (t == 0) {
         seen++ ;
         ctrs[d][p.breaks()]++ ;
         bits[c>>4] |= 1 << (2 * (c & 15)) ;
         if (d == N + 1) {
            cout << d << " " << p.breaks() << ":" ;
            for (int i=0; i<N; i++)
               cout << " " << (int)p.m[i] ;
            cout << endl ;
         }
      }
      return ;
   }
   if (togo == 1 && t == 3)
      return ;
   for (int i=2; i<=N; i++) {
      pos p2 = p ;
      p2.flip(i) ;
      xplor(p2, togo-1) ;
      bits[c>>4] |= 2 << (2 * (c & 15)) ;
   }
}
int main(int argc, char *argv) {
   for (int i=1; i<1<<N; i++)
      bc[i] = 1 + bc[i & (i-1)] ;
   fact[0] = 1 ;
   for (int i=1; i<=N; i++)
      fact[i] = fact[i-1] * i ;
   bits = (int *)calloc(sizeof(int), 1+fact[N]/16) ;
   if (bits == 0) {
      cerr << "No memory?" << endl ;
      exit(0) ;
   }
   pos p ;
   for (d=0; seen != fact[N]; d++) {
      long long thislev = seen ;
      xplor(p, d) ;
      cout << "At depth " << d << " size is " << (seen-thislev) << endl ;
   }
   for (int i=0; i<=100; i++) for (int j=0; j<=100; j++)
      if (ctrs[i][j])
         cout << i << " " << j << " " << ctrs[i][j] << endl ;
}
