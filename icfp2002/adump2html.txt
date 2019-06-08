#!
#   Written by Tomas Rokicki, Radical Eye Software.
#
#   This Perl script and the embedded JavaScript can be used under either
#   of two licenses, your choice:
#
#      1.  The GPL.  This is suitable if you make it part of another
#          collection of GPL code, in which case all rules and
#          regulations of the GPL apply.
#
#      2.  Absolutely and completely public domain.  Feel free to
#          use any or part of this script or its code, with or without
#          attribution, for any purpose whatsoever.
#
#   Run with Perl.  To process an adump file:
#
#      perl adump2html.pl foo <inputfile
#
#   To process a Simulator.log file:
#
#      perl adump2html.pl -p <packagefile> -m <init-money> foo <inputfile
#
#   You need to give the initial money so this script can track
#   the money; you need the package file to track score.  And you
#   need the simulator version 6.3.
#
#   It will create foo.gif and foo.html in the current directory.
#
#   Try to make the gif about this wide/high, but no wider/higher unless the
#   playing field is wider/higher.
#
$goalwidth = 800 ;
$goalheight = 450 ;
#
$moneyinit = 0 ;
$packagefile = '' ;
while ($ARGV[0] =~ /^-/) {
   if ($ARGV[0] eq '-p') {
      shift ;
      $packagefile = shift ;
   } elsif ($ARGV[0] eq '-m') {
      shift ;
      $moneyinit = shift ;
   } else {
      die "Bad option $ARGV[0]" ;
   }
}
my $board = shift ;
$board or die "Please give a board name as the first argument." ;
#
#   Colors, as dec triads.
#
$watercolor = '5 5 9' ;
$forestcolor = '9 5 5' ;
$emptycolor = '8 8 8' ;
$basecolor = '8 7 5' ;
#
#   We are called upon to parse Haskell-ish things if we read
#   the Simulator log files.
#
#   Parse something given as a (single) string.
#
#   () [] {} group, and must nest.
#   , =      are same as whitespace
#   ""       strings
#
my $srcpos = 0 ;
my $srcstr = "" ;
my $srcend = 0 ;
sub peek {
   if ($srcpos < $srcend) {
      return substr($srcstr, $srcpos, 1) ;
   }
   return '' ;
}
sub get {
   if ($srcpos < $srcend) {
      return substr($srcstr, $srcpos++, 1) ;
   }
   return '' ;
}
sub skipw {
   while (1) {
      my $c = peek() ;
      last if $c eq '' ;
      if ($c le ' ' || $c eq ',' || $c eq '=') {
         get() ;
      } else {
         last ;
      }
   }
   return peek() ;
}
sub toparse {
   my $s = shift ;
   $srcpos = 0 ;
   $srcstr = $s ;
   $srcend = length($s) ;
}
sub e {
   my $c = skipw() ;
   return 0 if $c eq '' ;
   if ($c eq '(' || $c eq '{' || $c eq '[') {
      my $firstc = $c ;
      my @r = () ;
      get() ;
      while (1) {
         $c = skipw() ;
         die "Premature EOF while expecting close bracket" if $c eq '' ;
         if ($c eq ')' || $c eq '}' || $c eq ']') {
            if (($firstc eq '(' && $c eq ')') ||
                ($firstc eq '[' && $c eq ']') ||
                ($firstc eq '{' && $c eq '}')) {
               get() ;
            } else {
               die "Mismatch: $firstc ended with $c" ;
            }
            last ;
         } else {
            push @r, e() ;
         }
      }
      return [@r] ;
   } elsif ($c eq '"') {
      get() ;
      my $r = '' ;
      while (1) {
         $c = get() ;
         die "Premature EOF while expecting \"" if $c eq '' ;
         if ($c eq '"') {
            return $r ;
         } else {
            $r .= $c ;
         }
      }
   } else {
      my $r = $c ;
      get() ;
      while (1) {
         $c = peek() ;
         last if $c eq '' || $c eq '=' || $c eq '(' || $c eq ')' ||
                 $c eq '[' || $c eq ']' || $c eq '{' || $c eq '}' ||
                 $c le ' ' || $c eq ',' ;
         $r .= get() ;
      }
      return $r ;
   }
}
sub show {
   my $s = $_[0] ;
   if (ref($s) eq 'ARRAY') {
      return '[' . join(",", map { show($_) } @{$s}) . ']' ;
   } else {
      return $s ;
   }
}
#
#   If a package file was given, read it in.  Well, we don't, actually.
#
my %packw ;
if ($packagefile ne '') {
   open P, "$packagefile" or die "Couldn't open file $packagefile" ;
   toparse(join('', <P>)) ;
   my $packlist = e() ;
   close P ;
   my $base ;
   for $base (@{$packlist}) {
      my @packs = @{$base->[1]} ;
      my $i ;
      for ($i=1; $i<@packs; $i += 2) {
	 my %packdesc = @{$packs[$i]} ;
	 $packw{$packdesc{'uid'}} = $packdesc{'weight'} ;
      }
   }
}
#
#   Read in the game log.  The first part is the map.  Use it to build
#   a board gif.
#
#   We have to flip it if it starts with #, since that indicates a
#   Simulator game log.
#
while (<>) {
   last unless /^#/ ;
}
($width, $height) = split " ", $_ ;
$mult = 1 ;
while (($mult + 1) * $width <= $goalwidth &&
       ($mult + 1) * $height <= $goalheight) {
   $mult++ ;
}
($realwidth, $realheight) = ($width * $mult, $height * $mult) ;
@boardlines = () ;
for ($i=0; $i<$height; $i++) {
   $lin = <> ;
   $newline = "" ;
   for ($j=0; $j<$width; $j++) {
      $c = substr($lin, $j, 1) ;
      if ($c eq '~') {
         $c = $watercolor ;
      } elsif ($c eq '.') {
         $c = $emptycolor ;
      } elsif ($c eq '#') {
         $c = $forestcolor ;
      } elsif ($c eq '@') {
         $c = $basecolor ;
      } else {
         die "Bad map char: $c\n" ;
      }
      $newline .= $c . ' ' ;
   }
   push @boardlines, "$newline\n" ;
}
#   We are either in adump format or in Simulator log format.
#
#   Simulator log format lines all start with ( or [.  adump format
#   all start with R or I.
#
#   Now read lines.  Two sorts of lines, move lines (R), and identity
#   lines (I).
#
$movemax = 0 ;
$linecount = 0 ;
$adumplines = 0 ;
$simlines = 0 ;
sub mabs {
   my $r = shift ;
   return $r < 0 ? -$r : $r ;
}
my $linemod = 2 ;
my %xpos ;
my %ypos ;
my %money ;
my %packs ;
while (<>) {
   chomp ;
   if (/^R/) {
      my ($move, $id, $x, $y, $score, $money, $packs, $req) =
              /^R (\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+) (.*)/ ;
      die "bad line $_" if !defined($req) ;
      $status[$move][$id] = "$x $y $score $money $packs" ;
      ($req, $death) = split '\|', $req ;
      $req[$move-1][$id] = $req if $move > 0 ;
      $movemax = $move if $move > $movemax ;
      if (defined($death)) {
         $req[$move][$id] = $death ;
         $movemax = $move + 1 if $move + 1 > $movemax ;
      }
      $nameof{$id} = $id if !defined($nameof{$id}) ;
      $adumplines++ ;
   } elsif (/^I/) {
      my ($id, $name) = /^I (\d+) (.*)/ ;
      $nameof{$id} = $name ;
      $adumplines++ ;
   } elsif (/^\[/ || /^\(/) {
#
#   Is this a verbose game log (4 lines per move) or a not-verbose
#   game log (2 lines per move)?  After eating the first line after
#   the board, the following line has the format
#
#   [(#,#...
#
#   if it's a four-line-per-move game log, and the format
#
#   [(#,(#...
#
#   if it's a two-line-per-move game log.
#   game log, and then, when we see the third line of the first move or
#   the first line of the second move, we try to figure out which it is.
#   
#
      if ($linecount > 0) { # ignore first package line
         if ($linecount == 1) { # look at the first line to determine linemod
	    $linemod = (/^\[\(\d+,\(/ ? 4 : 2) ;
         }
	 if ($linecount % $linemod == $linemod-1) {  # commands
	    toparse($_) ;
	    $botcmds = e() ;
	 } elsif ($linecount % $linemod == 0) { # events
#
#   The events really tell us everything we need to know, except of course
#   for the bot requests themselves.  We do have to keep running counts of
#   the x and y positions, the money, and the number of packages carried by
#   each bot though in order to interpret the events.
#
	    toparse($_) ;
	    $botev = e() ;
            $move = ($linecount / $linemod) - 1 ;
            $movemax = $move + 1 ;
            for $i (@{$botcmds}) {
	       my ($id, $bid, @rest) = @{$i} ;
               $req[$move][$id] = "$bid " . show([@rest]) ;
            }
            for $i (@{$botev}) {
               my ($id, $cmds) = @{$i} ;
               my $j ;
               for ($j=0; $j<@{$cmds}; $j++) {
                  my $cmd = $cmds->[$j] ;
                  if ($cmd eq 'Spawned') {
                     my $arg = $cmds->[++$j] ;
                     $xpos{$id} = $arg->[0] ;
                     $ypos{$id} = $arg->[1] ;
                     $money{$id} = $moneyinit ;
                     $packs{$id} = 0 ;
                     $score{$id} = 0 ;
                  }
               }
               $status[$move][$id] =
                  "$xpos{$id} $ypos{$id} $score{$id} $money{$id} $packs{$id}" ;
            }
            for $i (@{$botcmds}) {
	       my ($id, $bid, @rest) = @{$i} ;
               $money{$id} -= mabs($bid) ;
            }
            for $i (@{$botev}) {
               my ($id, $cmds) = @{$i} ;
               my $j ;
               for ($j=0; $j<@{$cmds}; $j++) {
                  my $cmd = $cmds->[$j] ;
                  if ($cmd eq 'Moved') {
                     my $arg = $cmds->[++$j] ;
                     if ($arg eq 'E') {
                        $xpos{$id}++ ;
                     } elsif ($arg eq 'W') {
                        $xpos{$id}-- ;
                     } elsif ($arg eq 'N') {
                        $ypos{$id}++ ;
                     } elsif ($arg eq 'S') {
                        $ypos{$id}-- ;
                     }
                  } elsif ($cmd eq 'Picked') {
                     $j++ ;
                     $packs{$id}++ ;
                  } elsif ($cmd eq 'Dropped') {
                     $j++ ;
                     $packs{$id}-- ;
                  } elsif ($cmd eq 'Delivered') {
                     my $arg = $cmds->[++$j] ;
                     $packs{$id}-- ;
                     $score{$id} += $packw{$arg} ;
                  } elsif ($cmd eq 'Died') {
                     my $arg = $cmds->[++$j] ;
                     $status[$move+1][$id] = 
                  "$xpos{$id} $ypos{$id} $score{$id} $money{$id} $packs{$id}" ;
                     $req[$move+1][$id] = $arg ;
                     $movemax = $move + 2 ;
                  }
               }
               $nameof{$id} = $id if !defined($nameof{$id}) ;
            }
	 }
      }
      $simlines++ ;
      $linecount++ ;
   }
}
die "Hmm, saw lines in both adump and sim format?" if $simlines && $adumplines ;
open F, ">$board.ppm" or die "Can't open $board.pgm" ;
print F "P3\n$width $height\n9\n" ;
@boardlines = reverse @boardlines if $simlines ;
print F $_ for @boardlines ;
close F ;
unlink("$board.gif") ;
system("ppmtogif <$board.ppm >$board.gif") ;
unlink("$board.ppm") ;
die "Couldn't build $board.gif" unless -f "$board.gif" ;
#
@ids = sort { $a <=> $b } keys %nameof ;
$title = join " vs ", map { $nameof{$_} } @ids ;
open F, ">$board.html" or die "Couldn't open $board.html" ;
print F <<EOF ;
<html><head><title>$title</title>
<script>
var data = [
EOF
for ($i=0; $i<$movemax; $i++) {
   $lin = '' ;
   for $id (@ids) {
      if (defined($req[$i][$id])) {
         $more = "$status[$i][$id] $req[$i][$id]" ;
         $lastline[$id] = $more ;
      } else {
         $more = "" ;
      }
      $more =~ s/;"/-/g ;
      $lin .= ';' ;
      $lin .= $more ;
   }
   $lin = substr($lin, 1) ;
   print F "\"$lin\",\n" ;
}
print F <<EOF ;
""] ;
var bots = [
EOF
for $id (@ids) {
   print F "\"$id $nameof{$id}\",\n" ;
}
print F <<EOF ;
""] ;
var lastline = [
EOF
for $id (@ids) {
   print F "\"$lastline[$id]\",\n" ;
}
print F <<EOF ;
""] ;
</script>
<script src="anim.js"></script>
</head>
<body onload="go($mult, $height, $width, '$board');">
<script>writebody();</script>
<noscript>This animation will not display without JavaScript enabled.</noscript>
</body></html>
EOF
close F ;
