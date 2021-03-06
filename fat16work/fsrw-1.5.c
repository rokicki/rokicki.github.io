#include <cstdio>
#include <cstdlib>
#include <cstring>
#define eight(a) ((a)&255)
#define constant(a) (a)
#define pri
//{
//   fsrw.spin 1.5  7 April 2007   Radical Eye Software
//
//   This object provides FAT16 file read/write access on a block device.
//   Only one file open at a time.  Open modes are 'r' (read), 'a' (append),
//   'w' (write), and 'd' (delete).  Only the root directory is supported.
//   No long filenames are supported.  We also support traversing the
//   root directory.
//
//   In general, negative return values are errors; positive return
//   values are success.  Other than -1 on popen when the file does not
//   exist, all negative return values will be "aborted" rather than
//   returned.
//
//   Changes:
//       v1.1  28 December 2006  Fixed offset for ctime
//       v1.2  29 December 2006  Made default block driver be fast one
//       v1.3  6 January 2007    Added some docs, and a faster asm
//       v1.4  4 February 2007   Rearranged vars to save memory;
//                               eliminated need for adjacent pins;
//                               reduced idle current consumption; added
//                               sample code with abort code data
//       v1.5  7 April 2007      Fixed problem when directory is larger
//                               than a cluster.
//}
//
//   Constants describing FAT volumes.
//
const int SECTORSIZE = 512 ;
const int SECTORSHIFT = 9 ;
const int DIRSIZE = 32 ;
const int DIRSHIFT = 5 ;
/* BEGIN IGNORE */
void error(const char *s) {
   printf("%s\n", s) ;
   exit(10) ;
}
void spinabort(int v) {
   printf("Spin abort %d\n", v) ;
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
//
//
//   Variables concerning the open file.
//
int fclust ; // the current cluster number
int filesize ; // the total current size of the file
int floc ; // the seek position of the file
int frem ; // how many bytes remain in this cluster from this file
int bufat ; // where in the buffer our current character is
int bufend ; // the last valid character (read) or free position (write)
int direntry ; // the byte address of the directory entry (if open for write)
int writelink ; // the byte offset of the disk location to store a new cluster
int fatptr ; // the byte address of the most recently written fat entry
//
//   Variables used when mounting to describe the FAT layout of the card.
//
int rootdir ; // the byte address of the start of the root directory
int rootdirend ; // the byte immediately following the root directory.
int dataregion ; // the start of the data region, offset by two sectors
int clustershift ; // log base 2 of blocks per cluster
int fat1 ; // the block address of the fat1 space
int totclusters ; // how many clusters in the volume
int sectorsperfat ; // how many sectors per fat
//
//   Variables controlling the caching.
//
int lastread ; // the block address of the buf2 contents
int dirty ; // nonzero if buf2 is dirty
//
//  Buffering:  two sector buffers.  These two buffers must be longword
//  aligned!  To ensure this, make sure they are the first byte variables
//  defined in this object.
//
char buf[SECTORSIZE] ; // main data buffer
char buf2[SECTORSIZE] ; // main metadata buffer
char padname[11] ; // filename buffer
/* end of vars */
pri void writeblock2(int n, char *b) {
//
//   On metadata writes, if we are updating the FAT region, also update
//   the second FAT region.
//
   writeblock(n, b) ;
   if (n >= fat1 && n < fat1 + sectorsperfat)
      writeblock(n+sectorsperfat, b) ;
}
pri void flushifdirty() {
//
//   If the metadata block is dirty, write it out.
//
   if (dirty) {
      writeblock2(lastread, buf2) ;
      dirty = 0 ;
   }
}
pri void readblockc(int n) {
//
//   Read a block into the metadata buffer, if that block is not already
//   there.
//
   if (n != lastread) {
      flushifdirty() ;
      readblock(n, buf2) ;
      lastread = n ;
   }
}
pri int brword(char *b) {
//
//   Read a byte-reversed word from a (possibly odd) address.
//
   return eight(b[0]) + (eight(b[1]) << 8) ;
}
pri int brlong(char *b) {
//
//   Read a byte-reversed long from a (possibly odd) address.
//
   return brword(b) + (brword(b+2) << 16) ;
}
pri void brwword(char *w, int v) {
//
//   Write a byte-reversed word to a (possibly odd) address, and
//   mark the metadata buffer as dirty.
//
   w++[0] = v ;
   w[0] = v >> 8 ;
   dirty = 1 ;
}
pri void brwlong(char *w, int v) {
//
//   Write a byte-reversed long to a (possibly odd) address, and
//   mark the metadata buffer as dirty.
//
   brwword(w, v) ;
   brwword(w+2, v >> 16) ;
}
int mount(int basepin) { int start, sectorspercluster, reserved, rootentries, sectors ;
//{
//   Mount a volume.  The address passed in is passed along to the block
//   layer; see the currently used block layer for documentation.  If the
//   volume mounts, a 0 is returned, else abort is called.
//}
   //SPIN sdspi.start(basepin) ;
   lastread = -1 ;
   dirty = 0 ;
   readblock(0, buf) ;
   if (brlong(buf+0x36) == constant('F' + ('A' << 8) + ('T' << 16) + ('1' << 24))) {
      start = 0 ;
   } else {
      start = brlong(buf+0x1c6) ;
      readblock(start, buf) ;
   }
   if (brlong(buf+0x36) != constant('F' + ('A' << 8) + ('T' << 16) + ('1' << 24)) || buf[0x3a] != '6')
      spinabort(-20) ; // not a fat16 volume
   if (brword(buf+0x0b) != SECTORSIZE)
      spinabort(-21) ; // bad bytes per sector
   sectorspercluster = buf[0x0d] ;
   if (sectorspercluster & (sectorspercluster - 1))
      spinabort(-22) ; // bad sectors per cluster
   clustershift = 0 ;
   while (sectorspercluster > 1) {
      clustershift++ ;
      sectorspercluster >>= 1 ;
   }
   sectorspercluster = 1 << clustershift ;
   reserved = brword(buf+0x0e) ;
   if (buf[0x10] != 2)
      spinabort(-23) ; // not two FATs
   rootentries = brword(buf+0x11) ;
   sectors = brword(buf+0x13) ;
   if (sectors == 0)
      sectors = brlong(buf+0x20) ;
   sectorsperfat = brword(buf+0x16) ;
   if (brword(buf+0x1fe) != 0xaa55)
      spinabort(-24) ; // bad FAT signature
   fat1 = start + reserved ;
   rootdir = (fat1 + 2 * sectorsperfat) << SECTORSHIFT ;
   rootdirend = rootdir + (rootentries << DIRSHIFT) ;
   dataregion = 1 + ((rootdirend - 1) >> SECTORSHIFT) - 2 * sectorspercluster ;
   totclusters = ((sectors - dataregion + start) >> clustershift) ;
   if (totclusters > 0xfff0)
      spinabort(-25) ; // too many clusters
   printf("Sectors %d root entries %d sectorspercluster %d clusters %d\n", sectors, rootentries, sectorspercluster, (totclusters-2)) ;
   return 0 ;
}
pri char *readbytec(int byteloc) {
//
//   Read a byte address from the disk through the metadata buffer and
//   return a pointer to that location.
//
   readblockc(byteloc >> SECTORSHIFT) ;
   return buf2 + (byteloc & constant(SECTORSIZE - 1)) ;
}
pri char *readfat(int clust) {
//
//   Read a fat location and return a pointer to the location of that
//   entry.
//
   fatptr = (fat1 << SECTORSHIFT) + (clust << 1) ;
   return readbytec(fatptr) ;
}
pri int followchain() { int clust ;
//
//   Follow the fat chain and update the writelink.
//
   clust = brword(readfat(fclust)) ;
   writelink = fatptr ;
   return clust ;
}
pri int nextcluster() { int clust ;
//
//   Read the next cluster and return it.  Set up writelink to 
//   point to the cluster we just read, for later updating.  If the
//   cluster number is bad, return a negative number.
//
   clust = followchain() ;
   if (clust < 2 || clust >= totclusters)
      spinabort(-9) ; // bad cluster value
   return clust ;
}
pri void freeclusters(int clust) { char *bp ;
//
//   Free an entire cluster chain.  Used by remove and by overwrite.
//   Assumes the pointer has already been cleared/set to 0xffff.
//
   while (clust < 0xfff0) {
      if (clust < 2)
         spinabort(-26) ; // bad cluster number") ;
      bp = readfat(clust) ;
      clust = brword(bp) ;
      brwword(bp, 0) ;
   }
   flushifdirty() ;
}
pri int datablock() {
//
//   Calculate the block address of the current data location.
//
   return (fclust << clustershift) + dataregion + ((floc >> SECTORSHIFT) & ((1 << clustershift) - 1)) ;
}
pri int uc(int c) {
//
//   Compute the upper case version of a character.
//
   if ('a' <= c && c <= 'z')
      return c - 32 ;
   return c ;
}
pri int pflushbuf(int r, int metadata) { int cluststart, newcluster, count, i ;
//
//   Flush the current buffer, if we are open for write.  This may
//   allocate a new cluster if needed.  If metadata is true, the
//   metadata is written through to disk including any FAT cluster
//   allocations and also the file size in the directory entry.
//
   if (direntry == 0)
      spinabort(-27) ; // not open for writing
   if (r > 0) { // must *not* allocate cluster if flushing an empty buffer
      if (frem < SECTORSIZE) {
         // find a new cluster; could be anywhere!  If possible, stay on the
         // same page used for the last cluster.
         newcluster = -1 ;
         cluststart = fclust & constant(~((SECTORSIZE >> 1) - 1)) ;
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
                     r = -5 ; // No space left on device
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
   if (r < 0)
      spinabort(r) ;
   return r ;
}
int pflush() {
//{
//   Call flush with the current data buffer location, and the flush
//   metadata flag set.
//}
   return pflushbuf(bufat, 1) ;
}
pri int pfillbuf() { int r ;
//
//   Get some data into an empty buffer.  If no more data is available,
//   return -1.  Otherwise return the number of bytes read into the
//   buffer.
//
   if (floc >= filesize)
      return -1 ;
   if (frem == 0) {
      fclust = nextcluster() ;
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
int pclose() { int r ;
//{
//   Flush and close the currently open file if any.  Also reset the
//   pointers to valid values.  If there is no error, 0 will be returned.
//}
   r = 0 ;
   if (direntry)
      r = pflush() ;
   bufat = 0 ;
   bufend = 0 ;
   filesize = 0 ;
   floc = 0 ;
   frem = 0 ;
   writelink = 0 ;
   direntry = 0 ;
   fclust = 0 ;
   return r ;
}
pri int pdate() {
//{
//   Get the current date and time, as a long, in the format required
//   by FAT16.  Right now it's hardwired to return the date this
//   software was created on (April 7, 2007).  You can change this 
//   to return a valid date/time if you have access to this data in
//   your setup.
//}
   return constant(((2007-1980) << 25) + (1 << 21) + (7 << 16) + (4 << 11)) ;
}
int popen(char *s, char mode) { int i, sentinel, dirptr, freeentry ;
//{
//   Close any currently open file, and open a new one with the given
//   file name and mode.  Mode can be 'r' 'w' 'a' or 'd' (delete).
//   If the file is opened successfully, 0 will be returned.  If the
//   file did not exist, and the mode was not 'w' or 'a', -1 will be
//   returned.  Otherwise abort will be called with a negative error
//   code.
//}
   pclose() ;
   i = 0 ;
   while (i<8 && s[0] && s[0] != '.')
      padname[i++] = uc(s++[0]) ;
   while (i<8)
      padname[i++] = ' ' ;
   while (s[0] && s[0] != '.')
      s++ ;
   if (s[0] == '.')
      s++ ;
   while (i<11 && s[0])
      padname[i++] = uc(s++[0]) ;
   while (i < 11)
      padname[i++] = ' ' ;
   sentinel = 0 ;
   freeentry = 0 ;
   for (dirptr=rootdir; dirptr<rootdirend; dirptr += DIRSIZE) {
      s = readbytec(dirptr) ;
      if (freeentry == 0 && (s[0] == 0 || s[0] == (char)0xe5))
         freeentry = dirptr ;
      if (s[0] == 0) {
         sentinel = dirptr ;
         break ;
      }
      for (i=0; i<11; i++)
         if (padname[i] != s[i])
            break ;
      if (i == 11 && 0 == (s[0x0b] & 0x18)) { // this always returns
         fclust = brword(s+0x1a) ;
         filesize = brlong(s+0x1c) ;
         if (mode == 'r') {
            frem = SECTORSIZE << clustershift ;
            if (frem > filesize)
               frem = filesize ;
            return 0 ;
         }
         if (s[11] & 0xd9)
            spinabort(-6) ; // no permission to write
         if (mode == 'd') {
            brwword(s, 0xe5) ;
            freeclusters(fclust) ;
            flushifdirty() ;
            return 0 ;
         }
         if (mode == 'w') {
            brwword(s+26, -1) ;
            brwlong(s+28, 0) ;
            writelink = dirptr + 26 ;
            direntry = dirptr ;
            freeclusters(fclust) ;
            bufend = SECTORSIZE ;
            fclust = 0 ;
            filesize = 0 ;
            frem = 0 ;
            return 0 ;
         } else if (mode == 'a') {
// this code will eventually be moved to seek
            frem = filesize ;
            freeentry = SECTORSIZE << clustershift ;
            if (fclust >= 0xfff0)
               fclust = 0 ;
            while (frem > freeentry) {
               if (fclust < 2)
                  spinabort(-7) ; // eof while following chain
               fclust = nextcluster() ;
               frem -= freeentry ;
            }
            floc = filesize & constant(~(SECTORSIZE - 1)) ;
            bufend = SECTORSIZE ;
            bufat = frem & constant(SECTORSIZE - 1) ;
            writelink = dirptr + 26 ;
            direntry = dirptr ;
            if (bufat) {
               readblock(datablock(), buf) ;
               frem = freeentry - (floc & (freeentry - 1)) ;
            } else {
               if (fclust < 2 || frem == freeentry)
                  frem = 0 ;
               else
                  frem = freeentry - (floc & (freeentry - 1)) ;
            }
            if (fclust >= 2)
               followchain() ;
            return 0 ;
         } else {
            spinabort(-3) ; // bad argument
         }
      }
   }
   if (mode != 'w' && mode != 'a')
      return -1 ; // not found
   direntry = freeentry ;
   if (direntry == 0)
      spinabort(-2) ; // no empty directory entry
   // write (or new append): create valid directory entry
   s = readbytec(direntry) ;
   memset(s, 0, DIRSIZE) ;
   memcpy(s, padname, 11) ;
   brwword(s+26, -1) ;
   i = pdate() ;
   brwlong(s+0xe, i) ; // write create time and date
   brwlong(s+0x16, i) ; // write last modified date and time
   if (direntry == sentinel && direntry + DIRSIZE < rootdirend)
      brwword(readbytec(direntry+DIRSIZE), 0) ;
   flushifdirty() ;
   writelink = direntry + 26 ;
   fclust = 0 ;
   bufend = SECTORSIZE ;
   return 0 ;
}
int pread(char *ubuf, int count) { int r, t ;
//{
//   Read count bytes into the buffer ubuf.  Returns the number of bytes
//   successfully read, or a negative number if there is an error.
//   The buffer may be as large as you want.
//}
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
//{
//   Read and return a single character.  If the end of file is
//   reached, -1 will be returned.  If an error occurs, a negative
//   number will be returned.
//}
   if (bufat >= bufend) {
      t = pfillbuf() ;
      if (t <= 0)
         return -1 ;
   }
   return eight(buf[bufat++]) ;
}
int pwrite(char *ubuf, int count) { int r, t ;
//{
//   Write count bytes from the buffer ubuf.  Returns the number of bytes
//   successfully written, or a negative number if there is an error.
//   The buffer may be as large as you want.
//}
   t = 0 ;
   while (count > 0) {
      if (bufat >= bufend)
         t = pflushbuf(bufat, 0) ;
      t = bufend - bufat ;
      if (t > count)
         t = count ;
      memcpy(buf+bufat, ubuf, t) ;
      r += t ;
      bufat += t ;
      ubuf += t ;
      count -= t ;
   }
   return t ;
}
int pputc(int c) {
//{
//   Write a single character into the file open for write.  Returns
//   0 if successful, or a negative number if some error occurred.
//}
   buf[bufat++] = c ;
   if (bufat == SECTORSIZE)
      return pflushbuf(SECTORSIZE, 0) ;
   return 0 ;
}
int opendir() { int off ;
//{
//   Close the currently open file, and set up the read buffer for
//   calls to nextfile().
//}
   pclose() ;
   off = rootdir - (dataregion << SECTORSHIFT) ;
   fclust = off >> (clustershift + SECTORSHIFT) ;
   floc = off - (fclust << (clustershift + SECTORSHIFT)) ;
   frem = rootdirend - rootdir ;
   filesize = floc + frem ;
 printf("Off %d rootdir %d dataregion %d fclust %d floc %d frem %d filesize %d\n", off, rootdir, dataregion, fclust, floc, frem, filesize) ;
   return 0 ;
}
int nextfile(char *fbuf) { int i, t ; char *at, *lns ;
//{
//   Find the next file in the root directory and extract its
//   (8.3) name into fbuf.  Fbuf must be sized to hold at least
//   13 characters (8 + 1 + 3 + 1).  If there is no next file,
//   -1 will be returned.  If there is, 0 will be returned.
//}
   while (1) {
      if (bufat >= bufend) {
         t = pfillbuf() ;
         if (t < 0)
            return t ;
         if (((floc >> SECTORSHIFT) & ((1 << clustershift) - 1)) == 0)
            fclust++ ;
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
   for (int dirptr=rootdir; dirptr<rootdirend; dirptr += DIRSIZE) {
      char *at = readbytec(dirptr) ;
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
