// gcc -Wall -Wextra -pedantic node_mem.c -lnuma -o node_mem
//
#include <assert.h>
#include <numa.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  void **mem;

  assert(argc == 3);
  size_t siz = atol(argv[1]);
  int sec = atoi(argv[2]);

  int numnodes = numa_num_configured_nodes();
  mem = malloc(numnodes * sizeof(void *));

  for (int i = 0; i < numnodes; ++i) {
    assert(0 < numa_node_size(i, NULL));
    mem[i] = numa_alloc_onnode(siz, i);
    memset(mem[i], 42, siz);
  }

  printf("numnodes=%d\n", numnodes);
  assert(0 == sleep(sec));

  return 0;
}
