/*
 * server.hh
 */

#ifndef __SERVER_HH__
#define __SERVER_HH__

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "BackendStorer.hh"
#include "DedupCore.hh"
#include "minDedupCore.hh"

#define BUFFER_LEN (4 * 1024 * 1024)
#define META_LEN (2 * 1024 * 1024)
#define META (-1)
#define DATA (-2)
#define STAT (-3)
#define DOWNLOAD (-7)
#define KEYFILE (-108)
#define KEY_RECIPE (-101)
#define GET_KEY_RECIPE (-102)
#define FILE_RECIPE (-103)

using namespace std;

class Server {
private:
    //port number
    int dataHostPort_;
    int metaHostPort_;

    //server address struct
    struct sockaddr_in dataAddr_;
    struct sockaddr_in metaAddr_;
    //receiving socket
    int dataHostSock_;

    //receiving socket
    int metaHostSock_;

    //socket size
    socklen_t addrSize_;

    //client socket
    int* dataclientSock_;
    int* metaclientSock_;
    //socket address
    struct sockaddr_in sadr_;

    //thread ID
    pthread_t threadId_;

public:
    /*the entry structure of the recipes of a file*/
    typedef struct {
        char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
    } fileRecipeEntry_t;

    /*the head structure of the recipes of a file*/
    typedef struct {
        int userID;
        long fileSize;
        int numOfShares;
    } fileRecipeHead_t;

    /*
 	 * constructor: initialize host socket
 	 *
 	 * @param metaPort - meta service port number
 	 * @param dataPort - data service port number
 	 * @param dedupObj - meta dedup object passed in
 	 * @param minDedupObj - data dedup object passed in
 	 *
 	 */
    Server(int metaPort, int dataPort, DedupCore* dedupObj, minDedupCore* dataDedupObj);

    /*
 	 * start linsten sockets and bind correct thread for coming connection
 	 *
 	 */
    void runReceive();

};
#endif
