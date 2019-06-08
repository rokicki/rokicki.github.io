CON
        _clkmode        = xtal1 + pll16x
        _xinfreq        = 5_000_000
obj
   term: "tv_text"
   sdfat: "fsrw"
var
   byte tbuf[20]
   byte bigbuf[8192]
pub go | x
   x := \start
   term.str(string("Returned from start", 13))
   term.dec(x)
   term.out(13)
pub start | r, sta, bytes
   term.start(12)
   term.str(string("Mounting.", 13))        
   sdfat.mount(0)
   term.str(string("Mounted.", 13))
   term.str(string("Dir: ", 13))
   sdfat.opendir
   repeat while 0 == sdfat.nextfile(@tbuf)
      term.str(@tbuf)
      term.out(13)
   term.str(string("That's the dir", 13))
   term.out(13)
   sta := cnt
   r := sdfat.popen(string("speed.txt"), "w")
   repeat 256
      sdfat.pwrite(@bigbuf, 8192)
   sdfat.pclose
   r := cnt - sta
   term.str(string("Writing 2M took "))
   term.dec(r)
   term.out(13)
   sta := cnt
   r := sdfat.popen(string("speed.txt"), "r")
   repeat 256
      sdfat.pread(@bigbuf, 8192)
   sdfat.pclose
   r := cnt - sta
   term.str(string("Reading 2M took "))
   term.dec(r)
   term.out(13)
   term.str(string("That's, all, folks! 3", 13))