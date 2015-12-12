#include <BPatch.h>

// FIXME: check for more errors

unique_ptr<string> get_path(char* exe) {
    char* path = getenv("PATH");
    char* dir = strtok(path, ":");
    unique_ptr<string> fullpath(new string);
    while (dir) {
        fullpath->assign(dir);
        fullpath->append("/");
        fullpath->append(exe);
        if (access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
        dir = strtok(nullptr, ":");
    }
    char* resolved_path = realpath(exe, nullptr);
    if(resolved_path) {
        fullpath->assign(resolved_path);
        if(access(fullpath->c_str(), F_OK) == 0) {
            return fullpath;
        }
    }
    return nullptr;
}

BPatch bpatch;

const char** get_params(int argc, char* argv[]) {
    int nargs = argc > 2 ? argc - 1 : 2;
    char** params = (char**)malloc(nargs * sizeof(char*));
    if (argc > 2) {
        for (int i = 1; i < nargs; i++) {
            int arglen = strlen(argv[i + 1]);
            params[i] = (char*)malloc(arglen + 1);
            memset(params[i], 0, arglen + 1);
            strncpy(params[i], argv[i + 1], arglen + 1);
        }
        params[nargs] = NULL;
    } else {
        params[nargs - 1] = NULL;
    }
    return (const char**)params;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: ./dyninst [program] arg1,arg2,arg3...\n");
        return 1;
    }
    unique_ptr<string> path = get_path(argv[1]);
    if (!path) {
        printf("%s not found in path\n", argv[1]);
        return 1;
    }
    const char** params = get_params(argc, argv);
    params[0] = path->c_str();
    unique_ptr<BPatch_process> app(bpatch.processCreate(path->c_str(), params));
    if (!app) {
        printf("Failed to start %s\n", path->c_str());
        return 1;
    } else {
        printf("Successfully started.\n");
    }
    // TODO: Attach hooks
    app->continueExecution();
    while (!app->isTerminated()) {
        bpatch.waitForStatusChange();
    }
    return 0;
}
