#!
#   Thrash the heck out of our file system.  Maintains a set of 100
#   files in flux.  On each file, does random tests of reading,
#   writing, appending for file sizes up to 1.5M.  At all points does
#   both ops on both extfs and on our FAT system.  Uses fsrw to do the
#   work for FAT.
#
@filenames = map { "test$_.txt" } 100..599 ;
system("gzip -d <memcard.gz >memcard") ;
system("rm thrash/*") ;
srand(5) ;
sub gendata {
   my $len = shift ;
   my $line = "The time is now " . localtime() . " r is " . rand() . "\n" ;
   my $r = "" ;
   $r = $line x int($len / length($line)) ;
   my $x = chr(97 + int rand 26) ;
   while ($len - length($r) > 1) {
      $r .= $x ;
   }
   if (length($r) < $len) {
      $r .= "\n" ;
   }
   return $r ;
}
while (1) {
   $op = substr("rwad", int rand 4, 1) ;
   $len = int rand 500000 ;
   $len &= ~511 if int rand 3 == 0 ;
   $len = 0 if int rand 4 == 0 ;
   $file = $filenames[rand @filenames] ;
   print "$op $file $len\n" ;
   if ($op eq "w") {
      $a = gendata($len) ;
      open F, ">thrash/$file" or die "Can't open output file" ;
      print F $a ;
      close F ;
      print "./fsrw w $file\n" ;
      open F, "| ./fsrw w $file" ;
      print F $a ;
      close F ;
   } elsif ($op eq "d") {
      unlink "thrash/$file" ;
      print "./fsrw d $file\n" ;
      system("./fsrw d $file") ;
   } elsif ($op eq "a") {
      $a = gendata($len) ;
      system("touch thrash/$file") if ! -f "thrash/$file" ;
      open F, ">>thrash/$file" or die "Can't open output file" ;
      print F $a ;
      close F ;
      print "./fsrw a $file\n" ;
      open F, "| ./fsrw a $file" or die "Can't open fsrw" ;
      print F $a ;
      close F ;
   }
   $opened = 1 ;
   open F, "thrash/$file" or $opened = 0 ;
   $a = '' ;
   if ($opened) {
      $a = join '', <F> ;
      close F ;
   }
   print "./fsrw r $file\n" ;
   open F, "./fsrw r $file |" ;
   $opened2 = 0 ;
   while (<F>) {
      if (/Opening/) {
         if (/succeeded/) {
            $opened2++ ;
         }
         last ;
      }
   }
   if ($opened2 != $opened) {
      die "Bad existence on $file" ;
   }
   $b = '' ;
   if ($opened2) {
      $b = join '', <F> ;
   }
   close F ;
   if ($a ne $b) {
      print "fs [$a]\n" ;
      print "fat [$b]\n" ;
      die "Data mismatch on $file" ;
   }
   print "./fsrw c\n" ;
   open F, "./fsrw c|" ;
   $okay = 0 ;
   while (<F>) {
      if (/FAT checks out/) {
          print ;
          $okay++ ;
      }
   }
   if (!$okay) {
      die "Bad fat check" ;
   }
   @files = map { s,.*/,, ; uc $_ } glob "thrash/*" ;
   open F, "./fsrw l|" ;
   @ofiles = () ;
   while (<F>) {
      if (/FILE (.*)/) {
         $fname = $1 ;
         if ($fname ne 'ABC.TXT' && $fname ne 'FSREAD.C' && $fname ne 'WORD.LST') {
            push @ofiles, $fname ;
         }
      }
   }
   @ofiles = sort @ofiles ;
   @files = sort @files ;
   if ("@ofiles" ne "@files") {
      die "Mismatch in file set" ;
   }
}
