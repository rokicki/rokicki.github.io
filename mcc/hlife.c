/*
 * hlife 0.3 by Radical Eye Software.
 *
 * Usage:
 *   hlife -m gencount [-M maxmem] [-p pbmfilename] [-w pbmdimen]
 *                     [-o macrocellfile] lifefile
 *
 * The lifefile can be in either RLE or Alan Hensel's Life format.
 * (I don't guarantee I understand all of the options or that the
 * reader is implemented correctly; it's a quick and dirty hack.)
 * The gencount is specified as an integer <100 where the desired number
 * of generations is actually 2**gencount.  The maxmem is the maximum
 * number of megabytes of memory you want the program to use (approximately).
 * (These megabytes are 1000000 bytes, not 1048576 bytes.)
 * The default is 64M.  If you start seeing [] with numbers in them, then
 * the program is garbage collecting.  The pbmfilename
 * is the name of pbm file to write with the final result, and the pbmdimen
 * is the maximum width or height for that pbm file you are willing to
 * accept (the default is 1024).  The -o writes a textual macrofile for
 * your edification.  Tons more to do, clearly; this is all so
 * crude . . .
 *
 *   All good ideas here were originated by Gosper or Bell or others, I'm
 *   sure, and all bad ones by yours truly.
 *
 *   The main reason I wrote this program was to attempt to push out the
 *   evaluation of metacatacryst as far as I could.  So this program
 *   really does very little other than compute life as far into the
 *   future as possible, using as little memory as possible (and reusing
 *   it if necessary).  No UI, few options.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/*
 *   Into instances of this node structure is where almost all of the
 *   memory allocated by this program goes.  Thus, it is imperative we
 *   keep it as small as possible so we can explore patterns as large
 *   and as deep as possible.
 *
 *   But first, how does this program even work?  Well, there are
 *   two major tricks that are used.
 *
 *   The first trick is to represent the 2D space `symbolically'
 *   (in the sense that a binary decision diagram is a symbolic
 *   representation of a boolean predicate).  This can be thought
 *   of as a sort of compression.  We break up space into a grid of
 *   squares, each containing 8x8 cells.  And we `canonicalize'
 *   each square; that is, all the squares with no cells set are
 *   represented by a single actual instance of an empty square;
 *   all squares with only the upper-left-most cell set are
 *   represented by yet another instance, and so on.  A single pointer
 *   to the single instance of each square takes less space than
 *   representing the actual cell bits themselves.
 *
 *   Where do we store these pointers?  At first, one might envision
 *   a large two-dimensional array of pointers, each one pointing
 *   to one of the square instances.  But instead, we group the
 *   squares (we'll call them 8-squares) into larger squares 16
 *   cells on a side; these are 16-squares.  Each 16-square contains
 *   four 8-squares, so each 16-square is represented by four
 *   pointers, each to an 8-square.  And we canonicalize these as
 *   well, so for a particular set of values for a 16 by 16 array
 *   of cells, we'll only have a single 16-square.
 *
 *   And so on up; we canonicalize 32-squares out of 16-squares, and
 *   on up to some limit.  Now the limit need not be very large;
 *   having just 20 levels of nodes gives us a universe that is
 *   4 * 2**20 or about 4M cells on a side.  Having 100 levels of
 *   nodes (easily within the limits of this program) gives us a
 *   universe that is 4 * 2**100 or about 5E30 cells on a side.
 *   I've run universes that expand well beyond 1E50 on a side with
 *   this program.
 *
 *   [A nice thing about this representation is that there are no
 *   coordinate values anywhere, which means that there are no
 *   limits to the coordinate values or complex multidimensional
 *   arithmetic needed.]
 *
 *   [Note that this structure so far is very similar to the octtrees
 *   used in 3D simulation and rendering programs.  It's different,
 *   however, in that we canonicalize the nodes, and also, of course,
 *   in that it is 2D rather than 3D.]
 *
 *   I mentioned there were two tricks, and that's only the first.
 *   The second trick is to cache the `results' of the LIFE calculation,
 *   but in a way that looks ahead farther in time as you go higher
 *   in the tree, much like the tree nodes themselves scan larger
 *   distances in space.  This trick is just a little subtle, but it
 *   is where the bulk of the power of the program comes from.
 *
 *   Consider once again the 8-squares.  We want to cache the result
 *   of executing LIFE on that area.  We could cache the result of
 *   looking ahead just one generation; that would yield a 6x6 square.
 *   (Note that we cannot calculate an 8-square, because we are
 *   using the single instance of the 8-square to represent all the
 *   different places that 8x8 arrangement occurs, and those different
 *   places might be surrounded by different border cells.  But we
 *   can say for sure that the central 6-square will evolve in a
 *   unique way in the next generation.)
 *
 *   We could also calculate the 4-square that is two generations
 *   hence, and the 3-square that is three generations hence, and
 *   the 2-square that is four generations hence.  We choose the
 *   4-square that is two generations hence; why will be clear in
 *   a moment.
 *
 *   Now let's consider the 16-square.  We would like to look farther
 *   ahead for this square (if we always only looked two generations
 *   ahead, our runtime would be at *least* linear in the number of
 *   generations, and we want to beat that.)  So let's look 4 generations
 *   ahead, and cache the resulting 8-square.  So we do.
 *
 *   Where do we cache the results?  Well, we cache the results in the
 *   same node structure we are using to store the pointers to the
 *   smaller squares themselves.  And since we're hashing them all
 *   together, we want a next pointer for the hash chain.  Put all of
 *   this together, and you get the following structure for the 16-squares
 *   and larger:
 */
struct node {
   struct node *next ;              // hash link
   struct node *nw, *ne, *sw, *se ; // constant; nw != 0 means nonleaf
   struct node *res ;               // cache
} ;
/*
 *   For the 8-squares, we do not have `children', we have actual data
 *   values.  We still break up the 8-square into 4-squares, but the
 *   4-squares only have 16 cells in them, so we represent them directly
 *   by an unsigned short (in this case, the direct value itself takes
 *   less memory than the pointer we might replace it with).
 *
 *   One minor trick about the following structure.  We did lie above
 *   somewhat; sometimes the struct node * points to an actual struct
 *   node, and sometimes it points to a struct leaf.  So we need a way
 *   to tell if the thing we are pointing at is a node or a leaf.  We
 *   could add another bit to the node structure, but this would grow
 *   it, and we want it to stay as small as possible.  Now, notice
 *   that, in all valid struct nodes, all four pointers (nw, ne, sw,
 *   and se) must contain a live non-zero value.  We simply ensure
 *   that the struct leaf contains a zero where the first (nw) pointer
 *   field would be in a struct node.
 *
 *   Each short represents a 4-square in normal, left-to-right then top-down
 *   order from the most significant bit.  So bit 0x8000 is the upper
 *   left (or northwest) bit, and bit 0x1000 is the upper right bit, and
 *   so on.
 */
struct leaf {
   struct node *next ;              // hash link
   struct node *isnode ;            // must always be zero for leaves
   unsigned short nw, ne, sw, se ;  // constant
   unsigned short res ;             // constant
} ;
/*
 *   If it is a struct node, this returns a non-zero value, otherwise it
 *   returns a zero value.
 */
#define is_node(n) (((struct node *)(n))->nw)
/*
 *   A key datastructure is our hash table; we use a simple bucket hash.
 *   We use a fixed-size hash for now, but the size is determined by the
 *   maximum memory we allow, in trying to keep the hash buckets short.
 */
unsigned int hashprime = 2000001 ;
struct node **hashtab ;
/*
 *   Prime hash sizes tend to work best.
 */
int nextprime(int i) {
   int j ;
   i |= 1 ;
   for (;; i+=2) {
      for (j=3; j*j<=i; j+=2)
         if (i % j == 0)
            break ;
      if (j*j > i)
         return i ;
   }
}
/*
 *   Now let's focus on some low-level details and bit manipulation.  We
 *   need a small array to hold the number of bits set in a short.  We
 *   need another small array to hold the 2-square result of a single
 *   life generation on a 4-square.  The 2-square result of that 4-square 
 *   is stored in a special way to make it easier to combine four of
 *   these into a new 4-square.  Specifically, the nw bit is stored at
 *   0x20, the ne bit at 0x10, the sw bit at 0x2, and the se bit at 0x1.
 *
 *   We initialize both of these arrays in the init() subroutine (way)
 *   below.
 *
 *   Note that all the places we represent 4-squares by short, we use
 *   unsigned shorts; this is so we can directly index into these arrays.
 */
unsigned char shortpop[65536] ;
unsigned char liferules[65536] ; // returns four bits in the format xx..yy
/*
 *   The cached result of an 8-square is a new 4-square representing
 *   two generations into the future.  This subroutine calculates that
 *   future, assuming liferules above is calculated.  The code that it
 *   uses is similar to code you'll see again, so we explain what's
 *   going on in some detail.
 *
 *   Each time we build a leaf node, we compute the result, because it
 *   is reasonably quick.
 *
 *   The first generation result of an 8-square is a 6-square, which
 *   we represent as nine 2-squares.  The nine 2-squares are called
 *   t00 through t22, and are arranged in a matrix:
 *
 *      t00   t01   t02
 *      t10   t11   t12
 *      t20   t21   t22
 *
 *   To compute each of these, we need to extract the relevant bits
 *   from the four 4-square values n->nw, n->ne, n->sw, and n->ne.
 *   We can use these values to directly index into the liferules
 *   array.
 *
 *   Then, given the nine values, we can compute a resulting 4-square
 *   by computing four 2-square results, and combining these into a
 *   single 4-square.
 *
 *   It's a bit intricate, but it's not really overwhelming.
 */
unsigned short leafres(struct leaf *n) {
   unsigned short
   t00 = liferules[n->nw],
   t01 = liferules[((n->nw << 2) & 0xcccc) | ((n->ne >> 2) & 0x3333)],
   t02 = liferules[n->ne],
   t10 = liferules[((n->nw << 8) & 0xff00) | ((n->sw >> 8) & 0x00ff)],
   t11 = liferules[((n->nw << 10) & 0xcc00) | ((n->ne << 6) & 0x3300) |
                   ((n->sw >> 6) & 0x00cc) | ((n->se >> 10) & 0x0033)],
   t12 = liferules[((n->ne << 8) & 0xff00) | ((n->se >> 8) & 0x00ff)],
   t20 = liferules[n->sw],
   t21 = liferules[((n->sw << 2) & 0xcccc) | ((n->se >> 2) & 0x3333)],
   t22 = liferules[n->se] ;
   return
   (liferules[(t00 << 10) | (t01 << 8) | (t10 << 2) | t11] << 10) |
   (liferules[(t01 << 10) | (t02 << 8) | (t11 << 2) | t12] << 8) |
   (liferules[(t10 << 10) | (t11 << 8) | (t20 << 2) | t21] << 2) |
    liferules[(t11 << 10) | (t12 << 8) | (t21 << 2) | t22] ;
}
/*
 *   Let's worry about allocation of nodes later; here's the declarations
 *   we need.  Each of these allocators actually *copy* an existing node
 *   in all of its fields.
 */
struct node *newnode() ;
struct leaf *newleaf() ;
/*
 *   These next two routines are (nearly) our only hash table access
 *   routines; we simply look up the passed in information.  If we
 *   find it in the hash table, we return it; otherwise, we build a
 *   new node and store it in the hash table, and return that.
 */
struct node *find_node(struct node *nw, struct node *ne,
                       struct node *sw, struct node *se) {
   struct node *p ;
   unsigned int h = 3 + (int)nw ;
   h = h * 3 + (int)ne ;
   h = h * 3 + (int)sw ;
   h = h * 3 + (int)se ;
   h = h % hashprime ;
   for (p=hashtab[h]; p; p = p->next) // make sure to compare nw *first*
      if (nw == p->nw && ne == p->ne && sw == p->sw && se == p->se)
         return p ;
   p = newnode() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   p->res = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = p ;
   return p ;
}
struct leaf *find_leaf(unsigned short nw, unsigned short ne,
                       unsigned short sw, unsigned short se) {
   struct leaf *p ;
   unsigned int h = 9 + nw ;
   h = h * 9 + ne ;
   h = h * 9 + sw ;
   h = h * 9 + se ;
   h = h % hashprime ;
   for (p=(struct leaf *)hashtab[h]; p; p = (struct leaf *)p->next)
      if (!is_node(p) &&
          nw == p->nw && ne == p->ne && sw == p->sw && se == p->se)
         return p ;
   p = newleaf() ;
   p->nw = nw ;
   p->ne = ne ;
   p->sw = sw ;
   p->se = se ;
   p->res = leafres(p) ;
   p->isnode = 0 ;
   p->next = hashtab[h] ;
   hashtab[h] = (struct node *)p ;
   return p ;
}
/*
 *   We do now support garbage collection, but there are some routines we
 *   call frequently to help us.
 */
struct node *save() ;
void clearstack(), pop() ;
int gsp ;
/*
 *   We've shown how to calculate the result for an 8-square.  What about
 *   the bigger squares?  Well, let's assume we have the following
 *   routines that do that work the hard way.
 */
struct leaf *dorecurs_leaf() ;
struct node *dorecurs() ;
/*
 *   The following routine does the same, but first it checks to see if
 *   the cached result is any good.  If it is, it directly returns that.
 *   Otherwise, it figures out whether to call the leaf routine or the
 *   non-leaf routine by whether two nodes down is a leaf node or not.
 *   (We'll understand why this is a bit later.)  All the sp stuff is
 *   stack pointer and garbage collection stuff.
 */
struct node *getres(struct node *n) {
   if (n->res == 0) {
      int sp = gsp ;
      save(n) ;            // protect this node from garbage collection
      if (is_node(n->nw))
         n->res = dorecurs(n->nw, n->ne, n->sw, n->se) ;
      else
         n->res = (struct node *)dorecurs_leaf(n->nw, n->ne, n->sw, n->se) ;
      pop(sp) ;
   }
   return save(n->res) ;
}
/*
 *   So let's say the cached way failed.  How do we do it the slow way?
 *   Recursively, of course.  For an n-square (composed of the four
 *   n/2-squares passed in, compute the n/2-square that is n/4
 *   generations ahead.
 *
 *   This routine works exactly the same as the leafres() routine, only
 *   instead of working on an 8-square, we're working on an n-square,
 *   returning an n/2-square, and we build that n/2-square by first building
 *   9 n/4-squares, use those to calculate 4 more n/4-squares, and
 *   then put these together into a new n/2-square.  Simple, eh?
 */
struct node *dorecurs(struct node *n, struct node *ne, struct node *t,
                      struct node *e) {
   int sp = gsp ;
   struct node
   *t00 = getres(n),
   *t01 = getres(find_node(n->ne, ne->nw, n->se, ne->sw)),
   *t02 = getres(ne),
   *t10 = getres(find_node(n->sw, n->se, t->nw, t->ne)),
   *t11 = getres(find_node(n->se, ne->sw, t->ne, e->nw)),
   *t12 = getres(find_node(ne->sw, ne->se, e->nw, e->ne)),
   *t20 = getres(t),
   *t21 = getres(find_node(t->ne, e->nw, t->se, e->sw)),
   *t22 = getres(e) ;
   n = find_node(getres(find_node(t00, t01, t10, t11)),
                 getres(find_node(t01, t02, t11, t12)),
                 getres(find_node(t10, t11, t20, t21)),
                 getres(find_node(t11, t12, t21, t22))) ;
   pop(sp) ;
   return n ;
}
/*
 *   This next is somewhat of a hack; we only do n/8 generations ahead
 *   rather than n/4.  Currently we only use this on the topmost node,
 *   said node only allocated so the total result fits within our
 *   universe.   (This is all about boundary conditions.)
 */
struct node *dorecurs1(struct node *n, struct node *ne, struct node *t,
                       struct node *e) {
   int sp = gsp ;
   struct node
   *t10 = getres(find_node(n->sw, n->se, t->nw, t->ne)),
   *t11 = getres(find_node(n->se, ne->sw, t->ne, e->nw)),
   *t20 = getres(t),
   *t21 = getres(find_node(t->ne, e->nw, t->se, e->sw)) ;
   n = find_node(t10, t11, t20, t21) ;
   pop(sp) ;
   return n ;
}
/*
 *   If the node is a 16-node, then the constituents are leaves, so we
 *   need a very similar but still somewhat different subroutine.  Since
 *   we do not (yet) garbage collect leaves, we don't need all that
 *   save/pop mumbo-jumbo.
 */
struct leaf *dorecurs_leaf(struct leaf *n, struct leaf *ne, struct leaf *t,
                           struct leaf *e) {
   unsigned short
   t00 = n->res,
   t01 = find_leaf(n->ne, ne->nw, n->se, ne->sw)->res,
   t02 = ne->res,
   t10 = find_leaf(n->sw, n->se, t->nw, t->ne)->res,
   t11 = find_leaf(n->se, ne->sw, t->ne, e->nw)->res,
   t12 = find_leaf(ne->sw, ne->se, e->nw, e->ne)->res,
   t20 = t->res,
   t21 = find_leaf(t->ne, e->nw, t->se, e->sw)->res,
   t22 = e->res ;
   return find_leaf(find_leaf(t00, t01, t10, t11)->res,
                    find_leaf(t01, t02, t11, t12)->res,
                    find_leaf(t10, t11, t20, t21)->res,
                    find_leaf(t11, t12, t21, t22)->res) ;
}
/*
 *   And that is nearly it!  Now we finish up some details.  First,
 *   allocation of nodes and leaves, but in a reasonably efficient way.
 *   For garbage collection, we need this forward declaration.
 */
void do_gc() ;
unsigned int alloced, maxmem = 64000000 ;
/*
 *   We keep free nodes in a linked list for allocation, and we allocate
 *   them 1000 at a time.
 */
struct node *freenodes ;
int okaytogc = 0 ;           // only true when we're running generations.
struct node *newnode() {
   struct node *r ;
   if (freenodes == 0) {
      int i ;
      freenodes = calloc(1000, sizeof(struct node)) ;
      alloced += 1000 * sizeof(struct node) ;
      for (i=0; i<999; i++) {
         freenodes[1].next = freenodes ;
         freenodes++ ;
      }
      fprintf(stderr, "N") ;
   }
   r = freenodes ;
   freenodes = freenodes->next ;
   if (freenodes == 0 && alloced + 1000 * sizeof(struct node) > maxmem &&
       okaytogc)
      do_gc() ;
   return r ;
}
/*
 *   Leaves are similar but different.
 */
struct leaf *freeleafs ;
int nfreeleafs ;
struct leaf *newleaf() {
   struct leaf *r ;
   if (nfreeleafs == 0) {
      freeleafs = calloc(1000, sizeof(struct leaf)) ;
      alloced += 1000 * sizeof(struct leaf) ;
      nfreeleafs = 1000 ;
      fprintf(stderr, "L") ;
   }
   r = freeleafs++ ;
   nfreeleafs-- ;
   return r ;
}
/*
 *   Sometimes we want the new node or leaf to be automatically cleared
 *   for us.
 */
struct node *newclearednode() {
   return memset(newnode(), 0, sizeof(struct node)) ;
}
struct leaf *newclearedleaf() {
   return memset(newleaf(), 0, sizeof(struct leaf)) ;
}
/*
 *   These are the rules we run.  Note that these rules are
 *   *total* cells (including the middle) for birth and health.
 *   To modify the rules, either modify these arrays, or add an
 *   option to initialize them from an option.
 */
char birth[10] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 } ;
char health[10] = { 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 } ;
/*
 *   Some globals representing our universe.  The root is the
 *   real root of the universe, and the depth is the depth of the
 *   tree where 2 means that root is a leaf, and 3 means that the
 *   children of root are leaves, and so on.  Minc and maxc are
 *   the bounds on both the x and y coordinates of the universe
 *   at startup (note that at startup there is a 2^32 range, due
 *   to the pattern readers needing coordinates, but when running
 *   the universe can get *much* larger.)  The zeronodea is an
 *   array of canonical `empty-space' nodes at various depths.
 *   The ngens is an input parameter which is the second power of
 *   the number of generations to run.
 */
struct node *root ;
int depth ;
int minc, maxc ;
struct node **zeronodea ;
int ngens = 20 ; // must be a power of two
/*
 *   Initialization is a little complex.
 */
void init() {
   int i ;
/*
 *   The population of one-bits in an integer is one more than the
 *   population of one-bits in the integer with one fewer bit set,
 *   and we can turn off a bit by anding an integer with the next
 *   lower integer.
 */
   for (i=1; i<65536; i++)
      shortpop[i] = shortpop[i & (i - 1)] + 1 ;
/*
 *   Here we calculate 1-square results for 9-square inputs using
 *   our population array, and our health/birth arrays.
 */
   for (i=0; i<0x1000; i = ((i | 0x888) + 1) & 0x1777)
      liferules[i] = (i & 0x20 ? health : birth)[shortpop[i]] ;
/*
 *   We expand the 9-square to 1-square array to a 16-square to
 *   4-square array by combining results.
 */
   for (i=0; i<65536; i++)
      liferules[i] = (1 & liferules[i & 0x777]) +
                     ((1 & liferules[(i >> 1) & 0x777]) << 1) +
                     ((1 & liferules[(i >> 4) & 0x777]) << 4) +
                     ((1 & liferules[(i >> 5) & 0x777]) << 5) ;
/*
 *   Here we set hashprime to be 1/40th of our total memory.  This
 *   assumes almost all memory will be held by nodes, and nodes are
 *   24-bytes, and pointers are 4-bytes, and thus 10% of our memory
 *   is our hash table.
 */
   hashprime = maxmem / 40 ;
   hashprime = nextprime(hashprime) ;
   hashtab = calloc(hashprime, sizeof(struct node *)) ;
   alloced += hashprime * sizeof(struct node *) ;
/*
 *   We initialize our universe to be an 8-square.
 */
   minc = 0 ;
   maxc = 7 ;
   root = (struct node *)newclearedleaf() ;
   depth = 2 ;
   zeronodea = (struct node **)calloc(ngens + 5, sizeof(struct node *)) ;
   alloced += (ngens + 5) * sizeof(struct node *) ;
}
/*
 *   This routine expands our universe by a factor of two, adding space
 *   either at lower coordinates than current (if lower is true) or at
 *   higher coordinates than current (if lower is false).
 */
void pushroot(int lower) {
   struct node *newroot = newclearednode() ;
   if (lower) {
      newroot->ne = root ;
      minc -= (2 << depth) ;
   } else {
      newroot->sw = root ;
      maxc += (2 << depth) ;
   }
   root = newroot ;
   depth++ ;
}
/*
 *   Here is our recursive routine to set a bit in our universe.  We
 *   pass in a depth, and walk the space.  Again, a lot of bit twiddling,
 *   but really not all that complicated.  We allocate new nodes and
 *   leaves on our way down.
 *
 *   Note that at this point our universe lives outside the hash table
 *   and has not been canonicalized, and that many of the pointers in
 *   the nodes can be null.  We'll path this up in due course.
 */
void setbit(struct node *n, int x, int y, int depth) {
   if (depth == 2) {
      struct leaf *l = (struct leaf *)n ;
      if (x < 4)
         if (y < 4)
            l->sw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         else
            l->nw |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
      else
         if (y < 4)
            l->se |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
         else
            l->ne |= 1 << (3 - (x & 3) + 4 * (y & 3)) ;
   } else {
      int w = 1 << depth ;
      struct node **nptr ;
      depth-- ;
      if (x < w)
         if (y < w)
            nptr = &(n->sw) ;
         else
            nptr = &(n->nw) ;
      else
         if (y < w)
            nptr = &(n->se) ;
         else
            nptr = &(n->ne) ;
      if (*nptr == 0)
         if (depth == 2)
            *nptr = (struct node *)newclearedleaf() ;
         else
            *nptr = newclearednode() ;
      setbit(*nptr, x & (w-1), y & (w-1), depth) ;
   }
}
/*
 *   Our nonrecurse top-level bit setting routine simply expands the
 *   universe as necessary to emcompass the passed-in coordinates, and
 *   then invokes the recursive setbit.
 */
void set(int x, int y) {
   while (x < minc || y < minc)
      pushroot(1) ;
   while (x > maxc || y > maxc)
      pushroot(0) ;
   setbit(root, x-minc, y-minc, depth) ;
}
/*
 *   This ugly bit of code will go undocumented.  It reads patterns in either
 *   RLE format or in Alan Hensel's life format.
 */
void readpattern() {
   int c ;
   int n = 0 ;
   int x=0, y=0 ;
   if ((c = getchar()) == '#') {
      int leftx = x ;
      char line[251] ;
      ungetc(c, stdin) ;
      while ((c=getchar()) != EOF) {
         if (c == '#') {
            fgets(line, 250, stdin) ;
            if (line[0] == 'P') {
               sscanf(line + 1, " %d %d", &x, &y) ;
               leftx = x ;
            }
         } else if (c == '*')
            set(x++, y) ;
         else if (c == '\n') {
            x = leftx ;
            y++ ;
         } else
            x++ ;
      }
   }
   while ((c = getchar()) != EOF && c != '\n') ;
   while ((c = getchar()) != EOF) {
      if ('0' <= c && c <= '9') {
         n = n * 10 + c - '0' ;
      } else if (c == '$') {
         if (n == 0)
            n = 1 ;
         y -= n ;
         x = 0 ;
         n = 0 ;
      } else if (c == 'b') {
         if (n == 0)
            n = 1 ;
         x += n ;
         n = 0 ;
      } else if (c == 'o') {
         if (n == 0)
            n = 1 ;
         while (n-- > 0)
            set(x++, y) ;
         n = 0 ;
      } else if (c == '!') {
         break ;
      } else if (c == '\n' || c == '\r') {
         // ignore
      } else {
         fprintf(stderr, "Saw char %d\n", c) ;
      }
   }
}
/*
 *   This routine returns the canonical clear space node at a particular
 *   depth.
 */
struct node *zeronode(int depth) {
   if (zeronodea[depth] == 0) {
      if (depth == 2) {
         zeronodea[depth] = (struct node *)find_leaf(0, 0, 0, 0) ;
      } else {
         struct node *z = zeronode(depth-1) ;
         zeronodea[depth] = find_node(z, z, z, z) ;
      }
   }
   return zeronodea[depth] ;
}
/*
 *   Canonicalize a universe by filling in the null pointers and then
 *   invoking find_node on each node.  Drops the original universe on
 *   the floor [big deal, it's probably small anyway].
 */
struct node *hashpattern(struct node *root, int depth) {
   if (root == 0) {
      return zeronode(depth) ;
   } else if (depth == 2) {
      struct leaf *n = (struct leaf *)root ;
      return (struct node *)find_leaf(n->nw, n->ne, n->sw, n->se) ;
   } else {
      depth-- ;
      return find_node(hashpattern(root->nw, depth),
                       hashpattern(root->ne, depth),
                       hashpattern(root->sw, depth),
                       hashpattern(root->se, depth)) ;
   }
}
/*
 *   Finally, we get to run the pattern.  We first ensure that all
 *   clearspace nodes and the input pattern is never garbage
 *   collected; we turn on garbage collection, and then we invoke our
 *   magic top-level routine passing in clearspace borders on the top
 *   and right.
 */
struct node *runpattern(struct node *n) {
   struct node *z = zeronode(depth) ;
   save(z) ;
   save(n) ;
   okaytogc = 1 ;
   return dorecurs1(z, z, n, z) ;
   okaytogc = 0 ;
   clearstack() ;
}
/*
 *   At this point, we're pretty much done.  The remaining routines just
 *   implement various utility functions and the like, but feel free to
 *   skip down to main if you like.
 *
 *   The next few routines are a very simple multiprecision arithmetic
 *   setup using arrays of integers.
 */
int base[200] ; // for first 100 numbers 0..99
/*
 *   Give me a new small multiprecision number for 0..99.
 */
int *newsmallint(int n) {
   int *r = base + 2 * n ;
   r[0] = n ;
   r[1] = -1 ;
   return r ;
}
/*
 *   Given an accumulator which is big enough, add s.
 */
int addto(int *acc, int *s) {
   int r = 0 ;
   int m = 1 ;
   while (*acc != -1 || *s != -1 || r > 0) {
      if (*acc == -1) {
         *acc = 0 ;
         acc[1] = -1 ;
      }
      if (*s == -1)
         *acc += r ;
      else
         *acc += *s++ + r ;
      if (*acc >= 1000000000) {
         r = 1 ;
         *acc -= 1000000000 ;
      } else
         r = 0 ;
      acc++ ;
      m++ ;
   }
   return m ;
}
/*
 *   Return the sum of four numbers.  Currently mallocs; a better allocation
 *   strategy would be good if this ever becomes a performance limiter.
 */
int *sum4(int *a1, int *a2, int *a3, int *a4) {
   int t[100] ;
   int m = 0 ;
   int i ;
   int *r ;
   for (i=0; a1[i] != -1; i++)
      t[i] = a1[i] ;
   t[i] = -1 ;
   addto(t, a2) ;
   addto(t, a3) ;
   m = addto(t, a4) ;
   if (m == 2 && t[0] < 100)
      return newsmallint(t[0]) ;
   else {
      r = calloc(sizeof(int), m) ;
      alloced += sizeof(int) * m ;
      memcpy(r, t, m * sizeof(int)) ;
      return r ;
   }
}
/*
 *   Stringify one of these multiprecision numbers.
 */
char *stringify(int *n) {
   static char stringified[2000] ;
   int i ;
   char *r = stringified ;
   for (i=0; n[i+1] != -1; i++) ;
   sprintf(r, "%d", n[i]) ;
   while (--i >= 0) {
      r += strlen(r) ;
      sprintf(r, "%09d", n[i]) ;
   }
   return stringified ;
}
/*
 *   A lot of the routines from here on down traverse the universe, hanging
 *   information off the nodes.  The way they generally do so is by using
 *   (or abusing) the cache (res) field, and the least significant bit of
 *   the hash next field (as a visited bit).
 */
#define marked(n) (1 & (int)(n)->next)
#define mark(n) ((n)->next = (struct node *)(1 | (int)(n)->next))
#define clearmark(n) ((n)->next = (struct node *)(~1 & (int)(n)->next))
#define clearmarkbit(p) ((struct node *)(~1 & (int)(p)))
/*
 *   This recursive routine calculates the population by hanging the
 *   population on marked nodes.
 */
int *calcpop(struct node *root, int depth) {
   int *r ;
   if (root == zeronode(depth)) {
      return newsmallint(0) ;
   } else if (depth == 2) {
      struct leaf *n = (struct leaf *)root ;
      return newsmallint(shortpop[n->nw] + shortpop[n->ne] +
                         shortpop[n->sw] + shortpop[n->se]) ;
   } else if (marked(root)) {
      return (int *)root->res ;
   } else {
      depth-- ;
      r = sum4(calcpop(root->nw, depth), calcpop(root->ne, depth),
               calcpop(root->sw, depth), calcpop(root->se, depth)) ;
      mark(root) ;
      root->res = (struct node *)r ;
      return r ;
   }
}
/*
 *   After running one of the marking routines, we can clean up by
 *   calling this routine.
 */
void aftercalcpop(struct node *root, int depth) {
   if (depth > 2 && marked(root)) {
      root->res = 0 ;  // clear cache field!
      clearmark(root) ;
      depth-- ;
      aftercalcpop(root->nw, depth) ;
      aftercalcpop(root->ne, depth) ;
      aftercalcpop(root->sw, depth) ;
      aftercalcpop(root->se, depth) ;
   }
}
/*
 *   This top level routine calculates the population of a universe.
 */
char *population(struct node *root, int depth) {
   int *r = calcpop(root, depth) ;
   aftercalcpop(root, depth) ;
   return stringify(r) ;
}
/*
 *   The next few *very* *kludgey* routines write out a PBM format
 *   scaled view of the universe.  The scaling is tiny (1/8 or less).
 */
int maxpbmdimen = 1024 ;
int minx, miny, maxx, maxy, tminx, tminy, tmaxx, tmaxy ;
char *pbmfilename ;
double gifscale ;
FILE *pbmfile ;
void genrow(struct node *root, int y, int d, int depth,
            int basex, int basey, int w) {
   if (basex > tmaxx || basex + w <= tminx)
      return ;
   if (root == zeronode(depth)) {
      if (basex + w >= tmaxx)
         w = tmaxx - basex + 1 ;
      if (basex < tminx)
         w -= tminx - basex ;
      while (w-- > 0)
         putc('0', pbmfile) ;
   } else if (d == depth) {
      putc('1', pbmfile) ;
      if (y != basey || w != 1)
         fprintf(stderr, "Problem: y %d basey %d i %d\n", y, basey, w) ;
      if (basex < minx)
         minx = basex ;
      if (basex > maxx)
         maxx = basex ;
      if (basey < miny)
         miny = basey ;
      if (basey > maxy)
         maxy = basey ;
   } else {
      w /= 2 ;
      depth-- ;
      if (y >= basey + w) {
         genrow(root->nw, y, d, depth, basex, basey+w, w) ;
         genrow(root->ne, y, d, depth, basex+w, basey+w, w) ;
      } else {
         genrow(root->sw, y, d, depth, basex, basey, w) ;
         genrow(root->se, y, d, depth, basex+w, basey, w) ;
      }
   }
}
int writepbm(int d) {
   int possibdimen, r, i ;
   if (d < 2)
      return 1000000000 ;
   gifscale = 2.0 ;
   i = d ;
   while (i-- > 0)
      gifscale *= 2.0 ;
   possibdimen = 1 << (depth - d) ;
   if (tmaxx == 0)
      tmaxx = tmaxy = possibdimen - 1 ;
   pbmfile = fopen(pbmfilename, "w") ;
   if (pbmfile == 0) {
      fprintf(stderr, "Couldn't open %s for writing\n", pbmfilename) ;
      exit(10) ;
   }
   minx = miny = possibdimen - 1 ;
   maxx = maxy = 0 ;
   fprintf(pbmfile, "P1\n%d %d\n", tmaxx-tminx+1, tmaxy-tminy+1) ;
   r = tmaxx - tminx + 1 ;
   if (r < tmaxy - tminy + 1)
      r = tmaxy - tminy + 1 ;
   for (i=tmaxy; i>=tminy; i--) {
      genrow(root, i, d, depth, 0, 0, possibdimen) ;
      putc('\n', pbmfile) ;
   }
   fclose(pbmfile) ;
   tminx = 2 * minx ;
   tminy = 2 * miny ;
   tmaxx = 2 * maxx + 1 ;
   tmaxy = 2 * maxy + 1 ;
   return r ;
}
/*
 *   This routine recursively writes out the cell structure of the universe.
 */
int cellcounter = 0 ;
FILE *macro ;
void writecell(struct node *root, int depth) {
   int thiscell = 0 ;
   if (marked(root))
      return ;
   mark(root) ;
   if (depth == 2) {
      int i, j ;
      struct leaf *n = (struct leaf *)root ;
      thiscell = ++cellcounter ;
      root->res = (struct node *)thiscell ;
      fprintf(macro, "%d: %d\n", thiscell, depth+1) ;
      for (j=7; j>=0; j--) {
         for (i=0; i<8; i++)
            if ((i < 4 ? j < 4 ? n->sw : n->nw : j < 4 ? n->se : n->ne) &
                (1 << (3 - (i & 3) + 4 * (j & 3))))
               fprintf(macro, "*") ;
            else
               fprintf(macro, ".") ;
         fprintf(macro, "\n") ;
      }
   } else {
      writecell(root->nw, depth-1) ;
      writecell(root->ne, depth-1) ;
      writecell(root->sw, depth-1) ;
      writecell(root->se, depth-1) ;
      thiscell = ++cellcounter ;
      root->res = (struct node *)thiscell ;
      fprintf(macro, "%d: %d %d %d %d %d\n", thiscell, depth+1,
                      (int)root->nw->res, (int)root->ne->res,
                      (int)root->sw->res, (int)root->se->res) ;
   }
}
/*
 *   And this is the top-level writer of the cell structure.
 */
void writeout(char *filename) {
   macro = fopen(filename, "w") ;
   if (!macro) {
      fprintf(stderr, "Couldn't open output file %s\n", filename) ;
      exit(10) ;
   }
   cellcounter = 0 ;
   writecell(root, depth) ;
   fclose(macro) ;
   aftercalcpop(root, depth) ;
}
/*
 *   Finally, our gc routine.  We keep a `stack' of all the `roots'
 *   we want to preserve.  Nodes not reachable from here, we allow to
 *   be freed.
 */
struct node **stack ;
int stacksize ;
/*
 *   This routine marks a node as needed to be saved.
 */
struct node *save(struct node *n) {
   if (gsp >= stacksize) {
      int nstacksize = stacksize * 2 + 100 ;
      alloced += sizeof(struct node *)*(nstacksize-stacksize) ;
      stack = realloc(stack, nstacksize * sizeof(struct node *)) ;
      stacksize = nstacksize ;
   }
   stack[gsp++] = n ;
   return n ;
}
/*
 *   This routine pops the stack back to a previous depth.
 */
void pop(int n) {
   gsp = n ;
}
/*
 *   This routine clears the stack altogether.
 */
void clearstack() {
   gsp = 0 ;
}
/*
 *   Do a gc.  Walk down from all nodes reachable on the stack, saveing
 *   them by setting the odd bit on the next link.  Then, walk the hash,
 *   eliminating the res from everything that's not saveed, and moving
 *   the nodes from the hash to the freelist as appropriate.  Finally,
 *   walk the hash again, clearing the low order bits in the next pointers.
 */
void gc_mark(struct node *root) {
   if (is_node(root) && !marked(root)) {
      mark(root) ;
      gc_mark(root->nw) ;
      gc_mark(root->ne) ;
      gc_mark(root->sw) ;
      gc_mark(root->se) ;
      if (root->res)
         gc_mark(root->res) ;
   }
}
void do_gc() {
   int i, freed=0 ;
   struct node *p ;
   fprintf(stderr, "[") ;
   for (i=0; i<gsp; i++)
      gc_mark(stack[i]) ;
   for (i=0; i<hashprime; i++) {
      p = hashtab[i] ;
      hashtab[i] = 0 ;
      while (p) {
         struct node *n = clearmarkbit(p->next) ;
         if (is_node(p) && p->res != 0 && is_node(p->res) != 0 &&
             !marked(p->res))
            p->res = 0 ;
         if (!is_node(p) || marked(p)) {
            p->next = hashtab[i] ;
            mark(p) ;
            hashtab[i] = p ;
         } else {
            p->next = freenodes ;
            freenodes = p ;
            freed++ ;
         }
         p = n ;
      }
   }
   fprintf(stderr, "%d", freed * sizeof(struct node)) ;
   for (i=0; i<hashprime; i++) {
      p = hashtab[i] ;
      while (p) {
         clearmark(p) ;
         p = p->next ;
      }
   }
   fprintf(stderr, "]") ;
}
/*
 *   Finally, our main routine!
 */
int main(int argc, char *argv[]) {
   char *outfile = 0 ;
   int i ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      switch (argv[0][1]) {
/*
 *   pbmfilename option.
 */
case 'p':
         pbmfilename = argv[1] ;
         argv++ ;
         argc-- ;
         break ;
/*
 *   Currently -M only indicates the max mem you *want* it to use, and
 *   we calculate the size of our hash table based on that.
 */
case 'M':
         if (sscanf(argv[1], "%u", &maxmem) != 1) {
            fprintf(stderr, "Need valid integer for -M (maxmem); saw %s\n",
                            argv[1]) ;
            exit(10) ;
         }
         maxmem *= 1000000 ;
         argv++ ;
         argc-- ;
         break ;
/*
 *   This required argument is the log base 2 of the number of gens to
 *   compute.
 */
case 'm':
         if (sscanf(argv[1], "%d", &ngens) != 1) {
            fprintf(stderr, "Need valid integer for -m (ngens); saw %s\n",
                            argv[1]) ;
            exit(10) ;
         }
         argv++ ;
         argc-- ;
         break ;
/*
 *   This is the max height or width of a picture to generate.
 */
case 'w':
         if (sscanf(argv[1], "%d", &maxpbmdimen) != 1) {
            fprintf(stderr, "Need valid integer for -w (maxpbmdimen); saw %s\n",
                            argv[1]) ;
            exit(10) ;
         }
         argv++ ;
         argc-- ;
         break ;
/*
 *   This is the name of the output file to write a macrocell to.
 */
case 'o':
         outfile = argv[1] ;
         argv++ ;
         argc-- ;
         break ;
default:
         fprintf(stderr, "Didn't understand arg %s\n", argv[0]) ;
         exit(10) ;
         break ;
      }
   }
   if (argc > 1 && freopen(argv[1], "r", stdin) == NULL) {
      fprintf(stderr, "Couldn't open %s for input\n", argv[1]) ;
      exit(10) ;
   }
/*
 *   Now we initialize things, read the pattern, clear out some space at
 *   the bottom and left (for the universe to expand; run pattern takes
 *   care of the top and right).  We then make the universe big enough for
 *   the number of generations we run (this also implicitly sets the
 *   number of generations to run because currently we basically just
 *   advance the top node), and then we run the pattern.  Finally, we
 *   calculate and print the population, and fulfill any other requests
 *   that might have been made of us.
 */
   init() ;
   readpattern() ;
   pushroot(1) ;
   while (ngens > depth-1)
      pushroot(1) ;
   if (ngens != depth-1) {
      fprintf(stderr, "Oops; ngens %d %d\n", ngens, depth-1) ;
      exit(10) ;
   }
   root = hashpattern(root, depth) ;
   printf("The population at start is %s\n", population(root, depth)) ;
   root = runpattern(root) ;
   fprintf(stderr, "Done!\n") ;
   printf("The population is %s\n", population(root, depth)) ;
   printf("Alloced %u total memory\n", alloced) ;
   if (pbmfilename) {
      for (i=depth-5; writepbm(i) * 2 <= maxpbmdimen; i--) ;
      printf("Gif (scale 1/%g) written to %s\n", gifscale, pbmfilename) ;
   }
   if (outfile)
      writeout(outfile) ;
   return 0 ;
}
