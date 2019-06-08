#include <cstdio>
#include <cstdlib>
#include <cstring>
const int sectorsize = 512 ;
const int sectorshift = 9 ;
const int dirsize = 32 ;
const int dirshift = 5 ;
const int dirpersector = sectorsize / dirsize ;
const int dpsshift = (sectorshift - dirshift) ;
void error(const char *s) {
   printf("%s\n", s) ;
   exit(10) ;
}
FILE *f ;
void init() {
   f = fopen("memcard", "rb") ;
   if (f == 0)
      error("! could not open file") ;
}
void readblock(int n, char *b) {
   if (fseek(f, n << sectorshift, SEEK_SET) != 0)
      error("! seek failure") ;
   if (fread(b, 1, sectorsize, f) != 512)
      error("! read failure") ;
}
char buf[sectorsize] ;
int brword(int off) {
   return (buf[off] & 255) + ((buf[off+1] & 255) << 8) ;
}
int brlong(int off) {
   return brword(off) + (brword(off+2) << 16) ;
}
int rootdir, rootentries, clustershift, fat1, totclusters ;
void mount() {
   readblock(0, buf) ;
   int start = brlong(0x1c6) ;
   int size = brlong(0x1ca) ;
   printf("Got a start of %d size of %d\n", start, size) ;
   readblock(start, buf) ;
   if (brlong(0x36) != ('F' + ('A' << 8) + ('T' << 16) + ('1' << 24)) ||
       buf[0x3a] != '6')
      error("! maybe not fat16") ;
   if (brword(0x0b) != sectorsize)
      error("! bad bytes per sector") ;
   int sectorspercluster = buf[0x0d] ;
   if (sectorspercluster & (sectorspercluster - 1))
      error("! bad sectors per cluster") ;
   clustershift = 0 ;
   while (sectorspercluster > 1) {
      clustershift++ ;
      sectorspercluster >>= 1 ;
   }
   int reserved = brword(0x0e) ;
   if (buf[0x10] != 2)
      error("! not two FATs") ;
   rootentries = brword(0x11) ;
   int sectors = brword(0x13) ;
   if (sectors == 0)
      sectors = brlong(0x20) ;
   int sectorsperfat = brword(0x16) ;
   if (buf[0x1fe] != 0x55 || (buf[0x1ff] & 255) != 0xaa)
      error("! bad signature") ;
   fat1 = start + reserved ;
   rootdir = fat1 + 2 * sectorsperfat ;
   totclusters = (sectors - reserved - 2 * fat1) >> clustershift ;
   if (totclusters > 0xfff0)
      error("! too many clusters") ;
}
int fclust, filesize, floc, frem, bufat, bufend ;
int popenr(char *s) {
   char padname[11] ;
   int i, j, k ;
   for (i=0, j=0; i<8 && s[i] && s[i] != '.';)
      padname[j++] = s[i++] ;
   while (j < 8)
      padname[j++] = ' ' ;
   while (s[i] && s[i] != '.')
      i++ ;
   if (s[i] == '.')
      i++ ;
   while (j<11 && s[i])
      padname[j++] = s[i++] ;
   while (j < 11)
      padname[j++] = ' ' ;
   for (i=0; i<rootentries / dirpersector; i++) {
      readblock(rootdir + i, buf) ;
      for (j=0; j<sectorsize && (i << dpsshift) +
                                 (j >> dirshift) < rootentries; j += dirsize) {
         char *at = buf + j ;
         if (at[0x0b] & 0x18)
            continue ;
         for (k=0; k<11; k++)
            if (padname[k] != at[k] &&
                ((padname[k] ^ at[k]) != 32 ||
                !('a' <= (padname[k] | 32) &&
                         (padname[k] | 32) <= 'z')))
            break ;
         if (k == 11) {
            fclust = brword(j+0x1a) ;
            filesize = brlong(j+0x1c) ;
            floc = 0 ;
            frem = sectorsize << clustershift ;
            if (frem + floc > filesize)
               frem = filesize + floc ;
            bufat = bufend = 0 ;
            return 0 ;
         }
      }
   }
   return 1 ;
}
int pfillbuf() {
   if (floc >= filesize)
      return -1 ;
   if (frem == 0) {
      int fatoff = (fclust >> (sectorshift-1)) ;
      int fatloc = fat1 + fatoff ;
      readblock(fatloc, buf) ;
      fclust = brword((fclust & ((sectorsize >> 1) - 1)) * 2) ;
      if (fclust < 2 || fclust >= totclusters)
         error("! bad cluster value") ;
      frem = sectorsize << clustershift ;
      if (frem + floc > filesize)
         frem = filesize - floc ;
   }
   int fblock = ((fclust - 2) << clustershift) + rootdir +
                (rootentries >> dpsshift) +
                ((floc >> sectorshift) & ((1 << clustershift) - 1)) ;
   readblock(fblock, buf) ;
   int r = sectorsize ;
   if (floc + r >= filesize)
      r = filesize - floc ;
   floc += r ;
   frem -= r ;
   bufat = 0 ;
   bufend = r ;
   return r ;
}
int pread(char *ubuf, int cnt) {
   int r = 0 ;
   while (cnt > 0) {
      if (bufat >= bufend) {
         int t = pfillbuf() ;
         if (t <= 0) {
            if (r > 0)
               return r ;
            return t ;
         }
      }
      int more = bufend - bufat ;
      if (more > cnt)
         more = cnt ;
      memcpy(ubuf, buf+bufat, more) ;
      bufat += more ;
      r += more ;
      ubuf += more ;
      cnt -= more ;
   }
   return r ;
}
int pgetc() {
   if (bufat >= bufend) {
      int t = pfillbuf() ;
      if (t < 0)
         return t ;
      if (t == 0)
         return -1 ;
   }
   return buf[bufat++] & 255 ;
}
int main(int argc, char *argv[]) {
   init() ;
   mount() ;
   char mbuf[12345] ;
   for (int j=1; j<argc; j++) {
      if (popenr(argv[j]) == 0) {
         while (1) {
            int t = pread(mbuf, 12345) ;
            if (t < 0)
               break ;
            for (int i=0; i<t; i++)
               printf("%c", mbuf[i]) ;
         }
         printf("Got all of the file.\n") ;
      } else {
         printf("Could not open %s\n", argv[j]) ;
      }
   }
   printf("Finished.\n") ;
}
