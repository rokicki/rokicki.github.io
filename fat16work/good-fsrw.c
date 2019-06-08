#include <cstdio>
#include <cstdlib>
#include <cstring>
#define eight(a) ((a)&255)
#define constant(a) (a)
const int SECTORSIZE = 512 ;
const int SECTORSHIFT = 9 ;
const int DIRSIZE = 32 ;
const int DIRSHIFT = 5 ;
const int DPSSHIFT = (SECTORSHIFT - DIRSHIFT) ;
const int DPSMASK = (1 << DPSSHIFT) - 1 ;
/* BEGIN IGNORE */
void error(const char *s) {
   printf("%s\n", s) ;
   exit(10) ;
}
FILE *f ;
void init() {
   f = fopen("memcard", "rb+") ;
   if (f == 0)
      error("! could not open file") ;
}
void readblock(int n, char *b) {
   fprintf(stderr, "Reading block %d\n", n) ;
   if (fseek(f, n << SECTORSHIFT, SEEK_SET) != 0)
      error("! seek failure") ;
   if (fread(b, 1, SECTORSIZE, f) != 512)
      error("! read failure") ;
}
void writeblock(int n, char *b) {
   fprintf(stderr, "Writing block %d\n", n) ;
   if (fseek(f, n << SECTORSHIFT, SEEK_SET) != 0)
      error("! seek failure") ;
   if (fwrite(b, 1, SECTORSIZE, f) != 512)
      error("! write failure") ;
}
/* END IGNORE */
int lastread ;
int rootdir, rootdirend, dataregion, clustershift, fat1, totclusters ;
int sectorsperfat, dirty ;
int fclust, filesize, floc, frem, bufat, bufend, writelink, direntry, fatptr ;
char buf[SECTORSIZE] ;
char buf2[SECTORSIZE] ;
char padname[11] ;
// end of var section
//
//   Mirror writes to fat.
//
void writeblock2(int n, char *b) {
   writeblock(n, b) ;
   if (n >= fat1 && n < fat1 + sectorsperfat)
      writeblock(n+sectorsperfat, b) ;
}
void flushifdirty() {
   if (dirty) {
      writeblock2(lastread, buf2) ;
      dirty = 0 ;
   }
}
void readblockc(int n) {
   if (n != lastread) {
      flushifdirty() ;
      readblock(n, buf2) ;
      lastread = n ;
   }
}
int brword(char *b) {
   return eight(b[0]) + (eight(b[1]) << 8) ;
}
int brlong(char *b) {
   return brword(b) + (brword(b+2) << 16) ;
}
void brwword(char *w, int v) {
   w[0] = v ;
   w[1] = v >> 8 ;
   dirty = 1 ;
}
void brwlong(char *w, int v) {
   brwword(w, v) ;
   brwword(w+2, v >> 16) ;
}
void mount(int basepin) { int start, sectorspercluster, reserved, rootsectors, sectors ;
   //SPIN sdspi.start(basepin) ;
   lastread = -1 ;
   dirty = 0 ;
   readblock(0, buf) ;
   start = brlong(buf+0x1c6) ;
   readblock(start, buf) ;
   if (brlong(buf+0x36) != constant('F' + ('A' << 8) + ('T' << 16) + ('1' << 24)) || buf[0x3a] != '6')
      error("! maybe not fat16") ;
   if (brword(buf+0x0b) != SECTORSIZE)
      error("! bad bytes per sector") ;
   sectorspercluster = buf[0x0d] ;
   if (sectorspercluster & (sectorspercluster - 1))
      error("! bad sectors per cluster") ;
   clustershift = 0 ;
   while (sectorspercluster > 1) {
      clustershift++ ;
      sectorspercluster >>= 1 ;
   }
   sectorspercluster = 1 << clustershift ;
   reserved = brword(buf+0x0e) ;
   if (buf[0x10] != 2)
      error("! not two FATs") ;
   rootsectors = brword(buf+0x11) ;
   if (rootsectors & DPSMASK)
      error("! root entries do not fill root sectors") ;
   sectors = brword(buf+0x13) ;
   if (sectors == 0)
      sectors = brlong(buf+0x20) ;
   sectorsperfat = brword(buf+0x16) ;
   if (brword(buf+0x1fe) != 0xaa55)
      error("! bad signature") ;
   fat1 = start + reserved ;
   rootdir = fat1 + 2 * sectorsperfat ;
   rootdirend = rootdir + (rootsectors >> DPSSHIFT) ;
   dataregion = rootdirend - 2 * sectorspercluster ;
   totclusters = ((sectors - dataregion + start) >> clustershift) ;
   if (totclusters > 0xfff0)
      error("! too many clusters") ;
   printf("Sectors %d root entries %d sectorspercluster %d clusters %d\n", sectors, rootsectors, sectorspercluster, (totclusters-2)) ;
}
char *readbytec(int byteloc) {
   readblockc(byteloc >> SECTORSHIFT) ;
   return buf2 + (byteloc & constant(SECTORSIZE - 1)) ;
}
char *readfat(int clust) {
   fatptr = (fat1 << SECTORSHIFT) + (clust << 1) ;
   return readbytec(fatptr) ;
}
int nextcluster() { int clust ;
   clust = brword(readfat(fclust)) ;
   writelink = fatptr ;
   if (clust < 2 || clust >= totclusters)
      return -9 ; // bad cluster value
   return clust ;
}
//
//   Assumes the pointer has already been cleared/set to 0xffff.
//
void freeclusters(int clust) { char *bp ;
   if (clust < 2)
      error("! bad cluster number") ;
   while (clust < 0xfff0) {
      if (clust < 2)
         error("! bad cluster number") ;
      bp = readfat(clust) ;
      clust = brword(bp) ;
      brwword(bp, 0) ;
   }
   flushifdirty() ;
}
int datablock() {
   return (fclust << clustershift) + dataregion + ((floc >> SECTORSHIFT) & ((1 << clustershift) - 1)) ;
}
int uc(int c) {
   if ('a' <= c && c <= 'z')
      return c - 32 ;
   return c ;
}
int pflushbuf(int r, int metadata) { int cluststart, newcluster, count, i ;
   if (direntry == 0)
      error("! not open for writing") ;
   if (r > 0) { // must *not* allocate cluster if flushing an empty buffer
      if (frem < SECTORSIZE) {
         // find a new cluster; could be anywhere!  If possible, stay on the
         // same page used for the last cluster.
         cluststart = fclust & constant(~((SECTORSIZE >> 1) - 1)) ;
         newcluster = -1 ;
         count = 2 ;
         while (1) {
            readfat(cluststart) ;
            for (i=0; i<SECTORSIZE; i+=2)
               if (buf2[i]==0 && buf2[i+1]==0)
                  break ;
            if (i < SECTORSIZE) {
               newcluster = cluststart + (i >> 1) ;
               if (newcluster >= totclusters)
                  newcluster = -1 ;
            }
            if (newcluster > 1) {
               brwword(buf2+i, -1) ;
               brwword(readbytec(writelink), newcluster) ;
               writelink = fatptr + i ;
               fclust = newcluster ;
               frem = SECTORSIZE << clustershift ;
               break ;
            } else {
               cluststart += constant(SECTORSIZE >> 1) ;
               if (cluststart >= totclusters) {
                  cluststart = 0 ;
                  count-- ;
                  if (count < 0) {
                     r = -5 ;
                     break ;
                  }
               }
            }
         }
      }
      if (frem >= SECTORSIZE) {
         writeblock(datablock(), buf) ;
         if (r == SECTORSIZE) {  // full buffer, clear it
            floc += r ;
            frem -= r ;
            bufat = 0 ;
            bufend = r ;
         } else {
            // not a full block; leave pointers alone
         }
      }
   }
   if (r < 0 || metadata) { // update metadata even if error
      readblockc(direntry >> SECTORSHIFT) ; // flushes unwritten FAT too
      brwlong(buf2+(direntry & constant(SECTORSIZE-1))+28, floc+bufat) ;
      flushifdirty() ;
   }
   return r ;
}
int pflush() {
   return pflushbuf(bufat, 1) ;
}
int pclose() { int r ;
   r = 0 ;
   if (direntry) {
      r = pflush() ;
      direntry = 0 ;
   }
   bufat = 0 ;
   bufend = 0 ;
   return r ;
}
int popen(char *s, char mode) { int i, j, k, sentinel, dirptr, clusterdata ; char *at ;
   pclose() ;
   bufend = 0 ;
   i = 0 ;
   j = 0 ;
   while (i<8 && s[i] && s[i] != '.')
      padname[j++] = uc(s[i++]) ;
   while (j < 8)
      padname[j++] = ' ' ;
   while (s[i] && s[i] != '.')
      i++ ;
   if (s[i] == '.')
      i++ ;
   while (j<11 && s[i])
      padname[j++] = uc(s[i++]) ;
   while (j < 11)
      padname[j++] = ' ' ;
   sentinel = -1 ;
   for (i=rootdir; i < rootdirend; i++) {
      readblockc(i) ;
      for (j=0; j<SECTORSIZE; j += DIRSIZE) {
         at = buf2 + j ;
         dirptr = j + (i << SECTORSHIFT) ;
         if (direntry == 0 && (at[0] == 0 || at[0] == (char)0xe5))
            direntry = dirptr ;
         if (at[0] == 0) {
            sentinel = dirptr ;
            break ;
         }
         for (k=0; k<11; k++)
            if (padname[k] != at[k])
               break ;
         if (k == 11 && 0 == (at[0x0b] & 0x18)) {
            writelink = dirptr + 26 ;
            direntry = dirptr ;
            fclust = brword(at+0x1a) ;
            filesize = brlong(at+0x1c) ;
            floc = 0 ;
            frem = SECTORSIZE << clustershift ;
            if (mode == 'r') {
               if (frem > filesize)
                  frem = filesize ;
               writelink = 0 ;  // not open for writing
               direntry = 0 ;
               return 0 ;
            }
            if (at[11] & 0xd9)
               return -6 ; // no permission to write
            if (mode == 'd') {
               brwword(at, 0xe5) ;
               freeclusters(fclust) ;
               return 0 ;
            }
            if (mode == 'w') {
               brwword(at+26, -1) ;
               brwlong(at+28, 0) ;
               freeclusters(fclust) ;
               writelink = direntry + 26 ;
               bufend = SECTORSIZE ;
               fclust = 0 ;
               filesize = 0 ;
               frem = 0 ;
               return 0 ;
            } else if (mode == 'a') {
// this code will eventually be moved to seek
               frem = filesize ;
               clusterdata = SECTORSIZE << clustershift ;
               if (fclust >= 0xfff0)
                  fclust = 0 ;
               while (frem > clusterdata) {
                  if (fclust < 2)
                     return -7 ; // eof while following chain
                  fclust = nextcluster() ;
                  if (fclust < 2)
                     return -8 ; // eof while following chain
                  frem -= clusterdata ;
               }
               floc = filesize & constant(~(SECTORSIZE - 1)) ;
               bufend = SECTORSIZE ;
               bufat = frem & constant(SECTORSIZE - 1) ;
               if (bufat != 0) {
                  readblock(datablock(), buf) ;
                  frem = clusterdata - (floc & (clusterdata - 1)) ;
               } else {
                  if (fclust < 2 || frem == clusterdata)
                     frem = 0 ;
                  else
                     frem = clusterdata - (floc & (clusterdata - 1)) ;
               }
               if (fclust >= 2)
                  if (nextcluster() >= 2)
                     printf("Ignoring bad cluster that should not be there\n") ;
               return 0 ;
            } else {
               return -1 ; // bad argument
            }
         }
      }
      if (sentinel > 0)
         break ;
   }
   if (mode == 'r' || mode == 'd')
      return 1 ; // not found
   if (direntry == 0)
      return -2 ; // no empty directory entry
   // write: create valid directory entry
   at = readbytec(direntry) ;
   memset(at, 0, DIRSIZE) ;
   memcpy(at, padname, 11) ;
   brwword(at+26, -1) ;
   if (direntry == sentinel && direntry + DIRSIZE < (rootdirend << SECTORSHIFT))
      brwword(readbytec(direntry+DIRSIZE), 0) ;
   flushifdirty() ;
   writelink = direntry + 26 ;
   fclust = 0 ;
   filesize = 0 ;
   floc = 0 ;
   frem = 0 ;
   bufend = SECTORSIZE ;
   return 0 ;
}
int opendir() { int off ;
   off = rootdir - dataregion ;
   fclust = off >> clustershift ;
   floc = off - (fclust << clustershift) ;
   frem = (rootdirend - rootdir) << SECTORSHIFT ;
   filesize = floc + frem ;
   bufat = 0 ;
   bufend = 0 ;
   return 0 ;
}
int pfillbuf() { int r ;
   if (floc >= filesize)
      return -1 ;
   if (frem == 0) {
      fclust = nextcluster() ;
      if (fclust < 2)
         return -8 ; // no cluster in read
      frem = SECTORSIZE << clustershift ;
      if (frem + floc > filesize)
         frem = filesize - floc ;
   }
   readblock(datablock(), buf) ;
   r = SECTORSIZE ;
   if (floc + r >= filesize)
      r = filesize - floc ;
   floc += r ;
   frem -= r ;
   bufat = 0 ;
   bufend = r ;
   return r ;
}
int nextfile(char *fbuf) { int i, t ; char *at, *lns ;
   while (1) {
      if (bufat >= bufend) {
         t = pfillbuf() ;
         if (t < 0)
            return t ;
      }
      at = buf + bufat ;
      if (at[0] == 0)
         return -1 ;
      bufat += DIRSIZE ;
      if (at[0] != (char)0xe5 && (at[0x0b] & 0x18) == 0) {
         lns = fbuf ;
         for (i=0; i<11; i++) {
            fbuf[0] = at[i] ;
            fbuf++ ;
            if (at[i] != ' ')
               lns = fbuf ;
            if (i == 7 || i == 10) {
               fbuf = lns ;
               if (i == 7) {
                  fbuf[0] = '.' ;
                  fbuf++ ;
               }
            }
         }
         fbuf[0] = 0 ;
         return 0 ;
      }
   }
}
int pread(char *ubuf, int count) { int r, t ;
   r = 0 ;
   while (count > 0) {
      if (bufat >= bufend) {
         t = pfillbuf() ;
         if (t <= 0) {
            if (r > 0)
               return r ;
            return t ;
         }
      }
      t = bufend - bufat ;
      if (t > count)
         t = count ;
      memcpy(ubuf, buf+bufat, t) ;
      bufat += t ;
      r += t ;
      ubuf += t ;
      count -= t ;
   }
   return r ;
}
int pgetc() { int t ;
   if (bufat >= bufend) {
      t = pfillbuf() ;
      if (t < 0)
         return t ;
      if (t == 0)
         return -1 ;
   }
   return eight(buf[bufat++]) ;
}
int pputc(int c) {
   buf[bufat++] = c ;
   if (bufat == SECTORSIZE)
      return pflushbuf(SECTORSIZE, 0) ;
   return 0 ;
}
/* BEGIN IGNORE */
struct {
   int fileno ;
   int pred ;
   int next ;
} fatdata[65536] ;
int chase(int clust, int fileno) {
   int clustercount = 0 ;
   while (clust != 0xffff && clust != 0xfff8) {
      if (fatdata[clust].fileno != 0) {
         printf("Cluster %d used by %d and %d\n", clust, fileno, fatdata[clust].fileno) ;
         error("! bad fileno") ;
      }
      fatdata[clust].fileno = fileno ;
      clust = fatdata[clust].next ;
      clustercount++ ;
   }
   return clustercount ;
}
void checkfat() {
   for (int i=2; i<totclusters; i++) {
      int cc = brword(readfat(i)) ;
      fatdata[i].next = cc ;
      if (cc >= 2 && cc < totclusters)
         if (fatdata[cc].pred++ > 1)
            error("! multiple predecessors?") ;
      if (cc == 1 || (cc >= totclusters && cc != 0xffff && cc != 0xfff8))
         printf("Bad fat value at cluster %d got %d\n", i, cc) ;
   }
   int sentinel = -1 ;
   int fileno = 0 ;
   for (int i=rootdir; sentinel < 0 && i<rootdirend; i++) {
      readblockc(i) ;
      for (int j=0; sentinel < 0 && j<SECTORSIZE; j += DIRSIZE) {
         char *at = buf2 + j ;
         if (at[0] == 0) {
            sentinel = 0 ;
            break ;
         }
         if (at[0] == (char)0xe5 || (at[0x0b] & 0x18))
            continue ;
         filesize = brlong(at+0x1c) ;
         fclust = brword(at+0x1a) ;
         fileno++ ;
         if (fclust != 0xffff && fclust != 0xfff8) {
            if (fatdata[fclust].pred != 0)
               error("! predecessor to file first block?") ;
            fatdata[fclust].pred++ ;
            int blocks = chase(fclust, fileno) ;
            if ((((filesize-1) >> (clustershift + SECTORSHIFT)) + 1) != blocks) {
               printf(
          "File size is %d should require %d clusters but saw %d clusters\n", 
                filesize, (((filesize-1) >> (clustershift + SECTORSHIFT)) + 1),
                blocks) ;
               error("! Bad block usage") ;
            }
         }
      }
   }
   int rootfiles = fileno ;
   int freeclusters = 0 ;
   for (int i=2; i<totclusters; i++)
      if (fatdata[i].fileno == 0 && fatdata[i].next != 0 &&
          fatdata[i].pred == 0) {
         fatdata[i].pred++ ;
         chase(i, ++fileno) ;
      }
   for (int i=2; i<totclusters; i++)
      if (fatdata[i].next == 0) {
         if (fatdata[i].fileno || fatdata[i].pred)
            error("! bad free cluster") ;
         freeclusters++ ;
      } else {
         if (fatdata[i].fileno == 0 || fatdata[i].pred != 1)
            error("! bad alloced cluster") ;
      }
   printf("FAT checks out; free clusters is %d rootfiles %d totfiles %d\n",
          freeclusters, rootfiles, fileno) ;
}
int main(int argc, char *argv[]) {
   init() ;
   mount(0) ;
   int err = 0 ;
   for (int j=1; j<argc; j += 2) {
      char *mode = argv[j] ;
      char *name = argv[j+1] ;
      if (*mode != 'c' && *mode != 'l') {
         if ((err=popen(name, *mode)) != 0) {
            printf("Opening %s in mode %s returned %d\n",
                   name, mode, err) ;
            exit(0) ;
         } else {
            printf("Opening %s in mode %s succeeded.\n", name, mode) ;
         }
fprintf(stderr, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
lastread,
rootdir, rootdirend, dataregion, clustershift, fat1, totclusters,
sectorsperfat, dirty,
fclust, filesize, floc, frem, bufat, bufend, writelink, direntry, fatptr) ;
      }
      int c ;
      char fbuf[13] ;
      switch (*mode) {
case 'r':
         while ((c=pgetc()) >= 0)
            putchar(c) ;
         break ;
case 'w': case 'a':
         while ((c=getchar()) >= 0) {
            err = pputc(c) ;
            if (err < 0) {
               printf("Writing returned %d\n", err) ;
               exit(10) ;
            }
         }
         break ;
case 'd':
         break ; // done
case 'c':
         checkfat() ;
         break ;
case 'l':
         opendir() ;
         while (nextfile(fbuf)==0)
            printf("FILE %s\n", fbuf) ;
         break ;
default:
         printf("Bad mode %s\n", mode) ;
         exit(10) ;
      }
      if (*mode != 'c') {
         err = pclose() ;
         if (err < 0)
            printf("Error closing:  %d\n", err) ;
      }
   }
}
/* END IGNORE */
