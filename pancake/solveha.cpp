/**
 *   Non-exhaustive, BFS exploration of pancakes.
 *
 *   Run with:
 *      ./solveha [options] stack
 *
 *   The stack can be one-based or zero-based.
 *
 *   Options:
 *
 *      -M float     how much memory in megabytes to use total.
 *                   To make this accurate, also give -w.
 *      -w           Write move recapture information to disk.
 *      -s           Use singleton heuristic
 *      -2           Use double-ended search
 *      -n           Do *not* insert original stack in search tree
 *                   (makes no sense unless you also give -b)
 *      -b           Insert inverse pattern into search tree
 *      -L int       Only look for solutions this length or shorter.
 *                   (This can speed things up towards the end.)
 *      -e           Print the solution parity with the solution;
 *                   also, print all solutions rather than just one.
 *      -d           Write each level to disk.  This doubles the
 *                   effective memory but makes it a lot slower.
 *      -B           Only permit this many non-singleton blocks.
 *                   Not a useful option.
 */
#include <cstdio>
#include <stddef.h>
#include <iostream>
#include <cstdlib>
#include <time.h>
#include <string.h>
using namespace std ;
#ifndef MAXN
#define MAXN 100
#endif
int N = 10 ;
int primes[MAXN+1] ;
int hashprime ;
int delta_arr[300] ;
int * const delta = delta_arr + 150 ;
inline int adj(int v) {
   return delta[v] ;
}
class pos {
public:
  pos() {
    newform_init() ;
  }
  void newform_init() {
    m[0] = 1 ;
    for (int i=1; i<MAXN+2; i++)
      m[i] = 2 * i ;
    first = 0 ;
  }
  void oldform_init() {
    for (int i=0; i<MAXN+2; i++)
      m[i] = i ;
    first = 0 ;
  }
  inline void slow_flip(int i) {
     int p = m[first] ;
     int prev = first ;
     while (i > 1) {
        int next = m[p]-prev ;
        prev = p ;
        p = next ;
        i-- ;
     }
     m[first] += p ;
     m[prev] -= p ;
     m[p] += first - prev ;
     first = prev ;
  }
  inline int slow_breaks() const {
     int r = 0 ;
     int p = m[first] ;
     int prev = first ;
     while (p != N+1) {
// cout << "Slow " << prev << " " << p << " " << (int)m[p] << endl ;
        if (!adj(p-prev))
           r++ ;
        int next = m[p]-prev ;
        prev = p ;
        p = next ;
     }
     return r ;
  }
  inline int slow_singletons() const {
     int r = 0 ;
     int p = m[first] ;
     int prev = first ;
     int d = 1 ;
     while (p != N+1) {
        int nadj = 1-adj(p-prev) ;
        if (d & nadj)
           r++ ;
        d = nadj ;
        int next = m[p]-prev ;
        prev = p ;
        p = next ;
     }
     return r ;
  }
  inline int slow_hash() const {
     int h = 0 ;
     for (int i=0; i<=N; i++)
        h = (h + primes[i] * m[i]) % hashprime ;
     return h ;
  }
  inline void to_oldform(pos &p) const {
     p.oldform_init() ;
     int pp = m[first] ;
     int prev = first ;
     int i = 0 ;
// cout << "Setting " << i << " to " << prev << endl ;
     p.m[i++] = prev ;
     while (pp != N+1) {
// cout << "Setting " << i << " to " << pp << endl ;
        p.m[i++] = pp ;
        int next = m[pp]-prev ;
        prev = pp ;
        pp = next ;
     }
  }
  inline void to_newform(pos &p) const {
     p.newform_init() ;
     p.first = m[0] ;
     p.m[p.first] = m[1] ;
     for (int i=1; i<N+1; i++) {
//        cout << " for triad " << (int)m[i-1] << " " << (int)m[i] << " " << (int)m[i+1] << " setting sum to " << (int)(m[i-1] + m[i+1]) << endl ;
        p.m[m[i]] = m[i-1] + m[i+1] ;
     }
  }
  inline void invert_into(pos &p) const {
     p.oldform_init() ;
     for (int i=0; i<N; i++)
        p.m[m[i]] = i ;
  }
  inline bool operator==(const pos &p) const {
    return memcmp(m, p.m, N)==0 ;
  }
  unsigned char first ;
  unsigned char m[MAXN+2];
} __attribute__ ((aligned(8))) ;
unsigned char *movehist[2*MAXN] ;
double maxmem = 10 ;
unsigned char *f0, *f1 ;
unsigned char *s0 ;
pos *p0, *p1 ;
int nextprime(int i) {
   int j ;
   i |= 1 ;
   for (;; i+=2) {
      for (j=3; j*j<=i; j+=2)
         if (i % j == 0)
            break ;
      if (j*j > i)
         return i ;
   }
}
int even_odd = 0 ;
int parities_seen = 0 ;
pos orig_pos ;
void solution(int *sol, int len) {
   int p[MAXN+2] ;
   int fixedsol[MAXN*3] ;
   pos go = orig_pos ;
   pos solved ;
   // first, fix the moves in the pre_move case.
   int a = 0 ;
   int b = len - 1 ;
   int end = 0 ;
   for (int i=0; i<len; i++) {
      if (sol[i] > 100) { // invert before
         end = ! end ;
      }
      if (end) {
         fixedsol[b--] = (sol[i] - 1) % 100 + 1 ;
      } else {
         fixedsol[a++] = (sol[i] - 1) % 100 + 1 ;
      }
   }
   sol = fixedsol ;
   for (int i=0; i<MAXN+2; i++)
      p[i] = i ;
   int parity = 0 ;
   for (int i=0; i<len; i++) {
      go.slow_flip(sol[i]) ;
      int m = sol[i] ;
      for (int j=0; j<m; j++)
         if (p[j] == 0)
            parity++ ;
      int k = m - 1 ;
      int kk = 0 ;
      while (k > kk) {
         int t = p[k] ;
         p[k] = p[kk] ;
         p[kk] = t ;
         k-- ;
         kk++ ;
      }
   }
   parity &= 1 ;
   parities_seen = 0 ;
   if (!(parities_seen & (1 << parity))) {
      cout << "SOLUTIONLEN " << len << endl ;
      if (even_odd)
         cout << "SOLUTION_PARITY " << parity << endl ;
      cout << "SOLUTION" ;
      for (int i=0; i<len; i++)
         cout << " " << sol[i] ;
      cout << endl ;
      parities_seen |= 1 << parity ;
   }
   if (!(go == solved))
      cout << "Check solution!" << endl ;
}
void outstate(FILE *f, const pos &p) {
   if (fwrite(&p, sizeof(pos), 1, f) != 1) {
      cerr << "Disk space?" << endl ;
      exit(10) ;
   }
}
pos readbuf ;
pos *readstate(FILE *f) {
   if (fread(&readbuf, sizeof(pos), 1, f) != 1) {
      cerr << "Disk read error?" << endl ;
      exit(10) ;
   }
   return &readbuf ;
}
int limit = 200 ;
int stat[200] ;
const int BIGGOAL = 200 ;
int insert_inv = 0 ;
int insert_for = 1 ;
int use_singletons = 0 ;
int use_disk_moves = 0 ;
int use_disk_state = 0 ;
int use_premoves = 0 ;
int maxblocks = 100 ;
int main(int argc, char *argv[]) {
   delta[1] = delta[-1] = 1 ;
   int solutioncount = 0 ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case '2':
         use_premoves = 1 ;
         break ;
case 'M':
         if (sscanf(argv[1], "%lg", &maxmem) != 1) {
            cerr << "Bad param to -M" << endl ;
            exit(10) ;
         }
         argc-- ;
         argv++ ;
         break ;
case 'L':
         if (sscanf(argv[1], "%d", &limit) != 1) {
            cerr << "Bad param to -L" << endl ;
            exit(10) ;
         }
         argc-- ;
         argv++ ;
         break ;
case 'B':
         if (sscanf(argv[1], "%d", &maxblocks) != 1) {
            cerr << "Bad param to -B" << endl ;
            exit(10) ;
         }
         use_singletons = 1 ;
         argc-- ;
         argv++ ;
         break ;
case 'b':
         insert_inv = 1 ;
         break ;
case 'n':
         insert_for = 0 ;
         break ;
case 'e':
         even_odd = 1 ;
         break ;
case 's':
         use_singletons = 1 ;
         break ;
case 'd':
         use_disk_state = 1 ;
         break ;
case 'w':
         use_disk_moves = 1 ;
         break ;
default:
         cerr << "Bad arg " << argv[0] << endl ;
         exit(10) ;
      }
   }
   if (use_disk_state)
      hashprime = 2 * ((int)(maxmem * 500000) / (sizeof(pos) + sizeof(unsigned char))) ;
   else
      hashprime = (int)(maxmem * 500000) / (sizeof(pos) + sizeof(unsigned char)) ;
   hashprime = nextprime(hashprime) ;
   cout << "Hashprime is " << hashprime << endl ;
   f0 = (unsigned char *)calloc(sizeof(unsigned char), hashprime) ;
   f1 = (unsigned char *)calloc(sizeof(unsigned char), hashprime) ;
   if (!use_disk_state)
      p0 = (pos *)calloc(sizeof(pos), hashprime) ;
   p1 = (pos *)calloc(sizeof(pos), hashprime) ;
   if (use_singletons)
      s0 = (unsigned char *)calloc(sizeof(unsigned char), hashprime) ;
   if (f0 == 0 || f1 == 0 || (!use_disk_state && p0 == 0) || p1 == 0 ||
       (use_singletons && s0 == 0)) {
      cerr << "No memory." << endl ;
      exit(10) ;
   }
   FILE *disk_moves = 0 ;
   FILE *disk_state = 0 ;
   if (use_disk_moves) {
      disk_moves = tmpfile() ;
      if (disk_moves == 0)
         cerr << "No disk? continuing . . ." << endl ;
   }
   if (use_disk_state) {
      disk_state = tmpfile() ;
      if (disk_state == 0)
         cerr << "No disk? continuing . . ." << endl ;
   }
   pos p ;
   p.oldform_init() ;
   int seed ;
   if (argc > 2) {
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
   } else {
      cerr << "Illegal number of arguments " << endl ;
      exit(10) ;
   }
   srand48(time(0)) ;
   for (int i=0; i<=N; i++) {
      int t = 0 ;
      while (1) {
         t = nextprime((int)(drand48() * hashprime)) ;
         if (t < hashprime)
            break ;
      }
      primes[i] = t ;
   }
   int start = time(0) ;
   cout << "INPUT" ;
   for (int i=0; i<N; i++)
      cout << " " << (int)p.m[i] ;
   cout << endl ;
   p.to_newform(orig_pos) ;
   p = orig_pos ;
   int v = p.slow_breaks() ;
   int h = p.slow_hash() ;
   if (insert_for) {
      f0[h] = v ;
      if (use_disk_state)
         outstate(disk_state, p) ;
      else
         p0[h] = p ;
   }
   orig_pos = p ;
   if (insert_inv) {
      pos pi1, pi2 ;
      p.to_oldform(pi1) ;
      pi1.invert_into(pi2) ;
      pi2.to_newform(pi1) ;
      v = pi1.slow_breaks() ;
      h = pi1.slow_hash() ;
      f0[h] = v ;
      if (use_disk_state)
         outstate(disk_state, pi1) ;
      else
         p0[h] = pi1 ;
   }
   if (use_disk_state && insert_inv && insert_for) {
      cerr << "Don't support both disk_state, insert_inv, and insert_for" ;
      exit(10) ;
   }
   if (v == 0)
      exit(0) ;
   int goalmin = 0 ;
   int goalmax = BIGGOAL ;
   pos pi1, pi2 ;
   for (int d=1; d<2*N; d++) {
      int added = 0 ;
      if (disk_moves) {
         if (d > 1) {
            if (fwrite(movehist[d-1], sizeof(unsigned char), hashprime,
                       disk_moves) != hashprime) {
               cerr << "Failed while writing to temp file" << endl ;
               exit(10) ;
            }
            memset(movehist[d-1], 0, hashprime * sizeof(unsigned char)) ;
            movehist[d] = movehist[d-1] ;
            movehist[d-1] = 0 ;
         } else {
            movehist[d] = (unsigned char *)calloc(sizeof(unsigned char), hashprime) ;
         }
      } else {
         movehist[d] = (unsigned char *)calloc(sizeof(unsigned char), hashprime) ;
      }
      if (movehist[d] == 0) {
         cerr << "No memory?" << endl ;
         exit(10) ;
      }
      if (s0)
         memset(s0, 0, hashprime * sizeof(unsigned char)) ;
      if (goalmax > limit - d)
         goalmax = limit - d ;
      for (int extreme=0; extreme<2; extreme++) {
 cout << "Goal " << goalmin << ".." << goalmax << endl ;
         if (disk_state)
            fseek(disk_state, 0, SEEK_SET) ;
         for (int i=0; i<hashprime; i++) {
            const pos *p0p ;
            int oh, move_addend = 0 ;
            for (int up=0; up<=use_premoves; up++) {
             if (f0[i]) {
               if (up) {
                  p0p->to_oldform(pi1) ;
                  pi1.invert_into(pi2) ;
                  pi2.to_newform(pi1) ;
                  p0p = &pi1 ;
                  oh = p0p->slow_hash() ;
                  if (!disk_state && s0[i] == s0[oh] && p0[oh] == *p0p)
                     break ;
                  move_addend = 100 ;
               } else if (disk_state) {
                  p0p = readstate(disk_state) ;
                  oh = p0p->slow_hash() ;
                  move_addend = 0 ;
               } else {
                  p0p = p0 + i ;
                  oh = i ;
                  move_addend = 0 ;
               }
               v = f0[i] ;
               int sc0 = 0 ;
               if (use_singletons)
                  sc0 = p0p->slow_singletons() ;
               int prev = p0p->first ;
               int first = p0p->first ;
               int pp = p0p->m[first] ;
               for (int m=2; m<=N; m++) {
                  int next = p0p->m[pp]-prev ;
                  prev = pp ;
                  pp = next ;
                  int nv = v - adj(first-pp) + adj(prev-pp) ;
                  if (nv + d <= limit && goalmin <= nv && nv <= goalmax) {
                     if (nv == 0) {
                        p = *p0p ;
                        int moves[2*N] ;
                        int res[2*N] ;
                        moves[0] = m ;
                        for (int ii=1; ii<d; ii++) {
                           int hh = p.slow_hash() ;
                           if (disk_moves) {
                              fseek(disk_moves, hh + (d-ii-1) * hashprime,
                                    SEEK_SET) ;
                              moves[ii] = getc(disk_moves) ;
                           } else {
                              moves[ii] = movehist[d-ii][hh] ;
                           }
                           p.slow_flip(((moves[ii] - 1) % 100) + 1) ;
                           if (moves[ii] > 100) {
                              pos p1, p2 ;
                              p.to_oldform(p1) ;
                              p1.invert_into(p2) ;
                              p2.to_newform(p) ;
                           }
                        }
                        if (p == orig_pos) {
                           for (int ii=d-1; ii>=0; ii--)
                              res[d-1-ii] = moves[ii] ;
                           solution(res, d) ;
                        } else {
                           pos p1, p2 ;
                           p.to_oldform(p1) ;
                           p1.invert_into(p2) ;
                           p2.to_newform(p1) ;
                           if (p1 == orig_pos) {
                              solution(moves, d) ;
                           } else {
                              cout << "Some sort of error??" << endl ;
                           }
                        }
                        solutioncount++ ;
                        if (even_odd)
                           continue ;
                        goto done ;
                     }
                     int deltah = pp * (primes[first] - primes[prev]) +
                                  (first - prev) * primes[pp] ;
                     h = oh + deltah ;
                     if (h >= 0)
                        h = h % hashprime ;
                     else
                        h = hashprime - 1 + ((h + 1) % hashprime) ;
                     if (use_singletons) {
                        int ns = sc0 ;
                        if (adj(first-pp))
                           ns -= 2 - (adj(p0p->m[first]-first) +
                                 adj(prev+pp-p0p->m[pp])) ;
                        if (adj(pp-prev))
                           ns += 2 - (adj(prev+pp-p0p->m[prev]) +
                                      adj(prev+pp-p0p->m[pp])) ;
                        if (nv - ns <= maxblocks && (
                            f1[h] == 0 || f1[h] > nv ||
                            (f1[h] == nv && s0[h] < ns))) {
                           if (f1[h] == 0)
                              added++ ;
                           f1[h] = nv ;
                           p1[h] = *p0p ;
                           p1[h].m[first] += pp ;
                           p1[h].m[prev] -= pp ;
                           p1[h].m[pp] += first - prev ;
                           p1[h].first = prev ;
                           s0[h] = ns ;
                           movehist[d][h] = m + move_addend ;
                        }
                     } else {
                        if (f1[h] == 0 || f1[h] > nv) {
                           if (f1[h] == 0)
                              added++ ;
                           f1[h] = nv ;
                           p1[h] = *p0p ;
                           p1[h].m[first] += pp ;
                           p1[h].m[prev] -= pp ;
                           p1[h].m[pp] += first - prev ;
                           p1[h].first = prev ;
                           movehist[d][h] = m + move_addend ;
                        }
                     }
                  }
               }
            }
           }
         }
         if (d == 1 || added > 9 * hashprime / 10 || goalmax >= limit - d)
            break ;
         goalmax = goalmin = goalmax + 1 ;
      }
      for (int i=0; i<200; i++)
         stat[i] = 0 ;
      for (int i=0; i<hashprime; i++)
         if (f1[i] > 0)
            stat[f1[i]]++ ;
      cout << d << ": " ;
      int runsum = 0 ;
      goalmax = 0 ;
      for (int i=0; i<200; i++) {
         if (stat[i]) {
            cout << "{" << i << ":" << stat[i] << "}" ;
            runsum += stat[i] ;
            if (goalmax == 0 && runsum >= 9 * hashprime / 10)
               goalmax = i ;
         }
      }
      cout << endl ;
      goalmin = 0 ;
      if (goalmax > 0)
         goalmax = goalmax - 1 ;
      else
         goalmax = BIGGOAL ;
      if (added == 0 && solutioncount == 0) {
         cout << "NOSOLUTION" << endl ;
         exit(0) ;
      }
      if (disk_state) {
         fseek(disk_state, 0, SEEK_SET) ;
         for (int i=0; i<hashprime; i++) {
            if (f1[i])
               outstate(disk_state, p1[i]) ;
         }
      } else {
         pos *t = p0 ;
         p0 = p1 ;
         p1 = t ;
      }
      {
         unsigned char *t = f0 ;
         f0 = f1 ;
         f1 = t ;
         memset(f1, 0, hashprime * sizeof(unsigned char)) ;
      }
      if (solutioncount)
         break ;
   }
done:
   int end = time(0) ;
   cout << "TIME " << (end-start) << endl ;
}
