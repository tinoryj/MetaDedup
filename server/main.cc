#include "CryptoPrimitive.hh"
#include "DedupCore.hh"
#include "minDedupCore.hh"
#include "server.hh"

using namespace std;

DedupCore* dedupObj;
minDedupCore* minDedupObj;

Server* server;

int main(int argv, char** argc)
{

    /* enable openssl locks */
    if (!CryptoPrimitive::opensslLockSetup()) {
        printf("fail to set up OpenSSL locks\n");

        exit(1);
    }

    /* initialize objects */
    BackendStorer* recipeStorerObj = NULL;
    BackendStorer* containerStorerObj = NULL;
    dedupObj = new DedupCore("./", "meta/DedupDB", "meta/RecipeFiles", "meta/ShareContainers", recipeStorerObj, containerStorerObj);
    minDedupObj = new minDedupCore("./", "meta/minDedupDB", "meta/RecipeFiles", "meta/minShareContainers", containerStorerObj);
    /* initialize server object */
    server = new Server(atoi(argc[1]), atoi(argc[2]), dedupObj, minDedupObj);

    /* run server service */
    server->runReceive();

    /* openssl lock cleanup */
    CryptoPrimitive::opensslLockCleanup();

    return 0;
}
