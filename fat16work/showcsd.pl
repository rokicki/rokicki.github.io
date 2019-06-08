$csdstr = <<EOF ;
CSD_STRUCT 2 (RES 6) TAAC 1:4:3 NSAC 8 TRAN_SPEED 1:4:3
CCC 1:1:1:1 1:1:1:1 1:1:1:1  READ_BL_LEN 4 READ_BL_PARTIAL 1
WRITE_BLK_MISALIGN 1 READ_BLK_MISALIGN 1
DSR_IMP 1 (RES 2) C_SIZE 12
VDD_R_CURR_MIN 3 VDD_R_CURR_MAX 3 VDD_W_CURR_MIN 3 VDD_W_CURR_MAX 3
C_SIZE_MULT 3 ERASE_BLK_EN 1 SECTOR_SIZE 7 WP_GRP_SIZE 7 WR_GRP_ENABLE 1
(RES 2) RW_FACTOR 3 WRITE_BL_LEN 4 WRITE_BL_PARTIAL 1
(RES 5) FILE_FORMAT_GRP 1 COPY 1 PERM_WRITE_PROT 1 TMP_WRITE_PROTECT 1
FILE_FORMAT 2 (RES 2) CRC 7 (ONE 1)
EOF
my $at = 0 ;
sub pull {
   my $w = shift ;
   my $r = substr($bitstr, $at, $w) ;
   $at += $w ;
   return oct("0b" . $r) ;
}
while (<>) {
   ($desc, $codes) = /(.*)\s*:\s*(.*)$/ ;
   $codes or die "No codes" ;
   print "DESC $desc\n" ;
   $bitstr = "" ;
   for ($i=0; $i<length($codes); $i++) {
      $c = substr($codes, $i, 1) ;
      if ($c gt " ") {
         $bitstr .= sprintf("%04b", hex($c)) ;
      }
   }
   $at = 0 ;
   my $t = $csdstr ;
   $t =~ s/([\d]+)/pull($1)/ge ;
   print $t ;
}
