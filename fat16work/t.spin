��'   sdspi:  SPI interface to a Secure Digital card.
'   This version is in Spin so it is very slow (3Kb/sec).
'   A version in assembly would be more than 100x faster.
'
'   You probably never want to call this; you want to use sdfat
'   instead (which calls this); this is only the lowest layer.
'
'   Assumes SD card is interfaced using four consecutive Propeller
'   pins, as follows (assuming the base pin is pin 0):
'                3.3v
'              � � � � � �
'              �� �� �� �� �� �� 20k
'   p0 %%%%���%%;%%<%%<%%<%%<%%<%%%%%%% do
'   p1 %%%%���%%%%;%%<%%<%%<%%<%%%%%%% clk
'   p2 %%%%���%%%%%%;%%<%%<%%<%%%%%%% di
'   p3 %%%%���%%%%%%%%;%%<%%<%%%%%%% cs (dat3)
'         150          %%<%%%%%%% irq (dat1)
'                        %%%%%%% p9 (dat2)
'
'   The 150 ohm resistors are current limiters and are only
'   needed if you don't trust your code (and don't want an SD
'   driven signal to conflict with a Propeller driven signal).
'   A value of 150 should be okay, unless you've got some
'   unusually high capacitance on the line.  The 20k resistors
'   are pullups, and should be there on all six lines (even
'   the ones we don't drive).
'
'   This code is not general-purpose SPI code; it's very specific
'   to reading SD cards, although it can be used as an example.
'
'   The code does not use CRC at the moment (this is the default).
'   With some additional effort we can probe the card to see if it
'   supports CRC, and if so, turn it on.   
'
'   All operations are guarded by a watchdog timer, just in case
'   no card is plugged in or something else is wrong.  If an
'   operation does not complete in one second it is aborted.
'
con
   sectorsize = 512
   sectorshift = 9
var
   long di, do, clk, cs, starttime
pri send(outv)
'
'   Send eight bits, then raise di.
'
   outv ><= 8
   repeat 8
      outa[clk] := 0
      outa[di] := outv
      outv >>= 1
      outa[clk] := 1
   outa[di] := 1
pri checktime
'
'   Did we go over our time limit yet?
'
   if cnt - starttime > clkfreq
      abort -41 ' Timeout during read
pri read | r
'
'   Read eight bits from the card.
'
   r := 0
   repeat 8
      outa[clk] := 0
      outa[clk] := 1
      r += r + ina[do]
   return r
pri readresp | r
'
'   Read eight bits, and loop until we
'   get something other than $ff.
'
   repeat
      if (r := read) <> $ff
         return r
      checktime
pri busy | r
'
'   Wait until card stops returning busy
'
   repeat
      if (r := read)
         return r
      checktime
pri cmd(op, parm)
'
'   Send a full command sequence, and get and
'   return the response.  We make sure cs is low,
'   send the required eight clocks, then the
'   command and parameter, and then the CRC for
'   the only command that needs one (the first one).
'   Finally we spin until we get a result.
'
   outa[cs] := 0
   read
   send($40+op)
   send(parm >> 15)
   send(parm >> 7)
   send(parm << 1)
   send(0)
   send($95)
   return readresp
pri endcmd
'
'   Deselect the card to terminate a command.
'
   outa[cs] := 1
   return 0
pub start(basepin)
'
'   Initialize the card!  Send a whole bunch of
'   clocks (in case the previous program crashed
'   in the middle of a read command or something),
'   then a reset command, and then wait until the
'   card goes idle.
'
   do := basepin++
   clk := basepin++ 
   di := basepin++
   cs := basepin
   outa := -1
   dira[clk..cs] := 7
   starttime := cnt
   repeat 600
      read
   cmd(0, 0)
   endcmd
   repeat
      cmd(55, 0)
      basepin := cmd(41, 0)
      endcmd
      if basepin <> 1
         quit
   if basepin
      abort -40 ' could not initialize card
   return 0
pub readblock(n, b)
'
'   Read a single block.  The "n" passed in is the
'   block number (blocks are 512 bytes); the b passed
'   in is the address of 512 blocks to fill with the
'   data.
'
   starttime := cnt
   cmd(17, n)
   readresp
   repeat sectorsize
      byte[b++] := read
   read
   read
   return endcmd
{
pub getCSD(b)
'
'   Read the CSD register.  Passed in is a 16-byte
'   buffer.
'
   starttime := cnt
   cmd(9, 0)
   readresp
   repeat 16
      byte[b++] := read
   read
   read
   return endcmd
}
pub writeblock(n, b)
'
'   Write a single block.  Mirrors the read above.
'
   starttime := cnt
   cmd(24, n)
   send($fe)
   repeat sectorsize
      send(byte[b++])
   read
   read
   if ((readresp & $1f) <> 5)
      abort -42
   busy
   return endcmd