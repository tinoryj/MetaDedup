/*
 * server.cc
 */

#include "server.hh"
#include <string.h>
#include <string>
#include <sys/time.h>

DedupCore* dedupObj_;
minDedupCore* dataDedupObj_;
pthread_mutex_t mutex;
using namespace std;
/*
 * constructor: initialize host socket
 *
 * @param metaPort - meta service port number
 * @param dataPort - data service port number
 * @param dedupObj - meta dedup object passed in
 * @param minDedupObj - data dedup object passed in
 *
 */
Server::Server(int metaPort, int dataPort, DedupCore* dedupObj, minDedupCore* dataDedupObj)
{
    //dedup. object
    dedupObj_ = dedupObj;
    dataDedupObj_ = dataDedupObj;
    //server port
    dataHostPort_ = dataPort;
    metaHostPort_ = metaPort;

    //server socket initialization
    dataHostSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (dataHostSock_ == -1) {

        printf("Error initializing socket %d\n", errno);
    }

    metaHostSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (metaHostSock_ == -1) {

        printf("Error initializing socket %d\n", errno);
    }

    //set data socket options
    int* p_int = (int*)malloc(sizeof(int));
    *p_int = 1;

    if ((setsockopt(dataHostSock_, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1) || (setsockopt(dataHostSock_, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1)) {

        printf("Error setting options %d\n", errno);
        free(p_int);
    }

    //set key socket options
    *p_int = 1;

    if ((setsockopt(metaHostSock_, SOL_SOCKET, SO_REUSEADDR, (char*)p_int, sizeof(int)) == -1) || (setsockopt(metaHostSock_, SOL_SOCKET, SO_KEEPALIVE, (char*)p_int, sizeof(int)) == -1)) {

        printf("Error setting options %d\n", errno);
        free(p_int);
    }
    free(p_int);

    //initialize address struct
    dataAddr_.sin_family = AF_INET;
    dataAddr_.sin_port = htons(dataHostPort_);

    memset(&(dataAddr_.sin_zero), 0, 8);
    dataAddr_.sin_addr.s_addr = INADDR_ANY;

    //bind port
    if (bind(dataHostSock_, (sockaddr*)&dataAddr_, sizeof(dataAddr_)) == -1) {
        fprintf(stderr, "Error binding to socket %d\n", errno);
    }

    //start to listen
    if (listen(dataHostSock_, 10) == -1) {
        fprintf(stderr, "Error listening %d\n", errno);
    }

    metaAddr_.sin_family = AF_INET;
    metaAddr_.sin_port = htons(metaHostPort_);

    memset(&(metaAddr_.sin_zero), 0, 8);
    metaAddr_.sin_addr.s_addr = INADDR_ANY;

    //bind port
    if (bind(metaHostSock_, (sockaddr*)&metaAddr_, sizeof(metaAddr_)) == -1) {
        fprintf(stderr, "Error binding to socket %d\n", errno);
    }

    //start to listen
    if (listen(metaHostSock_, 10) == -1) {
        fprintf(stderr, "Error listening %d\n", errno);
    }
}

void timerStart(double* t)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *t = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

double timerSplit(const double* t)
{
    struct timeval tv;
    double cur_t;
    gettimeofday(&tv, NULL);
    cur_t = (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
    return (cur_t - *t);
}

/*
 * Meta Thread function: each thread maintains a socket from a certain client
 *
 * @param lp - input parameter structure
 *
 */
void* SocketHandlerMeta(void* lp)
{
    //double timer,split,bw;

    //get socket from input param
    int* clientSock = (int*)lp;

    //variable initialization
    int bytecount;
    char* buffer = (char*)malloc(sizeof(char) * BUFFER_LEN);
    char* metaBuffer = (char*)malloc(sizeof(char) * META_LEN);
    bool* statusList = (bool*)malloc(sizeof(bool) * BUFFER_LEN);
    memset(statusList, 0, sizeof(bool) * BUFFER_LEN);
    int metaSize;
    int user = 0;
    int dataSize = 0;
    //get user ID
    if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
        fprintf(stderr, "Error recv userID %d\n", errno);
    }
    user = ntohl(*(int*)buffer);

    memset(buffer, 0, BUFFER_LEN);
    int numOfShare = 0;

    //initialize hash object
    CryptoPrimitive* hashObj = new CryptoPrimitive(SHA256_TYPE);

    //main loop for recv data package
    while (true) {

        /*recv indicator first*/
        if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
            fprintf(stderr, "Error receiving data %d\n", errno);
        }

        /*if client closes, break loop*/
        if (bytecount == 0)
            break;

        int indicator = *(int*)buffer;

        if (indicator == KEY_RECIPE) {

            /* get key file size */
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            int length = *(int*)buffer;
            /* get file name size */
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            int namesize = *(int*)buffer;
            printf("namesize = %d\n", namesize);

            char* keybuffer = (char*)malloc(sizeof(char) * length);

            char namebuffer[namesize + 1];
            if ((bytecount = recv(*clientSock, namebuffer, namesize, 0)) == -1) {

                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            namebuffer[namesize] = '\0';
            int id = 0;
            while (namebuffer[id] != '\0') {
                id++;
                if (namebuffer[id] == '/') {
                    namebuffer[id] = '_';
                }
            }
            /* create a new cipher file */
            char name[256];
            sprintf(name, "meta/keystore/%s", namebuffer);
            printf("key file name : %s\n", name);
            FILE* wp = fopen(name, "wb+");
            if (wp == NULL) {
                printf("key file can not creat\n");
            }
            /* get stub */
            int total = 0;
            printf("key file length = %d\n", length);
            while (total < length) {

                if ((bytecount = recv(*clientSock, keybuffer + total, length - total, 0)) == -1) {

                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                total += bytecount;
            }

            fwrite(keybuffer, 1, length, wp);
            fclose(wp);
            free(keybuffer);
            continue;
        }

        /* if it's key download */
        if (indicator == GET_KEY_RECIPE) {

            /* get file name size */

            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            int namesize = *(int*)buffer;
            char namebuffer[namesize + 1];

            if ((bytecount = recv(*clientSock, namebuffer, namesize, 0)) == -1) {

                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            namebuffer[namesize] = '\0';
            int id = 0;
            while (namebuffer[id] != '\0') {
                id++;
                if (namebuffer[id] == '/') {
                    namebuffer[id] = '_';
                }
            }
            /* find stub file */
            char name[256];
            sprintf(name, "meta/keystore/%s", namebuffer);
            FILE* rp = fopen(name, "r");
            if (rp == NULL) {

                printf("file not exist\ndownload fail\n");
                break;
            }

            fseek(rp, 0, SEEK_END);
            int length = ftell(rp);
            fseek(rp, 0, SEEK_SET);
            char* stubtemp = (char*)malloc(sizeof(char) * length);

            int ret;
            ret = fread(stubtemp, 1, length, rp);
            if (ret != length) {
                printf("error reading cipher file\n");
            }
            /* send key size back */

            printf("key file length = %d\n", length);
            if ((bytecount = send(*clientSock, &length, sizeof(int), 0)) == -1) {

                fprintf(stderr, "Error sending data %d\n", errno);
            }
            int total = 0;

            while (total < length) {

                if ((bytecount = send(*clientSock, stubtemp + total, length - total, 0)) == -1) {

                    fprintf(stderr, "Error sending data %d\n", errno);
                }
                total += bytecount;
            }
            free(stubtemp);
            fclose(rp);
            continue;
        }

        if (indicator == FILE_RECIPE) {

            pthread_mutex_lock(&mutex);

            /* get key file size */
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            int length = *(int*)buffer;
            char* keybuffer = (char*)malloc(sizeof(char) * sizeof(fileRecipeEntry_t) * 1000);
            /* get file name size */

            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            int namesize = *(int*)buffer;
            char namebuffer[namesize + 1];

            if ((bytecount = recv(*clientSock, namebuffer, namesize, 0)) == -1) {

                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            namebuffer[namesize] = '\0';
            int id = 0;
            while (namebuffer[id] != '\0') {
                id++;
                if (namebuffer[id] == '/') {
                    namebuffer[id] = '_';
                }
            }
            /* create a new cipher file */
            char name[256];
            sprintf(name, "meta/RecipeFiles/%s", namebuffer);

            FILE* wp = fopen(name, "ab+");
            if (wp == NULL) {
                printf("recipe file can not creat\n");
            }
            /* get stub */
            int total = 0;
            char* headerbuffer = (char*)malloc(sizeof(char) * sizeof(fileRecipeHead_t));
            if ((bytecount = recv(*clientSock, headerbuffer, sizeof(fileRecipeHead_t), 0)) == -1) {

                fprintf(stderr, "Error receiving data %d\n", errno);
            }
            fwrite(headerbuffer, 1, sizeof(fileRecipeHead_t), wp);
            length -= sizeof(fileRecipeHead_t);
            while (total < length) {
                if (length - total >= sizeof(fileRecipeEntry_t) * 1000) {
                    if ((bytecount = recv(*clientSock, keybuffer, sizeof(fileRecipeEntry_t) * 1000, 0)) == -1) {

                        fprintf(stderr, "Error receiving data %d\n", errno);
                    }
                } else {
                    if ((bytecount = recv(*clientSock, keybuffer, length - total, 0)) == -1) {

                        fprintf(stderr, "Error receiving data %d\n", errno);
                    }
                }
                total += bytecount;
                fwrite(keybuffer, 1, bytecount, wp);
            }
            fclose(wp);
            free(headerbuffer);
            free(keybuffer);
            pthread_mutex_unlock(&mutex);
            break;
        }
        /*while metadata recv.ed, perform first stage deduplication*/
        if (indicator == META) {

            /*recv following package size*/
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }

            memcpy(metaBuffer, buffer, count);
            metaSize = count;

            dedupObj_->firstStageDedup(user, (unsigned char*)metaBuffer, count, statusList, numOfShare, dataSize);

            int ind = STAT;
            memcpy(buffer, &ind, sizeof(int));

            /*return the status list*/
            int bytecount;
            if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }

            memcpy(buffer, &numOfShare, sizeof(int));
            if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }

            if ((bytecount = send(*clientSock, statusList, sizeof(bool) * numOfShare, 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }
        }

        /*while data recv.ed, perform second stage deduplication*/
        if (indicator == DATA) {

            /*recv following package size*/
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }

            dedupObj_->secondStageDedup(user, (unsigned char*)metaBuffer, metaSize, statusList, (unsigned char*)buffer, hashObj);
        }

        /*while download request recv.ed, perform restore*/
        if (indicator == DOWNLOAD) {

            /*recv following package size*/
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }
            std::string fullFileName;
            fullFileName.assign(buffer, count);
            dedupObj_->restoreShareFile(user, fullFileName, 0, *clientSock, hashObj);
        }
    }
    delete hashObj;
    free(buffer);
    free(statusList);
    free(metaBuffer);
    free(clientSock);
    return 0;
}

/*
 * Data Thread function: each thread maintains a socket from a certain client
 *
 * @param lp - input parameter structure
 *
 */
void* SocketHandlerData(void* lp)
{
    //get socket from input param
    int* clientSock = (int*)lp;

    //variable initialization
    int bytecount;
    char* buffer = (char*)malloc(sizeof(char) * BUFFER_LEN);
    char* metaBuffer = (char*)malloc(sizeof(char) * META_LEN);
    bool* statusList = (bool*)malloc(sizeof(bool) * BUFFER_LEN);
    memset(statusList, 0, sizeof(bool) * BUFFER_LEN);
    int metaSize;
    int user = 0;
    int dataSize = 0;
    //get user ID
    if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
        fprintf(stderr, "Error recv userID %d\n", errno);
    }
    user = ntohl(*(int*)buffer);

    memset(buffer, 0, BUFFER_LEN);
    int numOfShare = 0;

    //initialize hash object
    CryptoPrimitive* hashObj = new CryptoPrimitive(SHA256_TYPE);

    //main loop for recv data package
    while (true) {

        /*recv indicator first*/
        if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
            fprintf(stderr, "Error receiving data %d\n", errno);
        }

        /*if client closes, break loop*/
        if (bytecount == 0)
            break;

        int indicator = *(int*)buffer;
        /*while metadata recv.ed, perform first stage deduplication*/
        if (indicator == META) {

            /*recv following package size*/
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }

            memcpy(metaBuffer, buffer, count);
            metaSize = count;
            dataDedupObj_->firstStageDedup(user, (unsigned char*)metaBuffer, count, statusList, numOfShare, dataSize);

            int ind = STAT;
            memcpy(buffer, &ind, sizeof(int));

            /*return the status list*/
            int bytecount;
            if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }

            memcpy(buffer, &numOfShare, sizeof(int));
            if ((bytecount = send(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }

            if ((bytecount = send(*clientSock, statusList, sizeof(bool) * numOfShare, 0)) == -1) {
                fprintf(stderr, "Error sending data %d\n", errno);
            }
        }

        /*while data recv.ed, perform second stage deduplication*/
        if (indicator == DATA) {

            /*recv following package size*/

            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }

            dataDedupObj_->secondStageDedup(user, (unsigned char*)metaBuffer, metaSize, statusList, (unsigned char*)buffer, hashObj);
        }

        /*while download request recv.ed, perform restore*/
        if (indicator == DOWNLOAD) {

            /*recv following package size*/
            if ((bytecount = recv(*clientSock, buffer, sizeof(int), 0)) == -1) {
                fprintf(stderr, "Error receiving data %d\n", errno);
            }

            int packageSize = *(int*)buffer;
            int count = 0;

            /*recv following data*/
            while (count < packageSize) {
                if ((bytecount = recv(*clientSock, buffer + count, packageSize - count, 0)) == -1) {
                    fprintf(stderr, "Error receiving data %d\n", errno);
                }
                count += bytecount;
            }
            buffer[packageSize] = '\0';
            int id = 0;
            while (buffer[id] != '\0') {
                id++;
                if (buffer[id] == '/') {
                    buffer[id] = '_';
                }
            }
            char name[256];
            sprintf(name, "meta/RecipeFiles/%s", buffer);
            std::string fullFileName(name);
            //fullFileName.assign(buffer, count);

            pthread_mutex_lock(&mutex);
            dataDedupObj_->restoreShareFile(user, fullFileName, 0, *clientSock, hashObj);
            pthread_mutex_unlock(&mutex);
        }
    }

    delete hashObj;
    free(buffer);
    free(statusList);
    free(metaBuffer);
    free(clientSock);
    return 0;
}

/*
 * start linsten sockets and bind correct thread for coming connection
 *
 */
void Server::runReceive()
{

    addrSize_ = sizeof(sockaddr_in);
    pthread_mutex_init(&mutex, NULL);
    //create a thread whenever a client connects
    while (true) {

        printf("waiting for a connection\n");
        dataclientSock_ = (int*)malloc(sizeof(int));
        metaclientSock_ = (int*)malloc(sizeof(int));

        if ((*dataclientSock_ = accept(dataHostSock_, (sockaddr*)&sadr_, &addrSize_)) != -1) {

            printf("Received data connection from %s\n", inet_ntoa(sadr_.sin_addr));
            pthread_create(&threadId_, 0, &SocketHandlerData, (void*)dataclientSock_);

            pthread_detach(threadId_);

        } else {

            fprintf(stderr, "Error accepting %d\n", errno);
        }

        if ((*metaclientSock_ = accept(metaHostSock_, (sockaddr*)&sadr_, &addrSize_)) != -1) {

            printf("Received meta connection from %s\n", inet_ntoa(sadr_.sin_addr));
            pthread_create(&threadId_, 0, &SocketHandlerMeta, (void*)metaclientSock_);

            pthread_detach(threadId_);

        } else {

            fprintf(stderr, "Error accepting %d\n", errno);
        }
    }
    pthread_mutex_destroy(&mutex);
}
