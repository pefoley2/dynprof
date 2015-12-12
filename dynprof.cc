#include <BPatch.h>


std::unique_ptr<string> get_path(char* exe) {
  char* path = getenv("PATH");
  char* dir = strtok(path, ":");
  unique_ptr<string> fullpath(new string);
  while(dir) {
    fullpath->append(dir);
    fullpath->append("/");
    fullpath->append(exe);
    if(access(fullpath->c_str(),F_OK)==0) {
      return fullpath;
    }
    fullpath->clear();
    dir = strtok(nullptr, ":");
  }
  return nullptr;
}

BPatch bpatch;

int main(int argc, char *argv[]) {
  if(argc < 2) {
    printf("Please specify at least one argument\n");
    return 1;
  }
  unique_ptr<string> path = get_path(argv[1]);
  if(!path) {
    printf("%s not found in path\n", argv[1]);
    return 1;
  }
  const char* params = "";
  if(argc>2) {
    params = (const char*)argv[2];
  }
  std::unique_ptr<BPatch_process> app(bpatch.processCreate(path->c_str(), &params));
  if(!app) {
    printf("Failed to start %s\n", path->c_str());
    return 1;
  } else {
    printf("Successfully started.\n");
  }
  //Attach hooks
  app->continueExecution();
  while (!app->isTerminated()) {
    bpatch.waitForStatusChange();
  }
  return 0;
}
