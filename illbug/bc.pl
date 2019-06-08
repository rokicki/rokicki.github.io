#!/usr/local/bin/perl -w
#
#   Say how many bytes of hex, whitespace, and non-hex non-whitespace.
#
while (defined($c = getc())) {
   $bc++ ;
   if ($c le ' ') {
      $wc++ ;
   } elsif (('0' le $c && $c le '9') || ('a' le $c && $c le 'f') ||
            ('A' le $c && $c le 'F')) {
      $hc++ ;
   } else {
      $mc++ ;
   }
}
print "Total bytes: $bc\n" ;
print "Hex:  $hc  White:  $wc  Misc:  $mc\n" ;
