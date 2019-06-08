require POSIX ;
@_s = () ;
$_pi180 = 90 / POSIX::acos(0) ;
sub _apply { (pop @_s)->() }
sub _add {  push @_s, (pop @_s) + (pop @_s) }
sub _sub {  push @_s, - (pop @_s) + (pop @_s) }
sub _mul {  push @_s, (pop @_s) * (pop @_s) }
sub _eq {  push @_s, (pop @_s) == (pop @_s) }
sub _div {  push @_s, 1 / (pop @_s) * (pop @_s) }
sub _divi {  push @_s, int(1 / (pop @_s) * (pop @_s)) }
sub _less {  push @_s, (pop @_s) > (pop @_s) }
sub _neg {  push @_s, - (pop @_s) }
sub _real {}
sub _sin { push @_s, sin((pop @_s)/$_pi180) }
sub _cos { push @_s, cos((pop @_s)/$_pi180) }
sub _asin { push @_s, $_pi180 * POSIX::asin(pop @_s) }
sub _acos { push @_s, $_pi180 * POSIX::acos(pop @_s) }
sub _i { my $f1 = pop @_s ; my $f2 = pop @_s ; ((pop @_s) ? $f2 : $f1)->() }
sub mclamp { my $v = shift ; return ($v < 0 ? 0 : $v > 1 ? 1 : $v) }
sub _clamp { push @_s, mclamp(pop @_s) }
sub _floor { push @_s, POSIX::floor(pop @_s) }
sub _frac { push @_s, POSIX::fmod(pop @_s) }
sub _get { my $i = pop @_s ; push @_s, (pop @_s)->[$i] }
sub _length { push @_s, scalar(@{pop @_s}) }
sub _mark { push @_s, undef }
sub _array { my @t = () ; my $i ;
             unshift @t, $i while defined($i=pop @_s) ;
             push @_s, [@t] }
sub _getx { push @_s, (pop @_s)->[0] }
sub _gety { push @_s, (pop @_s)->[1] }
sub _getz { push @_s, (pop @_s)->[2] }
sub _mod { my $v = pop @_s; push @_s, (pop @_s) % $v }
sub _point { push @_s, [reverse (pop @_s, pop @_s, pop @_s)] }
sub _sqrt { push @_s, sqrt(pop @_s) }
my $idmatrix = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1] ;
sub matmul {
   my ($t1, $t2) = @_ ;
   my ($i, $j) ; 
   my $s = [(0) x 16] ;
   for ($i=0; $i<4; $i++) {
      for ($j=0; $j<16; $j += 4) {
         $s->[$i + $j] = $t1->[$j] * $t2->[$i] + $t1->[$j + 1] * $t2->[$i+4] +
                  $t1->[$j+2] * $t2->[$i+8] + $t1->[$j+3] * $t2->[$i+12] ;
      }
   }
   return $s ;
}
sub transf {
   my ($ob, $t, $inv) = @_ ;
   if ($ob->{object} eq "Union") {
      return { %{$ob}, ob1=>transf($ob->{ob1}, $t, $inv),
                       ob2=>transf($ob->{ob2}, $t, $inv) } ;
   } else {
      return { %{$ob}, coor=>matmul($t, $ob->{coor}),
                       inv=>matmul($ob->{inv}, $inv) }
   }
}
#   Rendering objects
sub _sphere { ob(Sphere, 1) }
sub _plane { ob(Plane, 0) }
sub _cube { ob(Cube, 3) ; push @_s, (1, 1, 1) ; _translate() ;
            push @_s, .5 ; _uscale() }
sub _cylinder { ob(Cylinder, 2) ; push @_s, (0, 1, 0) ; _translate() ;
                push @_s, (1, .5, 1) ; _scale() }
sub _cone { ob(Cone, 1) ; push @_s, (0, 1, 0) ; _translate() }
#   Geometry
sub ob { push @_s, {object=>$_[0], surface=>pop @_s, diam=>$_[1],
         coor=>$idmatrix, inv=>$idmatrix } }
sub transform { push @_s, transf(pop @_s, shift, shift) }
sub _uscale {
   my $s = pop @_s ;
   transform([$s, 0, 0, 0, 0, $s, 0, 0, 0, 0, $s, 0, 0, 0, 0, 1],
             [1/$s, 0, 0, 0, 0, 1/$s, 0, 0, 0, 0, 1/$s, 0, 0, 0, 0, 1]) ;
}
sub _scale {
   my $z = pop @_s ;
   my $y = pop @_s ;
   my $x = pop @_s ;
   transform([$x, 0, 0, 0, 0, $y, 0, 0, 0, 0, $z, 0, 0, 0, 0, 1],
             [1/$x, 0, 0, 0, 0, 1/$y, 0, 0, 0, 0, 1/$z, 0, 0, 0, 0, 1]) ;
}
sub _translate {
   my $z = pop @_s ;
   my $y = pop @_s ;
   my $x = pop @_s ;
   transform([1, 0, 0, $x, 0, 1, 0, $y, 0, 0, 1, $z, 0, 0, 0, 1],
             [1, 0, 0, -$x, 0, 1, 0, -$y, 0, 0, 1, -$z, 0, 0, 0, 1]) ;
}
sub _rotatex {
   my $a = (pop @_s) / $_pi180 ;
   my $s = sin $a ;
   my $c = cos $a ;
   transform([1, 0, 0, 0, 0, $c, -$s, 0, 0, $s, $c, 0, 0, 0, 0, 1],
             [1, 0, 0, 0, 0, $c, $s, 0, 0, -$s, $c, 0, 0, 0, 0, 1]) ;
}
sub _rotatey {
   my $a = (pop @_s) / $_pi180 ;
   my $s = sin $a ;
   my $c = cos $a ;
   transform([$c, 0, $s, 0, 0, 1, 0, 0, -$s, 0, $c, 0, 0, 0, 0, 1],
             [$c, 0, -$s, 0, 0, 1, 0, 0, $s, 0, $c, 0, 0, 0, 0, 1]) ;
}
sub _rotatez {
   my $a = (pop @_s) / $_pi180 ;
   my $s = sin $a ;
   my $c = cos $a ;
   transform([$c, -$s, 0, 0, $s, $c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
             [$c, $s, 0, 0, -$s, $c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]) ;
}
sub _union { push @_s, {object=>Union, ob1=>pop @_s, ob2=>pop @_s} }
sub _intersect { pop @_s }
sub _difference { pop @_s }
sub _light { push @_s, {object=>Light, color=>pop @_s, dir=>pop @_s} }
sub _pointlight { push @_s, {object=>Pointlight, color=>pop @_s, loc=>pop @_s} }
sub _spotlight { pop @_s ; pop @_s ; push @_s, {object=>Pointlight,
                               color=>pop @_s, ignore=>pop @_s, loc=>pop@_s } }
#  Rendering!
sub walkobj {
   my $ob = shift ;
   if ($ob->{object} eq "Union") {
      walkobj($ob->{ob1}) ;
      walkobj($ob->{ob2}) ;
   } else {
      $ob->{intsct} = ${$ob->{object} . "intsct"} ;
      $ob->{surf} = ${$ob->{object} . "surf"} ;
      push @objs, $ob ;
   }
}
sub walklights {
   my $light = shift ;
   my $r = {%{$light}} ; 
   $r->{dir} = negate(scalar normalize($light->{dir})) if $r->{dir} ;
   return $r ;
}
sub consider {
   my $v = shift ;
   @xs = ($v) if (@xs == 0 || $xs[0]->[0] > $v->[0]) ;
}
# Object geometry
$Planeintsct = sub {
   my ($ob, $o, $d, $len) = @_ ;
   return if $d->[1] == 0 ;
   $u = - $o->[1] / $d->[1] ;
   consider([$u/$len, $ob, [0, 1, 0], $o, $d, $u]) if ($u > 0) ;
} ;
$Planesurf = sub {
   my ($u, $ob, $norm, $o, $d, $uu) = @{(shift)} ;
   my $p = along($o, $d, $uu) ;
   push @_s, (0, $p->[0], $p->[2]) ;
} ;
$Sphereintsct = sub {
   my ($ob, $o, $d, $u, $d2, $len) = @_ ;
   if ($u > 0) {
      $u -= sqrt(1-$d2) ;
      consider([$u/$len, $ob, along($o, $d, $u), $o, $d]) ;
   }
} ;
$Spheresurf = sub {
   my ($u, $ob, $norm) = @{(shift)} ;
   my $ang = $_pi180 * atan2($norm->[0], $norm->[2]) ;
   $ang += 360 if $ang < 0 ;
   push @_s, (0, $ang/360, (1+$norm->[1])/2) ;
} ;
$Cubeintsct = sub {
   my ($ob, $o, $d, $u, $d2, $len) = @_ ;
   my $f ;
   for ($f=0; $f < 6; $f++) {
      my ($i, $t) = ((($f >> 1) + 2) % 3, 2 * ($f & 1) - 1) ;
      if ($d->[$i] * $t < 0) {
         $u = ($t - $o->[$i]) / $d->[$i] ;
         if ($u > 0) {
            my $p = along($o, $d, $u) ;
            if (abs($p->[($i+1)%3]) <= 1 && abs($p->[($i+2)%3]) <= 1) {
               my $norm = [0, 0, 0] ;
               $norm->[$i] = $t ;
               consider([$u/$len, $ob, $norm, $o, $d, $f, $p]) ;
            }
         }
      }
   }
} ;
$Cubesurf = sub {
   my ($uu, $ob, $norm, $o, $d, $f, $p) = @{(shift)} ;
   my ($u, $v) = ($p->[($f & 6)==2], $p->[2-(($f & 6)==0)]) ;
   push @_s, ($f, ($u + 1) / 2, ($v + 1) / 2) ;
} ;
$Cylinderintsct = sub {
   my ($ob, $o, $d, $u, $d2, $len) = @_ ;
   if ($d->[1] != 0) {
      my ($t, $f) = ($d->[1] < 0 ? (1, 1) : (-1, 2)) ;
      $u = ($t - $o->[1]) / $d->[1] ;
      if ($u > 0) {
         my $p = along($o, $d, $u) ;
         consider([$u/$len, $ob, [0, $t, 0], $o, $d, $f, $p]) if ss($p) <= 2 ;
      }
   }
   my ($x, $y, $z) = @{$o} ;
   my ($xd, $yd, $zd) = @{$d} ;
   my ($a, $b, $c) = ($xd * $xd + $zd * $zd,
           2 * ($x * $xd + $z * $zd),
           $x * $x + $z * $z - 1) ;
   my $q = $b * $b - 4 * $a * $c ;
   if ($a != 0 && $q >= 0) {
      if (($u = (-sqrt($q)-$b)/(2 * $a)) > 0) {
         my $p = along($o, $d, $u) ;
         consider([$u/$len, $ob, scalar normalize([$p->[0], 0, $p->[2]]),
                                       $o, $d, 0, $p]) if (abs($p->[1]) <= 1) ;
      }
   }
} ;
$Cylindersurf = sub {
   my ($u, $ob, $norm, $o, $d, $f, $p) = @{(shift)} ;
   if ($f == 0) {
      my $ang = $_pi180 * atan2($norm->[0], $norm->[2]) ;
      push @_s, ($f, $ang/360, (1+$p->[1])/2) ;
   } else {
      push @_s, ($f, ($p->[0]+1)/2, ($p->[2]+1)/2) ;
   }
} ;
$Coneintsct = sub {
   my ($ob, $o, $d, $u, $d2, $len) = @_ ;
   if ($d->[1] < 0) {
      if (($u = - $o->[1] / $d->[1]) > 0) {
         my $p = along($o, $d, $u) ;
         consider([$u/$len, $ob, [0, 1, 0], $o, $d, 1, $p]) if ss($p) <= 1 ;
      }
   }
   my ($x, $y, $z) = @{$o} ;
   my ($xd, $yd, $zd) = @{$d} ;
   $y += 1 ;
   my ($a, $b, $c) = ($xd * $xd - $yd * $yd + $zd * $zd,
           2 * ($x * $xd - $y * $yd + $z * $zd),
           $x * $x - $y * $y + $z * $z) ;
   my $q = $b * $b - 4 * $a * $c ;
   if ($a != 0 && $q >= 0) {
      if (($u = (-sqrt($q)-$b)/(2 * $a)) > 0) {
         my $p = along($o, $d, $u) ;
         my $div = sqrt($p->[0] * $p->[0] + $p->[2] * $p->[2]) ;
         consider([$u/$len, $ob, scalar normalize([$p->[0]/$div, -1,
           $p->[2]/$div]), $o, $d, 0, $p]) if ($p->[1] >= -1 && $p->[1] <= 0) ;
      }
   }
} ;
$Conesurf = sub {
   my ($u, $ob, $norm, $o, $d, $f, $p) = @{(shift)} ;
   if ($f == 0) {
      my $ang = $_pi180 * atan2($norm->[0], $norm->[2]) ;
      push @_s, ($f, $ang/360, 1+$p->[1]) ;
   } else {
      push @_s, ($f, ($p->[0]+1)/2, ($p->[2]+1)/2) ;
   }
} ;
sub ss {
   my $p = shift ;
   return $p->[0] * $p->[0] + $p->[1] * $p->[1] + $p->[2] * $p->[2] ;
}
sub vecmap {
   my ($vec, $mat) = @_ ;
   my ($x, $y, $z) = @{$vec} ;
   return normalize([$mat->[0] * $x + $mat->[1] * $y + $mat->[2] * $z,
                     $mat->[4] * $x + $mat->[5] * $y + $mat->[6] * $z,
                     $mat->[8] * $x + $mat->[9] * $y + $mat->[10] * $z]) ;
}
sub pointmap {
   my ($point, $mat) = @_ ;
   my ($x, $y, $z) = @{$point} ;
   return [
      $mat->[0] * $x + $mat->[1] * $y + $mat->[2] * $z + $mat->[3],
      $mat->[4] * $x + $mat->[5] * $y + $mat->[6] * $z + $mat->[7],
      $mat->[8] * $x + $mat->[9] * $y + $mat->[10] * $z + $mat->[11]
   ] ;
}
sub normalmap { # pass the *inverse* matrix you'd expect
   my ($vec, $mat) = @_ ;
   my ($x, $y, $z) = @{$vec} ;
   return normalize([$mat->[0] * $x + $mat->[4] * $y + $mat->[8] * $z,
                     $mat->[1] * $x + $mat->[5] * $y + $mat->[9] * $z,
                     $mat->[2] * $x + $mat->[6] * $y + $mat->[10] * $z]) ;
}
sub normalize {
   my ($x, $y, $z) = @{$_[0]} ;
   my $d = sqrt($x * $x + $y * $y + $z * $z) ;
   return ([$x/$d, $y/$d, $z/$d], $d) if wantarray ;
   return [$x/$d, $y/$d, $z/$d] ;
}
sub dirof {
   my ($p1, $p2) = @_ ;
   return normalize([$p2->[0]-$p1->[0], $p2->[1]-$p1->[1], $p2->[2]-$p1->[2]]) ;
}
sub negate {
   my $a = shift ;
   return [-$a->[0], -$a->[1], -$a->[2]] ;
}
sub chkob {
   my ($origin, $dirv, $obj) = @_ ;
   my $toob = $obj->{inv} ;
   my $o = pointmap($origin, $toob) ;
   my ($d, $len) = vecmap($dirv, $toob) ;
   if ($obj->{object} eq Plane) {
      $obj->{intsct}->($obj, $o, $d, $len) ;
   } else {
      my $u = -dot($o, $d) ;
      my $d2 = ss(along($o, $d, $u)) ;
      $obj->{intsct}->($obj, $o, $d, $u, $d2, $len) if $d2 <= $obj->{diam} ;
   }
}
sub dot {
   my ($p1, $p2) = @_ ;
   return $p1->[0] * $p2->[0] + $p1->[1] * $p2->[1] + $p1->[2] * $p2->[2] ;
}
sub prod {
   my ($p1, $p2) = @_ ;
   return [$p1->[0] * $p2->[0], $p1->[1] * $p2->[1], $p1->[2] * $p2->[2]] ;
}
sub along {
   my ($p1, $p2, $d) = @_ ;
   return [$d * $p2->[0] + $p1->[0], $d * $p2->[1] + $p1->[1],
           $d * $p2->[2] + $p1->[2]] ;
}
sub cvdiff {
   my ($vn, $v1) = @_ ;
   return along($v1, $vn, -2 * dot($vn, $v1)) ;
}
sub chaseray {
   my ($origin, $dirv, $depth, $notobj) = @_ ;
   @xs = () ;
   for (@objs) {
      chkob($origin, $dirv, $_) if $_ != $notobj;
   }
   return [0, 0, 0] if @xs == 0 ;
   my $work = $xs[0] ;
   my $obj = $work->[1] ;
   my $normal = normalmap($work->[2], $obj->{inv}) ;
   $obj->{surf}->($work) ;
   $obj->{surface}->() ;
   my $phong = pop @_s ;
   my $spec = pop @_s ;
   my $diff = pop @_s ;
   my $color = pop @_s ;
# light sources
   my $intersect = along($origin, $dirv, $work->[0]) ;
   my $sofar = along([0, 0, 0], $amb, $diff) ;
   my $light ;
   for $light (@lights) {
      my ($ldist, $ldir, $atten) = (1e80, 0, 1) ;
      if ($light->{object} ne "Light") {
         ($ldir, $ldist) = dirof($intersect, $light->{loc}) ;
         $atten = 100 / (99 + $ldist * $ldist) ;
      } else {
         $ldir = $light->{dir} ;
      }
      my $d = dot($normal, $ldir) ;
      if ($d > 0) {
         @xs = () ;
         for (@objs) {
            chkob($intersect, $ldir, $_) if $_ != $obj ;
            last if @xs && $xs[0]->[0] <= $ldist ;
         }
         if (0 == @xs || $xs[0]->[0] > $ldist) {
            my $half = dirof($dirv, $ldir) ;
            my $spech = dot($half, $normal) ;
            if ($spech > 0 && $spec > 0) {
               $sofar = along($sofar, $light->{color},
                          $atten * ($d * $diff + ($spech ** $phong) * $spec)) ;
            } else {
               $sofar = along($sofar, $light->{color}, $atten * $d * $diff) ;
            }
         }
      }
   }
   $sofar = along($sofar, chaseray($intersect, cvdiff($normal, $dirv),
                          $depth-1, $obj), $spec) if $depth > 0 && $spec > 0 ;
   return prod($sofar, $color) ;
}
sub renderpix {
   my ($x, $y) = @_ ;
   $drawn++ ;
   my $v = chaseray([0, 0, -1], scalar dirof([0, 0, -1], [$x, $y, 0]),
                    $depth, {}) ;
   return join '', map { chr (255.99999 * mclamp($_)) } @{$v} ;
}
sub tan {
   my $a = shift ;
   return sin($a)/cos($a) ;
}
sub _render {
   @objs = () ;
   $drawn = 0 ;
   my $filename = pop @_s ;
   $pixh = pop @_s ; $pixw = pop @_s ;
   my $fov = pop @_s ; $depth = pop @_s ;
   my $obj = pop @_s ;
   $lights = pop @_s ; $amb = pop @_s ;
   @lights = map {walklights $_} @{$lights} ;
#  Iterate through objs making a list
   walkobj($obj) ;
   print STDERR scalar @objs, " objs ", scalar @{$lights}, " lights " ;
   $imagepw = tan(0.5 * $fov / $_pi180) ;
   $imageph = $pixh / $pixw * $imagepw ;
   my $prevint = 1048576 ;
   $lastper = $pixh * $pixw / 40 ;
   $img[0][0] = renderpix((1 / $pixw - 1) * $imagepw,
                          (1 - 1 / $pixh) * $imageph) ;
   for ($int=8; $int>0; $int >>= 1) {
      my $fn = $int==1 ? "$filename" : "p$int.ppm" ;
      open PPM, ">$fn" or die "Couldn't open image file" ;
      $prevint -= $int ;
      print PPM "P6\n# Radical\n", int($pixw/$int), " ", int($pixh/$int),
                " 255\n" ;
      my ($imagei, $imagej) ;
      for ($imagej=0; $imagej<$pixh; $imagej += $int) {
         for ($imagei=0; $imagei<$pixw; $imagei += $int) {
            if (($imagei | $imagej) & $prevint) {
               $img[$imagei][$imagej] = renderpix(
                         (2 * ($imagei + 0.5) / $pixw - 1) * $imagepw,
                         (1 - 2 * ($imagej + 0.5) / $pixh) * $imageph) ;
            }
            print PPM $img[$imagei][$imagej] ;
         }
         if ($drawn > $lastper) {
            print STDERR "." ;
            $lastper += $pixh * $pixw / 40 ;
         }
      }
      $prevint = $int ;
      close PPM ;
      print STDERR "$fn" ;
   }
}
@predefined = qw(acos addi addf apply asin clampf cone cos cube cylinder
                difference divi divf eqi eqf floor frac get getx gety getz if
                intersect length lessi lessf light modi muli mulf negi negf
                plane point pointlight real render rotatex rotatey rotatez
                scale sin sphere spotlight sqrt subi subf translate
                union uscale) ;
for (@predefined) { $predefined{$_}++ }
sub fixvar {
   my $s = shift ;
   $s =~ s/-/_MINUS_HACK_/g ;
   return '$__' . $s ;
}
sub translate {
   my $w = shift ;
   return 'push @_s, sub {' if $w eq "{" ;
   return '_mark() ;' if $w eq "[" ;
   return '_array() ;' if $w eq "]" ;
   return '} ;' if $w eq "}" ;
   return 'my ' . fixvar($1) . ' = pop @_s ;' if $w =~ /^\/(.*)$/ ;
   return 'push @_s, 1 ;' if $w eq "true" ;
   return 'push @_s, 0 ;' if $w eq "false" ;
   if ($w =~ /^[a-zA-Z]/) {
      if ($predefined{$w}) {
                        # kill any trailing i or f in operators except divi
         $w =~ s/[if]$// if $w ne "divi" ;
         return '_' . $w . '() ;' ;
      }
      return 'push @_s, ' . fixvar($w) . ' ;' ;
   }
   return 'push @_s, ' . $w . ' ;' ; # assume anything else is a number
                                     # or string
}
while (<>) {
   s/%.*// ;     # kill comments
   s/([\[\]\{\}])/ $1 /g ; # surround obvious special chars with whitespace
   my $line = join " ", map { translate $_ } (split) ;
   $prog .= "$line\n" ;
}
eval $prog ;
$@ and die "$@" ;
print "\n" ;
exit(0) ;
