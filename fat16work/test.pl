#
#   Test writing, to fill up card.
#
system("gzip -d <memcard.gz >memcard") ;
for ($i=0; $i<600; $i++) {
   $MAGICKEY = rand() ;
   $fn = "t$i.txt" ;
   open F, "|./fsrw w $fn" or die "Can't open fsrw" ;
   for ($j=0; $j<10000; $j++) {
      print F
   "Now is the time for all good men to come to the aid of their country.\n" ;
   }
   print F <<EOF ;
This is a test of a file.
$MAGICKEY
There is not a lot of data in this file.
EOF
   close F ;
   open F, "./fsrw r $fn|" or die "Can't open fsrw" ;
   $ok = 0 ;
   while (<F>) {
      chomp ;
      $ok++ if $_ eq $MAGICKEY ;
   }
   close F ;
   die "Failed at $fn\n" if !$ok ;
}
