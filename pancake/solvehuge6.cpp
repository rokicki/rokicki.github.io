/**
 *   Non-exhaustive, BFS exploration of pancakes; huge, uses disk for
 *   everything.
 *
 *   Run with:
 *      ./solveha [options] stack
 *
 *   Note:  you *need* to use DMA I/O for this one.  On a typical
 *   run (2GB machine, with -M 1500 option, on a 100-length stack)
 *   it will do more than a *terabyte* of I/O.  You don't want to
 *   do this with PIO-based IO.
 *
 *   The stack can be one-based or zero-based.
 *
 *   Options:
 *
 *      -M float     how much memory in megabytes to use total.
 *                   To make this accurate, also give -w.
 *      -s           Use singleton heuristic
 *      -n           Do *not* insert original stack in search tree
 *                   (makes no sense unless you also give -b)
 *      -b           Insert inverse pattern into search tree
 *      -L int       Only look for solutions this length or shorter.
 *                   (This can speed things up towards the end.)
 *      -e           Print the solution parity with the solution;
 *                   also, print all solutions rather than just one.
 *      -B           Only permit this many non-singleton blocks.
 *                   Not a useful option.
 *      -Z int       Split the state space into this many zones, and
 *                   store each state into the zone numbered the number
 *                   of breaks mod z.  This helps reduce the tendency
 *                   of the solution to have all late wastes.
 *                   Good values are typically 2 to one less than the
 *                   max number of wastes.
 *      -N int       Write every this many levels to disk; the default
 *                   is 5.  Higher numbers make less efficient use of
 *                   memory since the predecessor tree between levels
 *                   is larger; lower numbers are slower because they
 *                   spend more time writing to the disk.
 */
#define _FILE_OFFSET_BITS 64
#include <cstdio>
#include <stddef.h>
#include <iostream>
#include <cstdlib>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <vector>
using namespace std ;
#ifndef MAXN
#define MAXN 100
#endif
int N = 10 ;
int primes[MAXN+1] ;
int hashprime, bitsize, memsize ;
int zones = 1 ;
int delta_arr[300] ;
int zoneadd[MAXN+1] ;
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
     long long h = 0 ;
     for (int i=0; i<=N; i++)
        h = (h + primes[i] * m[i]) ;
     return h % hashprime ;
  }
  inline void slow_all(int &hash, int &breaks, int &singletons) const {
     long long h = 0 ;
     for (int i=0; i<=N; i++)
        h = (h + primes[i] * m[i]) ;
     hash = h % hashprime ;
     int r = 0 ;
     int rr = 0 ;
     int p = m[first] ;
     int prev = first ;
     int d = 1 ;
     while (p != N+1) {
        int nadj = 1-adj(p-prev) ;
        rr += nadj ;
        if (d & nadj)
           r++ ;
        d = nadj ;
        int next = m[p]-prev ;
        prev = p ;
        p = next ;
     }
     breaks = rr ;
     singletons = r ;
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
/**
 *   Simulate a very large file with fewer smaller files.
 *   Allows us to avoid the system-dependent 64-bit stuff.
 *   Also, allows us to reclaim storage for one layer as we
 *   read it for the last time.  We do it in chunks of 100M
 *   unless mod is larger than that.
 */
struct bigtmpfile {
   bigtmpfile(int mod_, int maxsize=100000000) {
      mod = mod_ ;
      singlefilelimit = (maxsize / mod) * mod ;
      if (singlefilelimit == 0)
         singlefilelimit = mod_ ;
      curf = tmpfile() ;
      if (curf == 0) {
         cerr << "Can't open tmpfile?" << endl ;
         exit(10) ;
      }
      files.push_back(curf) ;
      pos = 0 ;
      fpos = 0 ;
      fileat = 0 ;
      delete_as_we_read = 0 ;
   }
   vector<FILE *> files ;
   FILE *curf ;
   long long pos ;
   int fpos ;
   int fileat ;
   int delete_as_we_read ;
   int mod ;
   int singlefilelimit ;
   long long ftell() const {
      return pos ;
   }
   int fwrite(const void *ptr, int size, int count) {
      int r = ::fwrite(ptr, size, count, curf) ;
      fpos += size * count ;
      pos += size * count ;
      if (fpos >= singlefilelimit) {
         fileat++ ;
         if (fileat >= files.size()) {
 cout << "w" << flush ;
            curf = tmpfile() ;
            if (curf == 0) {
               cerr << "Can't open tmpfile?" << endl ;
               exit(10) ;
            }
            files.push_back(curf) ;
         } else {
            curf = files[fileat] ;
            ::fseek(curf, 0, SEEK_SET) ;
         }
         fpos = 0 ;
      }
      return r ;
   }
   int fread(void *ptr, int size, int count) {
      if (fpos >= singlefilelimit) {
         if (delete_as_we_read) {
            ::fclose(curf) ;
            files[fileat] = 0 ;
         }
         fileat++ ;
         if (fileat >= files.size()) {
            return 0 ;
         } else {
 cout << "r" << flush ;
            curf = files[fileat] ;
            ::fseek(curf, 0, SEEK_SET) ;
         }
         fpos = 0 ;
      }
      int r = ::fread(ptr, size, count, curf) ;
      if (r <= 0)
         return r ;
      fpos += size * count ;
      pos += size * count ;
      return r ;
   }
   int fseek(long long seekpos) {
      pos = seekpos ;
      fileat = seekpos / singlefilelimit ;
      fpos = pos - fileat * (long long)singlefilelimit ;
      curf = files[fileat] ;
      return ::fseek(curf, fpos, SEEK_SET) ;
   }
   int getc() {
      return ::getc(curf) ; // does *not* update position
   }
   void set_delete_as_read() { // after this we must delete this
      delete_as_we_read = 1 ;
   }
   void fclose() { // closes and deletes all files
      for (int i=0; i<files.size(); i++)
         if (files[i]) {
            ::fclose(files[i]) ;
            files[i] = 0 ;
         }
   }
} ;
struct readwork {
   pos thisone ; // source (not what's returned) (not used for files)
   int filepos, limit ; // max count here (only for files)
   bigtmpfile *fromfile ; // this one from a file? if so, movepos etc. is skipped
   int bitpos ;
   unsigned char *bits ;
   inline void rewindbits() {
      bitpos = 0 ;
   }
   inline void writebit(int v) {
// cout << "Writing bit " << v << endl ;
      bits[bitpos >> 3] |= v << (bitpos & 7) ;
      bitpos++ ;
   }
   inline void writebits(int v, int siz) {
// cout << "Writing bits " << v << " size " << siz << endl ;
      bits[bitpos >> 3] |= v << (bitpos & 7) ;
      bits[1+(bitpos >> 3)] |= v >> (8 - (bitpos & 7)) ;
      bitpos += siz ;
   }
   inline int readbit() {
      int r = (bits[bitpos >> 3] >> (bitpos & 7)) & 1 ;
      bitpos++ ;
// cout << "Read bit " << r << endl ;
      return r ;
   }
   inline int readbits(int siz) {
      int r = (((bits[1 + (bitpos >> 3)] << 8) +
                bits[bitpos >> 3]) >> (bitpos & 7)) & ((1 << siz) - 1) ;
      bitpos += siz ;
// cout << "Read bits " << r << " size " << siz << endl ;
      return r ;
   }
   /**
    *   Returns 0 on EOF, 1 on success
    */
   int readone(pos &fillme, int skip) {
      if (fromfile) {
         if (filepos >= limit)
            return 0 ;
         if (fromfile->fread(&fillme, sizeof(pos), 1) != 1) {
            cerr << "Some read error . . . errno " << errno << " at " << filepos << endl ;
            exit(10) ;
         }
         filepos++ ;
// cout << "Read an element from the file " << endl ;
         return 1 ;
      } else {
         int skipcount = 0 ;
         while (readbit())
            skipcount++ ;
         while (skipcount > 1) {
            (this-1)->readone(thisone, 1) ;
            skipcount-- ;
         }
         if (skipcount) {
            if ((this-1)->readone(thisone, 0) == 0) {
               cerr << "Some error . . ." << endl ;
               exit(10) ;
            }
         }
      }
      int move = readbits(7) ;
      if (move == 1)
         return 0 ; // indication of EOF
      if (!skip) {
         fillme = thisone ;
         fillme.slow_flip(move) ;
      }
      return 1 ;
   }
   void rewind() {
      if (fromfile) {
// cout << "Rewinding file . . ." << endl ;
         fromfile->fseek(0LL) ;
         filepos = 0 ;
      } else {
         (this-1)->rewind() ;
         rewindbits() ;
      }
   }
   void setup(int *prev, unsigned char *m, int memsize) {
      int mw = 0 ;
      rewindbits() ;
      memset(bits, 0, bitsize) ;
      int at = -1 ;
      for (int i=0; i<memsize; i++) {
         if (prev[i] < memsize) {
            while (prev[i] > at) {
               writebit(1) ;
               at++ ;
            }
            writebit(0) ;
            writebits(m[i], 7) ;
         }
      }
      writebit(0) ;
      writebits(1, 7) ;
   }
   void setfile(bigtmpfile *f) {
      fromfile = f ;
      limit = f->ftell() / sizeof(pos) ;
 cout << "File set; limit is " << limit << endl ;
   }
} readworkarr[200] ;
unsigned char *movehist ;
double maxmem = 10 ;
unsigned char *s1 ;
int *prev1 ;
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
void outstate(bigtmpfile *f, const pos &p) {
   if (f->fwrite(&p, sizeof(pos), 1) != 1) {
      cerr << "Disk space?" << endl ;
      exit(10) ;
   }
}
void minsort(int *p, unsigned char *m, int n) {
   for (int i=0; i<n; i++)
      for (int j=0; j<i; j++)
         if (p[i] < p[j]) {
            int t = m[i] ;
            m[i] = m[j] ;
            m[j] = t ;
            t = p[i] ;
            p[i] = p[j] ;
            p[j] = t ;
         }
}
void mqsort(int *p, unsigned char *m, int n) {
   if (n <= 1)
      return ;
   if (n <= 6) {
      minsort(p, m, n) ;
      return ;
   }
   int mm = p[n-1] ;
   int si = 0 ;
   for (int i=0; i<n-1; i++)
      if (p[i] < mm) {
         int t = m[i] ;
         m[i] = m[si] ;
         m[si] = t ;
         t = p[i] ;
         p[i] = p[si] ;
         p[si] = t ;
         si++ ;
      }
   p[n-1] = p[si] ;
   p[si] = mm ;
   int t = m[n-1] ;
   m[n-1] = m[si] ;
   m[si] = t ;
   mqsort(p, m, si) ;
   while (p[si] == mm && si < n)
      si++ ;
   mqsort(p+si, m+si, n-si) ;
}
int limit = 200 ;
int stat[200] ;
const int BIGGOAL = 200 ;
int insert_inv = 0 ;
int insert_for = 1 ;
int use_singletons = 0 ;
int maxblocks = 100 ;
int stepsize = 5 ;
int main(int argc, char *argv[]) {
   delta[1] = delta[-1] = 1 ;
   int solutioncount = 0 ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 'M':
         if (sscanf(argv[1], "%lg", &maxmem) != 1) {
            cerr << "Bad param to -M" << endl ;
            exit(10) ;
         }
         argc-- ;
         argv++ ;
         break ;
case 'Z':
         if (sscanf(argv[1], "%d", &zones) != 1) {
            cerr << "Bad param to -Z" << endl ;
            exit(10) ;
         }
         argc-- ;
         argv++ ;
         break ;
case 'N':
         if (sscanf(argv[1], "%d", &stepsize) != 1) {
            cerr << "Bad param to -N" << endl ;
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
default:
         cerr << "Bad arg " << argv[0] << endl ;
         exit(10) ;
      }
   }
   hashprime = (int)(maxmem * 1000000 / (6 + 1.125 * stepsize)) ;
   hashprime = nextprime(hashprime / zones) ;
   memsize = hashprime * zones ;
   if (memsize >= (1 << 28)) {
      cerr << "Memsize too big " << memsize << "; reduce -M or increase -N"
           << endl ;
      exit(10) ;
   }
   for (int i=0; i<=MAXN; i++)
      zoneadd[i] = hashprime * (i % zones) ;
   bitsize = memsize * 9 / 8 + 3 ;
   cout << "Hashprime is " << hashprime << endl ;
   movehist = (unsigned char *)calloc(sizeof(unsigned char), memsize) ;
   if (use_singletons)
      s1 = (unsigned char *)calloc(sizeof(unsigned char), memsize) ;
   prev1 = (int *)calloc(sizeof(int), memsize) ;
   if ((use_singletons && s1 == 0) || prev1 == 0 || movehist == 0) {
      cerr << "No memory." << endl ;
      exit(10) ;
   }
   bigtmpfile *disk_moves = new bigtmpfile(memsize) ;
   bigtmpfile *disk_state = new bigtmpfile(sizeof(pos)) ;
   bigtmpfile *disk_write = new bigtmpfile(sizeof(pos)) ;
   if (disk_moves == 0 || disk_state == 0 || disk_write == 0)
      cerr << "No disk? continuing . . ." << endl ;
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
   int maxprime = hashprime ;
   if (hashprime > 10000000)
      maxprime = 10000000 ;
   for (int i=0; i<=N; i++) {
      int t = 0 ;
      while (1) {
         t = nextprime((int)(drand48() * maxprime)) ;
         if (t < maxprime)
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
      outstate(disk_write, p) ;
   }
   orig_pos = p ;
   if (insert_inv) {
      pos pi1, pi2 ;
      p.to_oldform(pi1) ;
      pi1.invert_into(pi2) ;
      pi2.to_newform(pi1) ;
      v = pi1.slow_breaks() ;
      h = pi1.slow_hash() ;
      outstate(disk_write, pi1) ;
   }
   if (insert_inv && insert_for) {
      cerr << "Don't support both disk_state, insert_inv, and insert_for" ;
      exit(10) ;
   }
   if (v == 0)
      exit(0) ;
   int vbase = v - 1 ;
   int goalmin = 0 ;
   int goalmax = BIGGOAL ;
   pos pi1, pi2, pbuff ;
   // set up readwork
   for (int i=0; i<stepsize; i++) {
      readworkarr[i].bits =
                    (unsigned char *)calloc(sizeof(unsigned char), bitsize) ;
      if (readworkarr[i].bits == 0) {
         cerr << "No mem." << endl ;
         exit(10) ;
      }
   }
   for (int i=stepsize; i<200; i++)
      readworkarr[i].bits = readworkarr[i-stepsize].bits ;
   readworkarr[0].setfile(disk_write) ;
   bigtmpfile *tfile = disk_write ;
   disk_write = disk_state ;
   disk_state = tfile ;
   int used_succ = 0 ;
   for (int d=1; d<2*N; d++) {
      int added = 0 ;
      if (s1)
         memset(s1, 0, memsize * sizeof(unsigned char)) ;
      memset(prev1, -1, memsize * sizeof(int)) ;
      if (goalmax > limit - d)
         goalmax = limit - d ;
      int needwrite = (used_succ >= stepsize) ;
      int really_need_write = needwrite ;
      for (int extreme=0; extreme<2; extreme++) {
 cout << "Goal " << goalmin << ".." << goalmax <<
         " (" << (time(0)-start) << ")" << endl ;
         if (really_need_write) {
            disk_write->fseek(0) ;
            disk_state->set_delete_as_read() ;
         }
         readworkarr[d-1].rewind() ;
         int i = 0 ;
         while (readworkarr[d-1].readone(pbuff, 0)) {
// cout << "Got one at " << d << endl ;
            if (really_need_write)
               outstate(disk_write, pbuff) ;
            const pos *p0p = &pbuff ;
            int oh = 0 ;
 //p0p->slow_hash() ;
            int move_addend = 0 ;
 //v = p0p->slow_breaks() ;
            int sc0 = 0 ;
 //         if (use_singletons)
 //            sc0 = p0p->slow_singletons() ;
            p0p->slow_all(oh, v, sc0) ;
            int prev = p0p->first ;
            int first = p0p->first ;
            int pp = p0p->m[first] ;
            for (int m=2; m<=N; m++) {
               int next = p0p->m[pp]-prev ;
               prev = pp ;
               pp = next ;
               if (adj(prev-pp))
                  continue ;
               int nv = v - adj(first-pp) ;
               if (nv + d <= limit && goalmin <= nv && nv <= goalmax) {
                  if (nv == 0) {
                     p = *p0p ;
                     int moves[2*N] ;
                     int res[2*N] ;
                     moves[0] = m ;
                     for (int ii=1; ii<d; ii++) {
                        int hh = p.slow_hash() + zoneadd[p.slow_breaks()] ;
                        disk_moves->fseek(hh + (d-ii-1) * (long long)memsize) ;
                        moves[ii] = disk_moves->getc() ;
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
                  h += zoneadd[nv] ;
                  int oldv = ((prev1[h] >> 28) & 15) + vbase ;
                  if (use_singletons) {
                     int ns = sc0 ;
                     if (adj(first-pp))
                        ns -= 2 - (adj(p0p->m[first]-first) +
                              adj(prev+pp-p0p->m[pp])) ;
                     if (adj(pp-prev))
                        ns += 2 - (adj(prev+pp-p0p->m[prev]) +
                                   adj(prev+pp-p0p->m[pp])) ;
                     if (nv - ns <= maxblocks && (
                         oldv > nv ||
                         (oldv == nv && s1[h] < ns))) {
                        if (oldv == vbase + 15)
                           added++ ;
                        s1[h] = ns ;
                        prev1[h] = ((nv - vbase) << 28) + i ;
                        movehist[h] = m + move_addend ;
                     }
                  } else {
                     if (oldv > nv) {
                        if (oldv == vbase + 15)
                           added++ ;
                        prev1[h] = ((nv - vbase) << 28) + i ;
                        movehist[h] = m + move_addend ;
                     }
                  }
               }
            }
            i++ ;
         }
 cout << "Saw " << i << endl ;
         if (really_need_write) {
            readworkarr[d-1].setfile(disk_write) ;
            disk_state->fclose() ;
            delete disk_state ;
            disk_state = disk_write ;
            disk_write = new bigtmpfile(sizeof(pos)) ;
         }
         if (d == 1 || added > 9 * memsize / 10 || goalmax >= limit - d)
            break ;
         goalmax = goalmin = goalmax + 1 ;
         really_need_write = 0 ;
      }
      for (int i=0; i<200; i++)
         stat[i] = 0 ;
      for (int i=0; i<memsize; i++) {
         int oldv = vbase + ((prev1[i] >> 28) & 15) ;
         if (oldv != vbase + 15)
            stat[oldv]++ ;
         prev1[i] &= (1 << 28) - 1 ;
      }
      cout << d << ": " ;
      int runsum = 0 ;
      goalmax = 0 ;
      int smallestv = 1000 ;
      for (int i=0; i<200; i++) {
         if (stat[i]) {
            if (smallestv == 1000)
               smallestv = i ;
            cout << "{" << i << ":" << stat[i] << "}" ;
            runsum += stat[i] ;
            if (goalmax == 0 && runsum >= 9 * memsize / 10)
               goalmax = i ;
         }
      }
      cout << endl ;
      vbase = smallestv - 1 ;
      goalmin = 0 ;
      if (goalmax > 0)
         goalmax = goalmax - 1 ;
      else
         goalmax = BIGGOAL ;
      if (added == 0 && solutioncount == 0) {
         cout << "NOSOLUTION" << endl ;
         exit(0) ;
      }
      if (solutioncount)
         break ;
      cout << "Writing moves . . . " << flush ;
      if (disk_moves->fwrite(movehist, 1, memsize) != memsize) {
         cerr << "Space writing moves?" ;
         exit(10) ;
      }
      cout << "done" << endl ;
      cout << "Sorting . . . " << flush ;
      mqsort(prev1, movehist, memsize) ;
      cout << "done" << endl ;
      readworkarr[d].setup(prev1, movehist, memsize) ;
      if (needwrite) {
         used_succ = 1 ;
      } else {
         used_succ++ ;
      }
   }
done:
   int end = time(0) ;
   cout << "TIME " << (end-start) << endl ;
}
