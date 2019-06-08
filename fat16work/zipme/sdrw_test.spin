CON
        _clkmode        = xtal1 + pll16x
        _xinfreq        = 5_000_000
obj
   term: "tv_text"
   sdfat: "fsrw"
var
   byte tbuf[20]
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
   r := sdfat.popen(string("newfilex.txt"), "w")
   term.str(string("Opening returned "))
   term.dec(r)
   term.out(13)
   sta := cnt
   bytes := 0
   repeat 3
      repeat 39
         sdfat.pputc("R")
      sdfat.pputc(13)
   sdfat.pclose
   term.str(string("Wrote file.", 13))
   r := sdfat.popen(string("newfilexr.txt"), "r")
   term.str(string("Opening returned "))
   term.dec(r)
   term.out(13)
   repeat
      r := sdfat.pgetc
      if r < 0
         quit
      term.out(r)
   term.str(string("That's, all, folks! 3", 13))