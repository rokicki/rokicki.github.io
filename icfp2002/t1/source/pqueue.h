/* Priority queue */

#define HEAPSIZE 128536   /* maximum heap size */

typedef struct pqelement pqelement;

struct pqelement {
    double priority;
};

void heap_init(void);
void heap_insert(pqelement *pq);
pqelement *heap_max(void);
