/**
 *   Next attempt at a rubik solver.  Like the previous, this does it
 *   based on solving each individual face.
 */
#define HALFTURN
#ifdef HALFTURN
#define NMOVES (18)
#define TWISTS (3)
#define DEPTH (10)
#else
#define NMOVES (12)
#define TWISTS (2)
#define DEPTH (11)
#endif
#define FACES (6)
#define M (48)
#define SYMMETRY (8)
#define POSITIONS (6)
#define MAXMOVES (40)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
int doall = 0 ;
/**
 *   Error.
 */
void error(char *s) {
   fprintf(stderr, "Error: %s\n", s) ;
   exit(10) ;
}
struct symfast {
   unsigned char c[8] ;
   int ss, charoff ;
   unsigned char *p ;
} ;
/**
 *   The number of states for four-edge and four-corner.
 */
#define EDGE4_SIZE (190080)
#define CORNER4_SIZE (136080)
#define CSIZE (16*81*8*7*6*5/5)
int topn, symp ;
int downcounter ;
struct symfast *symptr ;
int primaryseen ;
int symmseen ;
double left ;
double plus ;
double evaled = 0.0 ;
double lur = 0.0 ;
int bestval ;
long oldtime ;
int parity = 0 ;
int pwr[5] = {1, 3, 9, 27, 81} ; // 20
int heap[POSITIONS] ; // 24
struct symfast protosymfast ; // 28
unsigned char movehist[MAXMOVES] ; // 40
unsigned char solution[MAXMOVES] ; // 40
unsigned char top[M] ; // 48
double rube[20] ; // 160
unsigned char fmap[M][FACES] ; // 288
unsigned char map[M][NMOVES] ; // 864
unsigned short val3[256] ; // 1024
unsigned char primary[12*12*12*12] ;
unsigned char *memp[12*12*12*12] ;
struct symfast symfast[MAXMOVES][POSITIONS] ; // 6720
unsigned char symmetrical[12*12*12*12] ;
char filename[100] ;
unsigned char *edge4_dist ;
unsigned char *corner4_dist ;
/**
 *   These are the twist permutations for edges, labeling the
 *   edges
 *
 *      0      4   5     8
 *    1   2            9   10
 *      3      6   7     11
 *     UP       MID     DOWN
 *
 *   We only give the + twists.  The others we figure by repeated
 *   application.
 *
 *   Twists are in the order UFRDBL.
 */
char edge_twist_perm[FACES][4] = {
   { 0, 2, 3, 1 },
   { 3, 7, 11, 6 },
   { 2, 5, 10, 7 },
   { 9, 11, 10, 8 },
   { 0, 4, 8, 5 },
   { 1, 6, 9, 4 }
} ;
/**
 *   These are the twist permutations for the corners, labeling the
 *   corners
 *
 *     0   1       4   5
 *     2   3       6   7
 *      UP          DOWN
 */
char corner_twist_perm[FACES][4] = {
   { 0, 1, 3, 2 },
   { 2, 3, 7, 6 },
   { 3, 1, 5, 7 },
   { 4, 6, 7, 5 },
   { 1, 0, 4, 5 },
   { 0, 2, 6, 4 }
} ;
/**
 *   We define the orientations to hold fixed for FRBL and to swap
 *   for quarter twists of UF.
 */
unsigned char edge_change[FACES] = { 1, 0, 0, 1, 0, 0 } ;
/**
 *   For the edges, we need to also know what the twists are.  We
 *   assign UD to hold the twists alone, and FRBL to change them.
 */
unsigned char corner_change[FACES][4] = {
   { 0, 0, 0, 0 },
   { 1, 2, 1, 2 },
   { 1, 2, 1, 2 },
   { 0, 0, 0, 0 },
   { 1, 2, 1, 2 },
   { 1, 2, 1, 2 },
} ;
#define CUBIES (24)
/**
 *   Basic cube geometry.  The following array holds the new cubie
 *   position/location for 
 */
unsigned char edge_trans[NMOVES][CUBIES] ;
unsigned char corner_trans[NMOVES][CUBIES] ;
/**
 *   Calculate the above.
 */
void init_basic_geometry() {
   int i, t, m, c, f, o ;
   for (m=0; m<NMOVES; m++)
      for (c=0; c<CUBIES; c++) {
         edge_trans[m][c] = c ;
         corner_trans[m][c] = c ;
      }
   for (f=0; f<FACES; f++)
      for (t=1; t<4; t++) {
         m = f * TWISTS + t - 1 ;
         if (t > TWISTS)
            m-- ;
         for (i=0; i<4; i++) {
            for (o=0; o<2; o++)
               edge_trans[m][o+2*edge_twist_perm[f][i]] =
                  (o^(edge_change[f]&t&1))+2*edge_twist_perm[f][(i+t)%4] ;
            for (o=0; o<3; o++)
               corner_trans[m][o+3*corner_twist_perm[f][i]] =
                  (t == 2 ? o : (corner_change[f][i] + o) % 3) +
                     3*corner_twist_perm[f][(i+t)%4] ;
         }
      }
}
/**
 *   We also need to handle the eight symmetries that keep the up
 *   face upwards.  There are two parts to this.  One is how the
 *   cubies map to other cubies; the other is how the (original)
 *   positions of the cubies change.  The basic idea is we can do
 *   a symmetry operation with
 *
 *   for (i=0; i<4; i++) c[cubieperm[i]] = edgerot[s][c[i]] ;
 *   for (i=4; i<8; i++) c[cubieperm[i]] = cornerrot[s][c[i]] ;
 */
unsigned char edgerot[SYMMETRY][CUBIES] ;
unsigned char cornerrot[SYMMETRY][CUBIES] ;
unsigned char cubieperm[SYMMETRY][8] ;
/**
 *   The next, to the left, of the edges and corners, and also the
 *   flipped position, when rotating the whole cube.
 */
unsigned char edgeleft[20] = { 1, 3, 0, 2, 6, 4, 7, 5, 9, 11, 8, 10 } ;
unsigned char cornerleft[12] = { 2, 0, 3, 1, 6, 4, 7, 5 } ;
unsigned char edgeflip[20] = { 0, 2, 1, 3, 5, 4, 7, 6, 8, 10, 9, 11 } ;
unsigned char cornerflip[20] = { 1, 0, 3, 2, 5, 4, 7, 6 } ;
unsigned char flip3[3] = { 0, 2, 1 } ;
/**
 *   Initialize the above arrays.
 */
void init_rotations() {
   int i, j ;
   for (i=0; i<4; i++) {
      cubieperm[0][i] = i ;
      cubieperm[0][i+4] = i + 4 ;
      cubieperm[4][i] = edgeflip[i] ;
      cubieperm[4][i+4] = cornerflip[i] + 4 ;
   }
   for (i=1; i<4; i++) {
      for (j=0; j<4; j++) {
         cubieperm[i][j] = edgeleft[cubieperm[i-1][j]] ;
         cubieperm[i][j+4] = cornerleft[cubieperm[i-1][j+4]-4]+4 ;
         cubieperm[i+4][j] = edgeflip[edgeleft[edgeflip[cubieperm[i+3][j]]]] ;
         cubieperm[i+4][j+4] = cornerflip[cornerleft[cornerflip[
                                                  cubieperm[i+3][j+4]-4]]]+4 ;
      }
   }
   for (i=0; i<24; i++) {
      edgerot[0][i] = i ;
      cornerrot[0][i] = i ;
      edgerot[4][i] = 2*edgeflip[i/2]+i%2 ;
      cornerrot[4][i] = 3*cornerflip[i/3]+flip3[i%3] ;
      for (j=1; j<4; j++) {
         int ii = edgerot[j-1][i] ;
         edgerot[j][i] = 2*edgeleft[ii/2]+ii%2 ;
         ii = cornerrot[j-1][i] ;
         cornerrot[j][i] = 3*cornerleft[ii/3]+ii%3 ;
         ii = edgerot[j+3][i] ;
         edgerot[4+j][i] = 2*edgeflip[edgeleft[edgeflip[ii/2]]]+ii%2 ;
         ii = cornerrot[j+3][i] ;
         cornerrot[4+j][i] = 3*cornerflip[cornerleft[cornerflip[ii/3]]]+ii%3 ;
      }
   }
}
/**
 *   Frequently we need to compress four corner cubies or four edge
 *   cubies into a dense representation.  The following arrays do this
 *   but do it for the positions separately from the orientation.
 *
 *   For edges, edge_expand[i][e] = floor(e/2)*(12**i)*16+(e&1)*(2**i).
 *   Thus, the low 4 bits are the orientation, and the next 15 are
 *   the index into the edge_compact array.
 */
int edge_expand[4][CUBIES] ;
int edge_expand_sym[SYMMETRY][4][CUBIES] ;
int edge_compact[12*12*12*12] ;
/**
 *   For corners, corner_expand[i][i] = floor(e/3)*(8**i)*128+(e%3)*(3**i)
 *   Thus, the low 7 bits are the orientation, and the next 12 are
 *   the index into the corner_compact array.  We also use corner_compact2
 *   which returns in the low 4 bits the permuation mod 5, and in the
 *   next 19 bits 81 * times the permutation divided by 5.
 */
int corner_expand[4][CUBIES] ;
int corner_expand_sym[SYMMETRY][4][CUBIES] ;
int corner_compact[8*8*8*8] ;
int corner_compact2[8*8*8*8] ;
/**
 *   To scale the orientations above the permutations, we use the
 *   following multiplication arrays.
 */
int corner_mul[81], edge_mul[16] ;
/**
 *   Build these arrays.
 */
void init_expand() {
   int i, j, k, m, n ;
   int pow3[4], pow12[4] ;
   pow3[0] = pow12[0] = 1 ;
   for (i=1; i<4; i++) {
      pow3[i] = pow3[i-1]*3 ;
      pow12[i] = pow12[i-1]*12 ;
   }
   for (i=0; i<4; i++) {
      for (j=0; j<CUBIES; j++) {
         edge_expand[i][j] = (j/2)*pow12[i]*16+((j%2)<<i) ;
         corner_expand[i][j] = ((j/3)<<(3*i))*128+(j%3)*pow3[i] ;
      }
   }
   n = 0 ;
   for (i=0; i<12; i++)
      for (j=0; j<12; j++)
         for (k=0; k<12; k++)
            for (m=0; m<12; m++)
               if (i != j && i != k && i != m && j != k && j != m && k != m)
                  edge_compact[i*12*12*12+j*12*12+k*12+m] = n++ ;
   for (i=0; i<16; i++)
      edge_mul[i] = i * n ;
   n = 0 ;
   for (i=0; i<8; i++)
      for (j=0; j<8; j++)
         for (k=0; k<8; k++)
            for (m=0; m<8; m++)
	       if (i != j && i != k && i != m && j != k && j != m && k != m) {
		  corner_compact[i*8*8*8+j*8*8+k*8+m] = n ;
		  corner_compact2[i*8*8*8+j*8*8+k*8+m] = 
                                             2 * (n % 5) + 16 * 81 * (n / 5) ;
                  n++ ;
               }
   for (i=0; i<81; i++)
      corner_mul[i] = i * n ;
}
/**
 *   We build the distance array in a straightforward way.  We initialize
 *   the start state at 0, all others at 100.  Then, we do a breadth-first
 *   search.
 */
void edge_explor4() {
   int i, added, dist, c[4], ii ;
   edge4_dist = malloc(EDGE4_SIZE) ;
   if (edge4_dist == 0)
      error("! no memory") ;
   memset(edge4_dist, 100, EDGE4_SIZE) ;
   for (i=0; i<4; i++)
      c[i] = i * 2 ;
   ii = edge_expand[0][c[0]] + edge_expand[1][c[1]] + edge_expand[2][c[2]] +
        edge_expand[3][c[3]] ;
   ii = edge_compact[ii>>4] + edge_mul[ii&15] ;
   edge4_dist[ii] = 0 ;
   printf("1") ;
   for (dist=0; dist<100; dist++) {
      added = 0 ;
      for (c[0]=0; c[0]<CUBIES; c[0]++)
         for (c[1]=0; c[1]<CUBIES; c[1]++)
            for (c[2]=0; c[2]<CUBIES; c[2]++)
               for (c[3]=0; c[3]<CUBIES; c[3]++)
                  if (c[0] != c[1] && c[0] != c[2] && c[0] != c[3] &&
                      c[1] != c[2] && c[1] != c[3] && c[2] != c[3]) {
                     ii = edge_expand[0][c[0]] + edge_expand[1][c[1]] +
                          edge_expand[2][c[2]] + edge_expand[3][c[3]] ;
                     ii = edge_compact[ii>>4] + edge_mul[ii&15] ;
                     if (edge4_dist[ii] == dist) {
                        int m, c2[4] ;
                        for (m=0; m<NMOVES; m++) {
                           c2[0] = edge_trans[m][c[0]] ;
                           c2[1] = edge_trans[m][c[1]] ;
                           c2[2] = edge_trans[m][c[2]] ;
                           c2[3] = edge_trans[m][c[3]] ;
                           ii = edge_expand[0][c2[0]] + edge_expand[1][c2[1]] +
                                edge_expand[2][c2[2]] + edge_expand[3][c2[3]] ;
                           ii = edge_compact[ii>>4] + edge_mul[ii&15] ;
                           if (edge4_dist[ii] == 100) {
                              edge4_dist[ii] = dist + 1 ;
                              added++ ;
                           }
                        }
                     }
                  }
      if (added == 0)
         break ;
      printf(".%d", added) ;
      fflush(stdout) ;
   }
   printf("\n") ;
}
/**
 *   Same, but for corners.
 */
void corner_explor4() {
   int i, added, dist, c[4], ii ;
   corner4_dist = malloc(CORNER4_SIZE) ;
   if (corner4_dist == 0)
      error("! no memory") ;
   memset(corner4_dist, 100, CORNER4_SIZE) ;
   for (i=0; i<4; i++)
      c[i] = i * 3 ;
   ii = corner_expand[0][c[0]] + corner_expand[1][c[1]] +
        corner_expand[2][c[2]] + corner_expand[3][c[3]] ;
   ii = corner_compact[ii>>7] + corner_mul[ii&127] ;
   corner4_dist[ii] = 0 ;
   printf("1") ;
   for (dist=0; dist<100; dist++) {
      added = 0 ;
      for (c[0]=0; c[0]<CUBIES; c[0]++)
         for (c[1]=0; c[1]<CUBIES; c[1]++)
            for (c[2]=0; c[2]<CUBIES; c[2]++)
               for (c[3]=0; c[3]<CUBIES; c[3]++)
                  if (c[0] != c[1] && c[0] != c[2] && c[0] != c[3] &&
                      c[1] != c[2] && c[1] != c[3] && c[2] != c[3]) {
                     ii = corner_expand[0][c[0]] + corner_expand[1][c[1]] +
                          corner_expand[2][c[2]] + corner_expand[3][c[3]] ;
                     ii = corner_compact[ii>>7] + corner_mul[ii&127] ;
                     if (corner4_dist[ii] == dist) {
                        int m, c2[4] ;
                        for (m=0; m<NMOVES; m++) {
                           c2[0] = corner_trans[m][c[0]] ;
                           c2[1] = corner_trans[m][c[1]] ;
                           c2[2] = corner_trans[m][c[2]] ;
                           c2[3] = corner_trans[m][c[3]] ;
                           ii = corner_expand[0][c2[0]] +
  corner_expand[1][c2[1]] + corner_expand[2][c2[2]] + corner_expand[3][c2[3]] ;
                           ii = corner_compact[ii>>7] + corner_mul[ii&127] ;
                           if (corner4_dist[ii] == 100) {
                              corner4_dist[ii] = dist + 1 ;
                              added++ ;
                           }
                        }
                     }
                  }
      if (added == 0)
         break ;
      printf(".%d", added) ;
      fflush(stdout) ;
   }
   printf("\n") ;
}
/**
 *   Next, build the 48 symmetries.
 */
int cornerloc[4][3] = {{0, 1, 2}, {2, 3, 4}, {4, 5, 0}, {0, 2, 4}} ;
int facemap(int op, int corner, int tw, int neg, int inface) {
   if (inface >= 3)
      return (3 + facemap(op, corner, tw, neg, inface-3)) % 6 ;
   if (op)
      return 5 - facemap(0, corner, tw, neg, inface) ;
   if (neg)
      return facemap(op, corner, tw, 0, 2-inface) ;
   return cornerloc[corner][(inface+tw) % 3] ;
}
/**
 *   Force the order generated.
 */
unsigned short rotorder[SYMMETRY] = { 12, 24, 45, 51, 15, 54, 42, 21 } ;
void genmoves(int op, int corner, int tw, int neg) {
   int f ;
   for (f=0; f<FACES; f++) {
      int fmapped = facemap(op, corner, tw, neg, f) ;
      fmap[symp][f] = fmapped ;
      fmapped *= TWISTS ;
#ifdef HALFTURN
      if (neg) {
         map[symp][3*f] = fmapped + 2 ;
         map[symp][3*f+1] = fmapped + 1 ;
         map[symp][3*f+2] = fmapped ;
      } else {
         map[symp][3*f] = fmapped ;
         map[symp][3*f+1] = fmapped + 1 ;
         map[symp][3*f+2] = fmapped + 2 ;
      }
#else
      if (neg) {
         map[symp][2*f] = fmapped + 1 ;
         map[symp][2*f+1] = fmapped ;
      } else {
         map[symp][2*f] = fmapped ;
         map[symp][2*f+1] = fmapped + 1 ;
      }
#endif
   }
   if (fmap[symp][0] == 0) {
      int mat = fmap[symp][1] * 10 + fmap[symp][2] ;
      for (topn=0; topn<SYMMETRY; topn++)
         if (rotorder[topn] == mat)
            break ;
      top[topn] = symp ;
   }
   symp++ ;
}
/*
 *   Re-sort the symmetries so they match 0.
 */
void symmresort() {
   int i, j, f ;
   unsigned char safemap[M][NMOVES] ;
   unsigned char safefmap[M][FACES] ;
   unsigned char temptop[M] ;
   memcpy(safemap, map, sizeof(map)) ;
   memcpy(safefmap, fmap, sizeof(fmap)) ;
   topn = 8 ;
   for (f=1; f<POSITIONS; f++) {
      int baseface = -1 ;
      for (i=0; i<M; i++)
         if (fmap[i][f] == 0) {
            baseface = i ;
            break ;
         }
      for (i=0; i<M; i++) {
         if (fmap[i][f] == 0) {
            if (i == baseface) {
               top[topn] = i ;
            } else { // where does this go?
               int assigned = 0 ;
               for (j=1; j<SYMMETRY; j++) {
                  if (fmap[top[j]][fmap[baseface][0]] == fmap[i][0] &&
                      fmap[top[j]][fmap[baseface][1]] == fmap[i][1] &&
                      fmap[top[j]][fmap[baseface][2]] == fmap[i][2]) {
                     top[topn+j] = i ;
                     assigned++ ;
                  }
               }
               if (assigned != 1)
                  error("! bad assignment?") ;
            }
         }
      }
      topn += SYMMETRY ;
   }
   memcpy(temptop, top, sizeof(top)) ;
   for (i=0; i<POSITIONS; i++)
      for (j=0; j<SYMMETRY; j++)
         top[j * POSITIONS + i] = temptop[i * SYMMETRY + j] ;
   for (i=0; i<M; i++) {
      memcpy(map+i, safemap+top[i], sizeof(map[i])) ;
      memcpy(fmap+i, safefmap+top[i], sizeof(fmap[i])) ;
   }
}
/**
 *   Gen and sort the symmetries.
 */
void gensymm() {
   int neg, op, tw, corner ;
   for (neg=0; neg<2; neg++)
      for (op=0; op<2; op++)
         for (tw=0; tw<3; tw++)
            for (corner=0; corner<4; corner++)
               genmoves(op, corner, tw, neg) ;
   symmresort() ;
}
/**
 *   Expand the symm arrays.
 */
void expandsymm() {
   int s, i, c ;
   for (s=0; s<SYMMETRY; s++) 
      for (i=0; i<4; i++)
	for (c=0; c<CUBIES; c++) {
           edge_expand_sym[s][i][c] =
                                 edge_expand[cubieperm[s][i]][edgerot[s][c]] ;
           corner_expand_sym[s][i][c] = 
                         corner_expand[cubieperm[s][i+4]-4][cornerrot[s][c]] ;
        }
}
/**
 *   Next concern:  exploit 8-way symmetry about the up face.
 */
void genprim(int i, int j, int k, int m) {
   int c[8][4], s, ss[8], issym=0 ;
   c[0][0] = 2 * i ;
   c[0][1] = 2 * j ;
   c[0][2] = 2 * k ;
   c[0][3] = 2 * m ;
   for (s=0; s<8; s++) {
      for (i=0; i<4; i++)
         c[s][cubieperm[s][i]] = edgerot[s][c[0][i]] ;
      ss[s] = (edge_expand[0][c[s][0]] + edge_expand[1][c[s][1]] +
               edge_expand[2][c[s][2]] + edge_expand[3][c[s][3]]) >> 4 ;
   }
   s = 0 ;
   for (i=1; i<8; i++)
      if (ss[i] < ss[s]) {
         s = i ;
         issym = 0 ;
      } else if (ss[i] == ss[s])
         issym++ ;
   primary[ss[0]] = s ;
   symmetrical[ss[0]] = issym ;
   if (s == 0) {
      primaryseen++ ;
      if (issym)
         symmseen++ ;
      memp[ss[0]] = calloc(1, CSIZE) ;
      if (memp[ss[0]] == 0)
         error("! no memory") ;
   }
}
void genprotos() {
   int i ;
   struct symfast *pf = &protosymfast ;
   unsigned char *c = pf->c ;
   for (i=0; i<4; i++)
      c[i] = i*2 ;
   for (i=0; i<4; i++)
      c[i+4] = i*3 ;
   pf->ss = edge_expand[0][c[0]] +
         edge_expand[1][c[1]] + edge_expand[2][c[2]] + edge_expand[3][c[3]] ;
}
int checkzero(struct symfast *p) {
   int i ;
   for (i=0; i<8; i++)
      if (protosymfast.c[i] != p->c[i])
         return 0 ;
   return 1 ;
}
int inputMoveCount ;
#define MAXINPUTMOVES (1000)
unsigned char inputMoves[MAXINPUTMOVES] ;
void initM() {
   int i ;
   for (i=0; i<M; i++) // this does too much but that's okay
      symfast[0][i] = protosymfast ;
}
void genprimary() {
   int i, j, k, m ;
   for (i=0; i<12; i++)
      for (j=0; j<12; j++)
         for (k=0; k<12; k++)
            for (m=0; m<12; m++)
               if (i != j && i != k && i != m && j != k && j != m && k != m)
                  genprim(i, j, k, m) ;
   printf("Out of %d we saw %d primary %d symmetrical\n",
                                     12*11*10*9, primaryseen, symmseen) ;
}
static inline int lookupslow(struct symfast *p) {
   int cf2 = corner_expand[0][p->c[4]] + corner_expand[1][p->c[5]] +
             corner_expand[2][p->c[6]] + corner_expand[3][p->c[7]] ;
   int cc2 = corner_compact2[cf2>>7] ;
   p->p = memp[p->ss>>4] + (cc2 & ~0xf) + (p->ss & 0xf) + ((cf2 & 0x7f) << 4) ;
   p->charoff = cc2 & 0xf ;
   return (val3[*(p->p)] >> p->charoff) & 3 ;
}
void lookupbump(struct symfast *p) {
   *(p->p) += pwr[p->charoff >> 1] ;
}
int lookup(struct symfast *p) {
   int cf2 = corner_expand[0][p->c[4]] + corner_expand[1][p->c[5]] +
             corner_expand[2][p->c[6]] + corner_expand[3][p->c[7]] ;
   int cc2 = corner_compact2[cf2>>7] ;
   return (val3[memp[p->ss>>4][(cc2 & ~0xf) + (p->ss & 0xf) +
                                    ((cf2 & 0x7f) << 4)]] >> (cc2 & 0xf)) & 3 ;
}
/*
 *   Rotate and lookup.
 *
 *   char *cp = cubieperm[s] ;
 *   char *er = edgerot[s] ;
 */
static inline int lookuprot(struct symfast *p) {
   int s = primary[p->ss>>4] ;
   int (*ee)[CUBIES] = edge_expand_sym[s] ;
   unsigned char *c = p->c ;
   int ss = ee[0][c[0]] + ee[1][c[1]] + ee[2][c[2]] + ee[3][c[3]] ;
   int (*ce)[CUBIES] = corner_expand_sym[s] ;
   int ss2 = ce[0][c[4]] + ce[1][c[5]] + ce[2][c[6]] + ce[3][c[7]] ;
   int cc2 = corner_compact2[ss2>>7] ;
   return (val3[memp[ss>>4][(cc2 & ~0xf) + (ss & 0xf) +
                                    ((ss2 & 0x7f) << 4)]] >> (cc2 & 0xf)) & 3 ;
}
static inline void forward(struct symfast *src, struct symfast *dst, int m) {
   unsigned char *c = edge_trans[m] ;
   dst->c[0] = c[src->c[0]] ;
   dst->c[1] = c[src->c[1]] ;
   dst->c[2] = c[src->c[2]] ;
   dst->c[3] = c[src->c[3]] ;
   c = corner_trans[m] ;
   dst->c[4] = c[src->c[4]] ;
   dst->c[5] = c[src->c[5]] ;
   dst->c[6] = c[src->c[6]] ;
   dst->c[7] = c[src->c[7]] ;
   c = dst->c ;
   dst->ss = edge_expand[0][c[0]] +
         edge_expand[1][c[1]] + edge_expand[2][c[2]] + edge_expand[3][c[3]] ;
}
static inline struct symfast *forwardwhich(
                           struct symfast *src, struct symfast *dst, int om) {
   unsigned char *c = edge_trans[om] ;
   int low, m ;
   dst->c[0] = c[src->c[0]] ;
   dst->c[1] = c[src->c[1]] ;
   dst->c[2] = c[src->c[2]] ;
   dst->c[3] = c[src->c[3]] ;
   c = dst->c ;
   dst->ss = edge_expand[0][c[0]] +
         edge_expand[1][c[1]] + edge_expand[2][c[2]] + edge_expand[3][c[3]] ;
   low = primary[dst->ss >> 4] ;
   m = map[low*POSITIONS][om] ;
   src += low ;
   dst += low ;
   forward(src, dst, m) ;
   return dst ;
}
/**
 *   Now we fill in the big huge array.
 */
/* double stats[10][10] ; */
int bumpuniq(struct symfast *ord, struct symfast *p) {
   struct symfast *bumped[SYMMETRY] ;
   int i, same=0, bumpn = 0 ;
   int off = ord->ss >> 4 ;
   for (i=0; i<SYMMETRY; i++, p++) {
      if (p->ss >> 4 == off) {
         int j ;
         for (j=0; j<bumpn; j++)
	    if (bumped[j]->ss == p->ss && bumped[j]->c[4] == p->c[4] &&
                bumped[j]->c[5] == p->c[5] && bumped[j]->c[6] == p->c[6] &&
                bumped[j]->c[7] == p->c[7])
	       break ;
         if (j>=bumpn) {
            bumped[bumpn++] = p ;
            lookupslow(p) ;
            lookupbump(p) ;
         } else if (j == 0) {
            same++ ;
         }
      }
   }
/* stats[bumpn][same]++ ; */
   return SYMMETRY - (SYMMETRY / (same + 1)) ;
}
double xplor1(struct symfast *p) {
   double r = 0.0 ;
   int f, i, j ;
   int low = 0 ;
   low = primary[p->ss>>4] ;
   for (f=0; f<6; f++) {
      for (i=0; i<TWISTS; i++) {
         int m = f * TWISTS + i ;
         struct symfast *pp = forwardwhich(p, p+SYMMETRY, m) ;
         if (lookupslow(pp) == 0) {
            r++ ;
            if (symmetrical[pp->ss>>4]) {
               for (j=0; j<SYMMETRY; j++)
                  forward(p+j, p+SYMMETRY+j, map[j*POSITIONS][m]) ;
               plus += bumpuniq(pp, p+SYMMETRY) ;
            } else {
               lookupbump(pp) ;
            }
         }
      }
   }
   if (symmetrical[p[low].ss>>4]) {
      bumpuniq(p + low, p) ;
   } else {
      lookupbump(p + low) ;
   }
   left-- ;
   return r ;
}
double xplor2(struct symfast *p) {
   double r = 0.0 ;
   int f, i, j ;
   for (f=0; f<6; f++) {
      for (i=0; i<TWISTS; i++) {
         int m = f * TWISTS + i ;
         struct symfast *pp = forwardwhich(p, p+SYMMETRY, m) ;
         if (lookupslow(pp) != 2) {
            for (j=0; j<SYMMETRY; j++)
               forward(p+j, p+j+SYMMETRY, map[j*POSITIONS][m]) ;
            r += xplor1(p+SYMMETRY) ;
         }
      }
   }
   return r ;
}
#ifdef HALFTURN
double xplorn(struct symfast *p, int depthleft, int prev) {
#else
double xplorn(struct symfast *p, int depthleft, int prev, int lastplus) {
#endif
   double r = 0.0 ;
   int f, i, j ;
   for (f=0; f<6; f++) {
#ifdef HALFTURN
      if (f == prev || f + 3 == prev)
         continue ; // don't allow moving the same face twice
#else
      if (f + 3 == prev || (f == prev && !lastplus))
         continue ; // don't allow moving the same face twice
#endif
      for (i=0; i<TWISTS; i++) {
         int m = f * TWISTS + i ;
         forward(p, p+SYMMETRY, map[0][m]) ;
         if (depthleft == 3) {
            if (primary[p[SYMMETRY].ss>>4] == 0) {
               for (j=1; j<8; j++)
                  forward(p+j, p+j+SYMMETRY, map[j*POSITIONS][m]) ;
               r += xplor2(p+SYMMETRY) ;
            }
         } else {
            for (j=1; j<8; j++)
               forward(p+j, p+j+SYMMETRY, map[j*POSITIONS][m]) ;
#ifdef HALFTURN
            r += xplorn(p+SYMMETRY, depthleft-1, f) ;
#else
            r += xplorn(p+SYMMETRY, depthleft-1, f, i==0 && f != prev) ;
#endif
         }
         if (left == 0.0)
            return r ;
#ifndef HALFTURN
         if (f == prev)
	    break ;
#endif
      }
   }
   return r ;
}
double xplor(struct symfast *p, int depthleft) {
   if (depthleft == 1) {
      return xplor1(p) ;
   } else if (depthleft == 2) {
      return xplor2(p) ;
   } else {
#ifdef HALFTURN
      return xplorn(p, depthleft, -100) ;
#else
      return xplorn(p, depthleft, -100, 0) ;
#endif
   }
}
/**
 *   Get the filename.
 */
char *getFileName() {
#ifdef HALFTURN
   sprintf(filename, "rub3h%d.dat", DEPTH) ;
#else
   sprintf(filename, "rub3q%d.dat", DEPTH) ;
#endif
   return filename ;
}
/**
 *   Write the table out.
 */
void writetab() {
   int i ;
   FILE *f = fopen(getFileName(), "w") ;
   if (f == 0)
      error("! can't open file") ;
   for (i=0; i<sizeof(memp)/sizeof(memp[0]); i++)
      if (memp[i]) 
         fwrite(memp[i], 1, CSIZE, f) ;
   fclose(f) ;
}
/**
 *   Read the table in.
 */
int readtab() {
   int target = 0 ;
   int i, j=0 ;
   FILE *f = fopen(getFileName(), "r") ;
   if (f == 0)
      return 0 ;
   for (i=0; i<sizeof(memp)/sizeof(memp[0]); i++)
      if (memp[i]) {
         fread(memp[i], 1, CSIZE, f) ;
         j++ ;
         if (j * 100 > target * 1547) {
            printf(".%d", target) ;
            fflush(stdout) ;
            target += 5 ;
         }
      }
   printf("\n") ;
   fclose(f) ;
   return 1 ;
}
void cross() {
   int dep ;
   struct symfast *p ;
   initM() ;
   p = symfast[0] ;
   dep = primary[p->ss>>4] ;
   if (symmetrical[p->ss>>4]) {
      bumpuniq(p+dep, p) ;
   } else {
      lookupslow(p+dep) ;
      lookupbump(p+dep) ;
   }
   left = 1 ;
   for (dep=1; dep<=DEPTH; dep++) {
      double mor ;
      plus = 0.0 ;
      mor = xplor(symfast[0], dep) ;
      printf("Depth %d found %.15g plus %.15g\n", dep, mor, plus) ;
      fflush(stdout) ;
      left = mor ;
   }
}
/**
 *   Initialize everything.
 */
void init() {
   int i, j ;
   for (i=0; i<256; i++)
      for (j=0; j<5; j++)
         val3[i] += ((i / pwr[j]) % 3) << (2 * j) ;
   init_basic_geometry() ;
   init_rotations() ;
   init_expand() ;
   edge_explor4() ;
   corner_explor4() ;
   gensymm() ;
   genprotos() ;
   genprimary() ;
   expandsymm() ;
   if (!readtab()) {
      cross() ;
      writetab() ;
   }
   for (i=0; i<POSITIONS; i++)
      heap[i] = i ;
}
int edgedepth(struct symfast *p) {
   return edge4_dist[edge_compact[p->ss>>4]+edge_mul[p->ss&15]] ;
}
int cornerdepth(struct symfast *p) {
   int ii = corner_expand[0][p->c[4]] + corner_expand[1][p->c[5]] +
            corner_expand[2][p->c[6]] + corner_expand[3][p->c[7]] ;
   return corner4_dist[corner_compact[ii>>7]+corner_mul[ii&127]] ;
}
int rubevalprint() {
   int i, s=0 ;
   for (i=0; i<POSITIONS; i++) {
      struct symfast *p = symfast[0] + i ;
      int mm = lookuprot(p) ;
      if (mm == 0) {
         mm = DEPTH+1 ;
      } else if (mm == 1) {
         mm = DEPTH ;
      } else {
         mm = 0 ;
      }
      if (mm == 0) {
         int m1 = edgedepth(p) ;
         int m2 = cornerdepth(p) ;
         if (m1 < m2)
            mm = m2 ;
         else
            mm = m1 ;
      }
      if (mm > s)
         s = mm ;
      printf("%c", 'a' + mm) ;
   }
   printf(" %d\n", s) ;
   return s ;
}
int rubeval(int left) {
   int i, ii, s=2, bonk=DEPTH-left ;
   int m = movehist[left+1] ;
   evaled++ ;
   rube[left]++ ;
   if (bonk >= 2)
      bonk = 1 ;
   for (ii=0; ii<POSITIONS; ii++) {
      int mm, jj, i = heap[ii] ;
      struct symfast *p = symptr + i - POSITIONS * (left + 1) ;
      forward(p, p+POSITIONS, map[i][m]) ;
      p += POSITIONS ;
      mm = lookuprot(p) ;
      if (mm <= bonk) {
         lur += ii + 1 ;
         while (ii > 0) {
            jj = heap[ii] ;
            heap[ii] = heap[ii-1] ;
            heap[ii-1] = jj ;
            ii-- ;
         }
         return DEPTH - mm + 1 ;
      }
      if (mm < s)
         s = mm ;
   }
   lur += POSITIONS ;
   if (s == 2) {
      if (left >= DEPTH - 2) {
         s = 0 ;
      } else {
         int m1 = 0 ;
         int m2 = 0 ;
         struct symfast *p = symptr - POSITIONS * left ;
         s = 0 ;
         for (i=0; i<POSITIONS; i++, p++) {
            m1 = edgedepth(p) ;
            m2 = cornerdepth(p) ;
            if (m2 > m1)
               m1 = m2 ;
            if (m1 > s)
               s = m1 ;
         }
      }
   } else {
      s = DEPTH - s + 1 ;
   }
   return s ;
}
void mv(int m) {
   int i ;
   for (i=0; i<POSITIONS; i++) {
      struct symfast *p = symfast[0] + i ;
      int mm = map[i][m] ;
      forward(p, p, mm) ;
   }
}
/**
 *   Reduce by symmetry; find those paths we don't need to explore.
 *   Only useful for symmetrical inputs.
 */
#ifdef HALFTURN
#define SYMMMOVES 3
#define MAXPATT 3240
#define TOTPATT (3240+243+18+1)
#define SKIPSIZE (NMOVES * (1 + NMOVES * (1 + NMOVES)))
#define HASHPRIME 5231
#else
#define SYMMMOVES 4
#define MAXPATT 10011
#define TOTPATT (10011+1068+114+12+1)
#define SKIPSIZE (NMOVES * (1 + NMOVES * (1 + NMOVES * (1 + NMOVES))))
#define HASHPRIME 16001
#endif
unsigned char skip[SKIPSIZE] ;
int solcount = 0 ;
int glook ;
void showsol() {
   int i ;
   for (i=glook; i>0; i--) {
       printf("%c%c", "UFRDBL"[solution[i]/TWISTS],
#ifdef HALFTURN
                      "+2-"[solution[i]%3]) ;
#else
                      "+-"[solution[i]%2]) ;
#endif
   }
   printf("\n") ;
   fflush(stdout) ;
}
#ifdef HALFTURN
int peekahead(int left, int prev, int skipval) {
#else
int peekahead(int left, int prev, int lastplus, int skipval) {
#endif
   int i, f ;
   int m = movehist[left+1] ;
   if (skipval >= -1) {
      skipval = (skipval + 1) * NMOVES + m ;
      if (skipval >= SKIPSIZE)
         skipval = -2 ;
      else {
         if (skip[skipval])
            return 0 ;
      }
   }
   if (DEPTH >= left) {
      int myv = rubeval(left) ;
      if (myv > left)
         return myv - 1 - left ;
      if (left == 0) {
         if (myv == 0) {
            if (doall) {
               if (solcount == 0) {
                  printf("!%d\n", glook) ;
               }
               solcount++ ;
               memcpy(solution, movehist, 30) ;
               showsol() ;
            } else {
               printf("!") ;
               memcpy(solution, movehist, 30) ;
               fflush(stdout) ;
               bestval = 0 ;
            }
         }
         return 0 ;
      }
   } else {
      struct symfast *p = symptr - POSITIONS * (left + 1) ;
      for (i=0; i<POSITIONS; i++, p++)
         forward(p, p+POSITIONS, map[i][m]) ;
   }
   for (f=0; f<6; f++) {
#ifdef HALFTURN
      if (f == prev || f + 3 == prev)
         continue ; // don't allow moving the same face twice
#else
      if (f + 3 == prev || (f == prev && !lastplus))
         continue ; // don't allow moving the same face twice
#endif
      for (i=0; i<TWISTS; i++) {
#ifdef HALFTURN
         int t ;
#endif
         m = f * TWISTS + i ;
         movehist[left] = m ;
#ifdef HALFTURN
         t = peekahead(left-1, f, skipval) ;
#else
         peekahead(left-1, f, i==0 && f != prev, skipval) ;
#endif
         if (bestval == 0)
            return 0 ;
#ifdef HALFTURN
         if (t)
            break ;
#else
         if (f == prev)
	    break ;
#endif
      }
   }
   return 0 ;
}
int peekaheadouter(int left) {
   int i, f ;
   for (f=0; f<6; f++) {
      for (i=0; i<TWISTS; i++) {
         int m = f * TWISTS + i ;
         movehist[left] = m ;
#ifdef HALFTURN
         peekahead(left-1, f, -1) ;
#else
         peekahead(left-1, f, i==0, -1) ;
#endif
         if (bestval == 0)
            return bestval ;
      }
   }
   return bestval ;
}
int lasttime() {
   long newtime = time(0) ;
   int r = newtime - oldtime ;
   oldtime = newtime ;
   return r ;
}
int rubsolv() {
   int value = rubevalprint() ;
   int i, look ;
   solcount = 0 ;
   if (value == 0)
      return 0 ;
   bestval = 1 ;
   evaled = 0.0 ;
   lur = 0.0 ;
#ifdef HALFTURN
   for (look=value; ;look++) {
#else
 printf("Parity %d value %d\n", parity, value) ;
   if ((parity ^ value) & 1)
      value++ ;
   for (look=value; ;look += 2) {
#endif
      glook = look ;
      symptr = symfast[0] + look * POSITIONS ;
      if (look > 0) peekaheadouter(look) ;
      printf("%d", look) ;
      fflush(stdout) ;
      if (solcount > 0) {
         printf("Found %d solutions\n", solcount) ;
         break ;
      } else if (bestval == 0) {
         struct symfast psafe[M] ;
         memcpy(psafe, symfast, sizeof(struct symfast)*M) ;
         printf("\n") ;
         showsol() ;
         for (i=look; i>0; i--)
            mv(solution[i]) ;
         printf("\n") ;
         for (i=0; i<POSITIONS; i++)
            if (!checkzero(symfast[0]+i))
               error("! bad solution") ;
         memcpy(symfast, psafe, sizeof(struct symfast)*M) ;
         break ;
      }
   }
   printf(": ") ;
   printf("Solved in %d moves %d secs %.15g evals\n", look, lasttime(),
                                                                    evaled) ;
   for (i=0; i<sizeof(rube)/sizeof(rube[0]); i++)
      if (rube[i] > 0.0)
         printf("%d:%.15g ", i, rube[i]) ;
   printf("%.15g lur\n", lur) ;
   return look ;
}
void docube() {
   initM() ;
   lasttime() ;
   while (1) {
      int i, m = 0 ;
      rubsolv() ;
      for (i=0; i<1000; i++) {
         m = lrand48() % 18 ;
//       printf("Moving %c%c\n", "UFRDBL"[m/3], "+2-"[m%3]) ;
#ifdef HALFTURN
         mv(m) ;
#else
         {
            int f = m / 3 ;
            m = m % 3 ;
            if (m == 1) {
               mv(f*TWISTS) ;
               mv(f*TWISTS) ;
            } else {
               mv(f*TWISTS + m/2) ;
               parity++ ;
            }
         }
#endif
      }
   }
}
void symmov(char *s) {
   printf(" %s", s) ;
   while (*s) {
      int m = 0 ;
      int cnt = 1 ;
      if (*s == '/')
         break ;
      switch (*s) {
case 'u': case 'U':  m = 0 ; break ;
case 'f': case 'F':  m = 1 ; break ;
case 'r': case 'R':  m = 2 ; break ;
case 'd': case 'D':  m = 3 ; break ;
case 'b': case 'B':  m = 4 ; break ;
case 'l': case 'L':  m = 5 ; break ;
default: error("! Bad move argument") ; break ;
      }
      m *= TWISTS ;
      s++ ;
      if (*s == '+' || *s == '1')
         s++ ;
      else if (*s == '2') {
         s++ ;
         cnt = 2 ;
      } else if (*s == '\'' || *s == '-' || *s == '3') {
         s++ ;
         cnt = 3 ;
      }
      parity += cnt ;
#ifdef HALFTURN
      inputMoves[inputMoveCount++] = m + cnt - 1 ;
#else
      if (cnt == 2) {
         inputMoves[inputMoveCount++] = m ;
         inputMoves[inputMoveCount++] = m ;
      } else {
         inputMoves[inputMoveCount++] = m + (cnt >> 1) ;
      }
#endif
      while (cnt-- > 0)
         mv(m) ;
      while (*s && *s <= ' ')
         s++ ;
   }
}
char seenpats[TOTPATT][25] ;
char canonpat[25] ;
char *hashtab[HASHPRIME] ;
int npats = 0 ;
int botlev = 0 ;
unsigned char suff[SYMMMOVES+1] ;
int nsuff = 0 ;
int invmov(int m) {
   return (m / TWISTS) * TWISTS + TWISTS-1-(m % TWISTS) ;
}
/**
 *   Reduce the search space using symmetry.
 */
void calccanon() {
   int i, j, inv ;
   char canon[25] ;
   for (inv=0; inv<2; inv++) {
      for (i=0; i<48; i++) {
         struct symfast *p = symfast[0] ;
         initM() ;
         if (inv) {
            for (j=nsuff-1; j>=0; j--) {
	       mv(invmov(map[i][suff[j]])) ;
            }
            for (j=inputMoveCount-1; j>=0; j--) {
	       mv(invmov(map[i][inputMoves[j]])) ;
            }
         } else {
            for (j=0; j<inputMoveCount; j++) {
	       mv(map[i][inputMoves[j]]) ;
            }
            for (j=0; j<nsuff; j++) {
	       mv(map[i][suff[j]]) ;
            }
         }
         memcpy(canon+0, p[0].c, 8) ;
         memcpy(canon+8, p[1].c, 4) ;
         memcpy(canon+12, p[3].c, 8) ;
         memcpy(canon+20, p[4].c, 4) ;
         canon[24] = 0 ;
         for (j=0; j<24; j++)
            canon[j] = canon[j] + 'a' ;
//printf("Int %s\n", canon) ;
         if ((i == 0 && inv == 0) || strcmp(canon, canonpat) < 0)
	    strcpy(canonpat, canon) ;
      }
   }
}
int hash(char *s) {
   int h = 37 ;
   while (*s)
      h = h * 37 + *s++ ;
   h = h & 0x7fffffff ;
   return h % HASHPRIME ;
}
void addsymm(int togo, int prev, int sofar, int lastplus) {
   if (togo == 0) {
      int h ;
      calccanon() ;
      h = hash(canonpat) ;
      while (hashtab[h]) {
	 if (strcmp(canonpat, hashtab[h]) == 0) {
            skip[sofar] = 1 ;
	    return ;
	 }
	 h++ ;
	 if (h >= HASHPRIME)
	    h = 0 ;
      }
      strcpy(seenpats[npats], canonpat) ;
      hashtab[h] = seenpats[npats++] ;
      botlev++ ;
   } else {
      int i, f ;
      if (sofar >= 0 && skip[sofar])
         return ; // already truncated this one
      for (f=0; f<6; f++) {
#ifdef HALFTURN
         if (f == prev || f + 3 == prev)
            continue ; // don't allow moving the same face twice
#else
         if (f + 3 == prev || (f == prev && !lastplus))
            continue ; // don't allow moving the same face twice
#endif
         for (i=0; i<TWISTS; i++) {
            int m = f * TWISTS + i ;
            suff[nsuff++] = m ;
#ifdef HALFTURN
	    addsymm(togo-1, f, (sofar + 1) * NMOVES + m, 0) ;
#else
	    addsymm(togo-1, f, (sofar + 1) * NMOVES + m, i==0 && f != prev) ;
#endif
	    nsuff-- ;
#ifndef HALFTURN
            if (f == prev)
               break ;
#endif
	 }
      }
   }
}
void reducesymm() {
   int i ;
   memset(skip, 0, sizeof(skip)) ;
   memset(hashtab, 0, sizeof(hashtab)) ;
   npats = 0 ;
   for (i=0; i<=SYMMMOVES; i++) {
      botlev = 0 ;
      addsymm(i, 100, -1, 0) ;
   }
   if (botlev != MAXPATT)
      printf("Symmetry:  %d/%d\n", botlev, MAXPATT) ;
   initM() ;
   for (i=0; i<inputMoveCount; i++) {
      mv(inputMoves[i]) ;
   }
   parity = inputMoveCount & 1 ;
}
char rline[8192] ;
int dosymmetry = 0 ;
int main(int argc, char *argv[]) {
#ifdef HALFTURN
   printf("This is rubsolv(half) Copyright 2003 Radical Eye Software\n") ;
#else
   printf("This is rubsolv(quarter) Copyright 2003 Radical Eye Software\n") ;
#endif
   init() ;
   while (argc > 1 && argv[1][0] == '-' && argv[1][1]) {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
case 's': dosymmetry = 1 ; break ;
case 'a': doall = 1 ; break ;
default: error("! bad option") ; break ;
      }
   }
   if (argc == 2 && strcmp(argv[1], "-") == 0) {
      while (fgets(rline, 8190, stdin)) {
         while (strlen(rline) > 0 && rline[strlen(rline)-1] <= ' ')
            rline[strlen(rline)-1] = 0 ;
         lasttime() ;
         initM() ;
         parity = 0 ;
	 inputMoveCount = 0 ;
         symmov(rline) ;
         printf("\n") ;
         if (dosymmetry)
	    reducesymm() ;
         if (rubsolv() < inputMoveCount)
            printf("** Improvement **\n") ;
      }
   } else if (argc > 1) {
      int i ;
      lasttime() ;
      initM() ;
      parity = 0 ;
      inputMoveCount = 0 ;
      for (i=1; i<argc; i++)
         symmov(argv[i]) ;
      printf("\n") ;
      if (dosymmetry)
         reducesymm() ;
      if (rubsolv() < inputMoveCount)
         printf("** Improvement **\n") ;
   } else {
      docube() ;
   }
   exit(0) ;
}
