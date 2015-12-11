#include <BPatch.h>

BPatch bpatch;

int main(int argc, char *argv[]) {
  if(argc != 2) {
    printf("Please specify exactly one argument\n");
    return 1;
  }
  int pid = strtol(argv[1], NULL, 10);
  if(!pid) {
    printf("Please specify a valid pid\n");
    return 1;
  }
  BPatch_process *app = bpatch.processAttach(NULL, pid);
  if(!app) {
    printf("Failed to attach to pid %d\n", pid);
    return 1;
  } else {
    printf("Successfully attached.\n");
  }
  app->continueExecution();
  // FIXME: exit cleanly
  while (!app->isTerminated()) {
    bpatch.waitForStatusChange();
  }
  return 0;
}
