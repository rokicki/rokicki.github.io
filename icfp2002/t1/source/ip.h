/**
 *   Manage interesting points and distances to/from interesting points.
 *
 *   The set of interesting points is dynamic.
 *
 *   Each square has a 16-bit index into the interesting point array.
 *   If that is zero, it's not an interesting point.
 *
 *   There is a max number of interesting points, limited mostly by
 *   the size of the 2D array we use to hold the points.
 *
 *   It also sets the speed because we tend to scan the rows to
 *   find close ip's.
 */
#define MAXIP (1000)
/**
 *   Uninteresting means, not an interesting point (currently).
 *   Note that this means index 0 is unused in the above array.
 */
#define UNINTERESTING (0)
/**
 *   For each interesting point, we have an x/y value, a distance
 *   to which we've already explored for other interesting points,
 *   and an int time meaning when we explored that deep.
 */
typedef struct ip ip ;
struct ip {
   int x, y ;
   int idx ;
   int explordepth ;
   int explorboxes ;
   int whenlastexplored ;
   square *s ;
   char total ; // explored *all* the way
   char free ;  // available for use
} ;
/**
 *   We always explore to at least a minimum depth every time we add
 *   an interesting point.  We actually measure this in terms of the
 *   number of squares *from* this point we've explored.  We set that
 *   to MAXIP.  So every time we add an interesting point, we examine
 *   the closest MAXIP points (at least) to find more points.
 *
 *   As time goes on we increase this number, depending on CPU time
 *   availability.  We double it each time.
 */
#define INITEXPLORBOXES (MAXIP)
/**
 *   What makes a square interesting?   It is an unexplored base
 *   (neither we nor anyone has visited it), a known base (we have
 *   visited it at least once, and it still contains at least one
 *   packet), a dirty unexplored base (we haven't visited it but
 *   some other robot has), a questionable base (some bot dropped
 *   something here that has not been picked up---but it might have
 *   been a delivery), a destination of a packet we hold, a
 *   destination of a known packet, etc.  There are a lot of
 *   possibilities.
 *
 *   For simple maps, the max number of IPs will probably stay small
 *   and this is good, let's us do good planning quickly.
 *   But for large maps, the number of IPs may exceed our limit
 *   above.  Every time we add an IP, we compare it to the existing
 *   IPs and kill of a victim IP if necessary, but only if that IP
 *   has a lower priority.
 *
 *   Priority is a dynamic thing, though, of course.  And tuning it is
 *   difficult.  But we do what we can.
 *
 *   Now, we want to always keep a certain number of bases and a
 *   certain number of destinations.  We never want to kill a base if
 *   that would push the number of bases below this value, and ditto
 *   for destinations.  This parameter tunes that.
 */
#define MINOFTYPE (IP/4)
/**
 *   What functions are available?  Here they are.
 *
 *   What is the priority of this square?  This in many senses reflects
 *   the desireability of us visiting that square and is probably a
 *   complex, maybe cached, function.
 *
 *   The low bits four need to include some key information; the high
 *   bits are for the actual priority info.
 */
int sqpriority(int x, int y) ;
#define PRIORITY_BASE (1)
#define PRIORITY_DEST (2) // of some packet we are carrying or have seen
/**
 *   Add a new interesting point.  This may remove an existing one, or
 *   it may completely fail if the point is less interesting than the
 *   ones we already have.  Returns the IP number.
 */
int add_ip(int x, int y) ;
/**
 *   Remove an interesting point.  Should never be called by you; only
 *   called by the add_ip function.
 */
void rm_ip(int x, int y) ;
/**
 *   Spend some time exploring the ip a little deeper.  In all cases this
 *   time should be well under a second, but it should also not be super
 *   small (under a millisecond).  Returns value != 0 if it did anything.
 */
int plan_ip(void) ;
/**
 *   What is the distance between two points, using the above datastructures?
 *   May return an approximate value.  In general will underestimate but
 *   accuracy will increase over time.  If both points are interesting will
 *   never return a value less than the number of boxes reachable with
 *   INITEXPLORBOXES, which is probably about 20.  Will never return a value
 *   less than the manhattan distance.
 */
int ip_dist(int x1, int y1, int x2, int y2) ;
/**
 *   ip init.
 */
void ip_init() ;
/**
 *   Explore the immediate neighborhood of self and add any 
 *   interesting squares that are not already in the interesting
 *   list.
 */
void visit_neighborhood(int maxboxes) ;
/**
 *   External things we can touch.
 */
int nips ;
ip ips[MAXIP] ;
