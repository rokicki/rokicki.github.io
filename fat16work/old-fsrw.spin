'
'   This object provides FAT16 file read/write access on a block device.
'   Only one file open at a time.  Open modes are 'r' (read), 'a' (append),
'   'w' (write), and 'd' (delete).  Only the root directory is supported.
'   No long filenames are supported.  Only partitioned volumes are
'   supported.  We also support traversing the root directory.
'
'   Constants describing FAT volumes.
'
con
   SECTORSIZE = 512
   SECTORSHIFT = 9
   DIRSIZE = 32
   DIRSHIFT = 5
'
'   The object that provides the block-level access.
'
obj
   sdspi: "sdspi"
var
'
'   Variables used when mounting to describe the FAT layout of the card.
'
int rootdir ' the byte address of the start of the root directory
int rootdirend ' the byte immediately following the root directory.
int dataregion ' the start of the data region, offset by two sectors
int clustershift ' log base 2 of blocks per cluster
int fat1 ' the block address of the fat1 space
int totclusters ' how many clusters in the volume
int sectorsperfat ' how many sectors per fat
'
'   Variables controlling the caching.
'
int lastread ' the block address of the @buf2 contents
int dirty ' nonzero if @buf2 is dirty
'
'   Variables concerning the open file.
'
int fclust ' the current cluster number
int filesize ' the total current size of the file
int floc ' the seek position of the file
int frem ' how many bytes remain in this cluster from this file
int bufat ' where in the buffer our current character is
int bufend ' the last valid character (read) or free position (write)
int writelink ' the byte offset of the disk location to store a new cluster
int direntry ' the byte address of the directory entry (if open for write)
int fatptr ' the byte address of the most recently written fat entry
'
'  Buffering:  two sector buffers.  These two buffers must be longword
'  aligned!  To ensure this, make sure they are the first byte variables
'  defined in this object.
'
char buf[SECTORSIZE] ' main data buffer
char buf2[SECTORSIZE] ' main metadata buffer
char padname[11] ' filename buffer
pri writeblock2(n, b)
'
'   On metadata writes, if we are updating the FAT region, also update
'   the second FAT region.
'
   sdspi.writeblock(n, b)
   if (n => fat1 and n < fat1 + sectorsperfat)
      sdspi.writeblock(n+sectorsperfat, b)
pri flushifdirty
'
'   If the metadata block is dirty, write it out.
'
   if (dirty)
      writeblock2(lastread, @buf2)
      dirty := 0
pri readblockc(n)
'
'   Read a block into the metadata buffer, if that block is not already
'   there.
'
   if (n <> lastread)
      flushifdirty
      sdspi.readblock(n, @buf2)
      lastread := n
pri brword(b)
'
'   Read a byte-reversed word from a (possibly odd) address.
'
   return (byte[b]) + ((byte[b][1]) << 8)
pri brlong(b)
'
'   Read a byte-reversed long from a (possibly odd) address.
'
   return brword(b) + (brword(b+2) << 16)
pri brwword(w, v)
'
'   Write a byte-reversed word to a (possibly odd) address, and
'   mark the metadata buffer as dirty.
'
   byte[w++] := v
   byte[w] := v >> 8
   dirty := 1
pri brwlong(w, v)
'
'   Write a byte-reversed long to a (possibly odd) address, and
'   mark the metadata buffer as dirty.
'
   brwword(w, v)
   brwword(w+2, v >> 16)
pub mount(basepin) | start, sectorspercluster, reserved, rootentries, sectors
'
'   Mount a volume.  The address passed in is passed along to the block
'   layersee the currently used block layer for documentation.  If the
'   volume mounts, a 0 is returned, else abort is called.
'
   sdspi.start(basepin)
   lastread := -1
   dirty := 0
   sdspi.readblock(0, @buf)
   start := brlong(@buf+$1c6)
   sdspi.readblock(start, @buf)
   if (brlong(@buf+$36) <> constant("F" + ("A" << 8) + ("T" << 16) + ("1" << 24)) or buf[$3a] <> "6")
      abort(string("! maybe not fat16"))
   if (brword(@buf+$0b) <> SECTORSIZE)
      abort(string("! bad bytes per sector"))
   sectorspercluster := buf[$0d]
   if (sectorspercluster & (sectorspercluster - 1))
      abort(string("! bad sectors per cluster"))
   clustershift := 0
   repeat while (sectorspercluster > 1)
      clustershift++
      sectorspercluster >>= 1
   sectorspercluster := 1 << clustershift
   reserved := brword(@buf+$0e)
   if (buf[$10] <> 2)
      abort(string("! not two FATs"))
   rootentries := brword(@buf+$11)
   sectors := brword(@buf+$13)
   if (sectors == 0)
      sectors := brlong(@buf+$20)
   sectorsperfat := brword(@buf+$16)
   if (brword(@buf+$1fe) <> $aa55)
      abort(string("! bad signature"))
   fat1 := start + reserved
   rootdir := (fat1 + 2 * sectorsperfat) << SECTORSHIFT
   rootdirend := rootdir + (rootentries << DIRSHIFT)
   dataregion := 1 + ((rootdirend - 1) >> SECTORSHIFT) - 2 * sectorspercluster
   totclusters := ((sectors - dataregion + start) >> clustershift)
   if (totclusters > $fff0)
      abort(string("! too many clusters"))
pri readbytec(byteloc)
'
'   Read a byte address from the disk through the metadata buffer and
'   return a pointer to that location.
'
   readblockc(byteloc >> SECTORSHIFT)
   return @buf2 + (byteloc & constant(SECTORSIZE - 1))
pri readfat(clust)
'
'   Read a fat location and return a pointer to the location of that
'   entry.
'
   fatptr := (fat1 << SECTORSHIFT) + (clust << 1)
   return readbytec(fatptr)
pri nextcluster | clust
'
'   Read the next cluster and return it.  Set up writelink to
'   point to the cluster we just read, for later updating.  If the
'   cluster number is bad, return a negative number.
'
   clust := brword(readfat(fclust))
   writelink := fatptr
   if (clust < 2 or clust => totclusters)
      return -9 ' bad cluster value
   return clust
pri freeclusters(clust) | bp
'
'   Free an entire cluster chain.  Used by remove and by overwrite.
'   Assumes the pointer has already been cleared/set to $ffff.
'
   if (clust < 2)
      abort(string("! bad cluster number"))
   repeat while (clust < $fff0)
      if (clust < 2)
         abort(string("! bad cluster number"))
      bp := readfat(clust)
      clust := brword(bp)
      brwword(bp, 0)
   flushifdirty
pri datablock
'
'   Calculate the block address of the current data location.
'
   return (fclust << clustershift) + dataregion + ((floc >> SECTORSHIFT) & ((1 << clustershift) - 1))
pri uc(c)
'
'   Compute the upper case version of a character.
'
   if ("a" =< c and c =< "z")
      return c - 32
   return c
pri pflushbuf(r, metadata) | cluststart, newcluster, count, i
'
'   Flush the current buffer, if we are open for write.  This may
'   allocate a new cluster if needed.  If metadata is true, the
'   metadata is written through to disk including any FAT cluster
'   allocations and also the file size in the directory entry.
'
   if (direntry == 0)
      abort(string("! not open for writing"))
   if (r > 0) ' must *not* allocate cluster if flushing an empty buffer
      if (frem < SECTORSIZE)
         ' find a new clustercould be anywhere!  If possible, stay on the
         ' same page used for the last cluster.
         cluststart := fclust & constant(!((SECTORSIZE >> 1) - 1))
         newcluster := -1
         count := 2
         repeat
            readfat(cluststart)
            repeat i from 0 to constant(SECTORSIZE - 2) step 2
               if (buf2[i]==0 and buf2[i+1]==0)
                  quit
            if (i < SECTORSIZE)
               newcluster := cluststart + (i >> 1)
               if (newcluster => totclusters)
                  newcluster := -1
            if (newcluster > 1)
               brwword(@buf2+i, -1)
               brwword(readbytec(writelink), newcluster)
               writelink := fatptr + i
               fclust := newcluster
               frem := SECTORSIZE << clustershift
               quit
            else
               cluststart += constant(SECTORSIZE >> 1)
               if (cluststart => totclusters)
                  cluststart := 0
                  count--
                  if (count < 0)
                     r := -5
                     quit
      if (frem => SECTORSIZE)
         sdspi.writeblock(datablock, @buf)
         if (r == SECTORSIZE) ' full buffer, clear it
            floc += r
            frem -= r
            bufat := 0
            bufend := r
         else
            ' not a full blockleave pointers alone
   if (r < 0 or metadata) ' update metadata even if error
      readblockc(direntry >> SECTORSHIFT) ' flushes unwritten FAT too
      brwlong(@buf2+(direntry & constant(SECTORSIZE-1))+28, floc+bufat)
      flushifdirty
   return r
pub pflush
'
'   Call flush with the current data buffer location, and the flush
'   metadata flag set.
'
   return pflushbuf(bufat, 1)
pri pfillbuf | r
'
'   Get some data into an empty buffer.  If no more data is available,
'   return -1.  Otherwise return the number of bytes read into the
'   buffer.
'
   if (floc => filesize)
      return -1
   if (frem == 0)
      fclust := nextcluster
      if (fclust < 2)
         return -8 ' no cluster in read
      frem := SECTORSIZE << clustershift
      if (frem + floc > filesize)
         frem := filesize - floc
   sdspi.readblock(datablock, @buf)
   r := SECTORSIZE
   if (floc + r => filesize)
      r := filesize - floc
   floc += r
   frem -= r
   bufat := 0
   bufend := r
   return r
pub pclose | r
'
'   Flush and close the currently open file if any.  Also reset the
'   pointers to valid values.  If there is no error, 0 will be returned.
'
   r := 0
   if (direntry)
      r := pflush
   bufat := 0
   bufend := 0
   filesize := 0
   floc := 0
   frem := 0
   writelink := 0
   direntry := 0
   fclust := 0
   return r
pub popen(s, mode) | i, sentinel, dirptr, clusterdata, freeentry, at
'
'   Close any currently open file, and open a new one with the given
'   file name and mode.  Mode can be "r" "w" "a" or "d" (delete).
'   If the file is opened successfully, 0 will be returned.
'
   pclose
   i := 0
   repeat while (i<8 and byte[s] and byte[s] <> ".")
      padname[i++] := uc(byte[s++])
   repeat while (i<8)
      padname[i++] := " "
   repeat while (byte[s] and byte[s] <> ".")
      s++
   if (byte[s] == ".")
      s++
   repeat while (i<11 and byte[s])
      padname[i++] := uc(byte[s++])
   repeat while (i < 11)
      padname[i++] := " "
   sentinel := 0
   freeentry := 0
   repeat dirptr from rootdir to rootdirend - DIRSIZE step DIRSIZE
      at := readbytec(dirptr)
      if (freeentry == 0 and (byte[at] == 0 or byte[at] == $e5))
         freeentry := dirptr
      if (byte[at] == 0)
         sentinel := dirptr
         quit
      repeat i from 0 to 10
         if (padname[i] <> byte[at][i])
            quit
      if (i == 11 and 0 == (byte[at][$0b] & $18)) ' this always returns
         fclust := brword(at+$1a)
         filesize := brlong(at+$1c)
         frem := SECTORSIZE << clustershift
         if (mode == "r")
            if (frem > filesize)
               frem := filesize
            return 0
         if (byte[at][11] & $d9)
            return -6 ' no permission to write
         if (mode == "d")
            brwword(at, $e5)
            freeclusters(fclust)
            flushifdirty
            return 0
         if (mode == "w")
            brwword(at+26, -1)
            brwlong(at+28, 0)
            writelink := dirptr + 26
            direntry := dirptr
            freeclusters(fclust)
            bufend := SECTORSIZE
            fclust := 0
            filesize := 0
            frem := 0
            return 0
         elseif (mode == "a")
' this code will eventually be moved to seek
            frem := filesize
            clusterdata := SECTORSIZE << clustershift
            if (fclust => $fff0)
               fclust := 0
            repeat while (frem > clusterdata)
               if (fclust < 2)
                  return -7 ' eof repeat while following chain
               fclust := nextcluster
               if (fclust < 2)
                  return -8 ' eof repeat while following chain
               frem -= clusterdata
            floc := filesize & constant(!(SECTORSIZE - 1))
            bufend := SECTORSIZE
            bufat := frem & constant(SECTORSIZE - 1)
            writelink := dirptr + 26
            direntry := dirptr
            if (bufat <> 0)
               sdspi.readblock(datablock, @buf)
               frem := clusterdata - (floc & (clusterdata - 1))
            else
               if (fclust < 2 or frem == clusterdata)
                  frem := 0
               else
                  frem := clusterdata - (floc & (clusterdata - 1))
            if (fclust => 2)
               if (nextcluster => 2)
            return 0
         else
            return -3 ' bad argument
   if (mode <> "w" and mode <> "a")
      return 1 ' not found
   direntry := freeentry
   if (direntry == 0)
      return -2 ' no empty directory entry
   ' write (or new append): create valid directory entry
   at := readbytec(direntry)
   bytefill(at, 0, DIRSIZE)
   bytemove(at, @padname, 11)
   brwword(at+26, -1)
   if (direntry == sentinel and direntry + DIRSIZE < rootdirend)
      brwword(readbytec(direntry+DIRSIZE), 0)
   flushifdirty
   writelink := direntry + 26
   fclust := 0
   bufend := SECTORSIZE
   return 0
pub pread(ubuf, count) | r, t
'
'   Read count bytes into the buffer ubuf.  Returns the number of bytes
'   successfully read, or a negative number if there is an error.
'   The buffer may be as large as you want.
'
   r := 0
   repeat while (count > 0)
      if (bufat => bufend)
         t := pfillbuf
         if (t =< 0)
            if (r > 0)
               return r
            return t
      t := bufend - bufat
      if (t > count)
         t := count
      bytemove(ubuf, @buf+bufat, t)
      bufat += t
      r += t
      ubuf += t
      count -= t
   return r
pub pgetc | t
'
'   Read and return a single character.  If the end of file is
'   reached, -1 will be returned.  If an error occurs, a negative
'   number will be returned.
'
   if (bufat => bufend)
      t := pfillbuf
      if (t < 0)
         return t
      if (t == 0)
         return -1
   return (buf[bufat++])
pub pwrite(ubuf, count) | r, t
'
'   Write count bytes from the buffer ubuf.  Returns the number of bytes
'   successfully written, or a negative number if there is an error.
'   The buffer may be as large as you want.
'
   t := 0
   repeat while (count > 0)
      if (bufat => bufend)
         t := pflushbuf(bufat, 0)
         if (t < 0)
            return t
      t := bufend - bufat
      if (t > count)
         t := count
      bytemove(@buf+bufat, ubuf, t)
      r += t
      bufat += t
      ubuf += t
      count -= t
   return t
pub pputc(c)
'
'   Write a single character into the file open for write.  Returns
'   0 if successful, or a negative number if some error occurred.
'
   buf[bufat++] := c
   if (bufat == SECTORSIZE)
      return pflushbuf(SECTORSIZE, 0)
   return 0
pub opendir | off
'
'   Close the currently open file, and set up the read buffer for
'   calls to nextfile.
'
   pclose
   off := (rootdir >> SECTORSHIFT) - dataregion
   fclust := off >> clustershift
   floc := off - (fclust << clustershift)
   frem := rootdirend - rootdir
   filesize := floc + frem
   return 0
pub nextfile(fbuf) | i, t, at, lns
'
'   Find the next file in the root directory and extract its
'   (8.3) name into fbuf.  Fbuf must be sized to hold at least
'   13 characters (8 + 1 + 3 + 1).  If there is no next file,
'   -1 will be returned.  If there is, 0 will be returned.
'
   repeat
      if (bufat => bufend)
         t := pfillbuf
         if (t < 0)
            return t
      at := @buf + bufat
      if (byte[at] == 0)
         return -1
      bufat += DIRSIZE
      if (byte[at] <> $e5 and (byte[at][$0b] & $18) == 0)
         lns := fbuf
         repeat i from 0 to 10
            byte[fbuf] := byte[at][i]
            fbuf++
            if (byte[at][i] <> " ")
               lns := fbuf
            if (i == 7 or i == 10)
               fbuf := lns
               if (i == 7)
                  byte[fbuf] := "."
                  fbuf++
         byte[fbuf] := 0
         return 0
