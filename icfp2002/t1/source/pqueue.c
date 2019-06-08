/* Priority queue */

#include "pqueue.h"
#include <assert.h>
#include <stdio.h>

pqelement *heap[HEAPSIZE];
int heapsize;

#define PARENT(x) ((x)/2)
#define LEFT(x) (2 * (x))
#define RIGHT(x) (2 * (x) + 1)

#define CMP(p, q) ((p)->priority < (q)->priority ? -1 : +1)

void heap_init(void)
{
    heapsize = 0;
}

void heap_insert(pqelement *pq)
{
    int i;
    heapsize++;
    assert(heapsize < HEAPSIZE);
    i = heapsize;
    while (i > 1 && CMP(heap[PARENT(i)], pq) < 0) {
	heap[i] = heap[PARENT(i)];
	i = PARENT(i);
    }		      
    heap[i] = pq;
}

static void heapify(int i)
{
    int l = LEFT(i), r = RIGHT(i);
    int largest;
    pqelement *t;

    if (l <= heapsize && CMP(heap[l], heap[i]) > 0)
	largest = l;
    else
	largest = i;
    if (r <= heapsize && CMP(heap[r], heap[largest]) > 0)
	largest = r;
    if (largest != i) {
	t = heap[i];
	heap[i] = heap[largest];
	heap[largest] = t;
	heapify(largest);
    }
}

pqelement *heap_max(void)
{
    pqelement *top;
    if (heapsize == 0) return 0 ;
    top = heap[1];
    heap[1] = heap[heapsize--];
    heapify(1);
    return top;
}
