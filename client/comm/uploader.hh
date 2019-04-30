/*
 * uploader.hh
 */

#ifndef __UPLOADER_HH__
#define __UPLOADER_HH__

#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "BasicRingBuffer.hh"
#include "CDCodec.hh"
#include "CryptoPrimitive.hh"
#include "socket.hh"

/* upload ringbuffer size */
#define UPLOAD_RB_SIZE 2048

/* data buffer size for each object in ringbuffer */
#define RING_BUFFER_DATA_SIZE (16 * 1024)
#define RING_BUFFER_META_SIZE (32 * 1024)

/* upload buffer size */
#define UPLOAD_BUFFER_SIZE (4 * 1024 * 1024)

/* max file full path name size */
#define DIR_MAX_SIZE 255

/* max dir levels */
#define DIR_MAX_LEVEL 8

/* HASH 256 length */
#define HASH_LENGTH 32

/* fingerprint size */
#define FP_SIZE 32

/* minimum ring buffer item size */
#define MINIMUN_ITEM_SIZE 32

/* num of upload threads */
#define UPLOAD_NUM_THREADS 4

/* object type indicators */
#define FILE_HEADER (-9)
#define SHARE_OBJECT (-8)
#define SHARE_END (-27)
#define KEY_RECIPE (-101)
#define GET_KEY_RECIPE (-102)

using namespace std;

/*
 * upload module
 * handle share to its targeting cloud
 *
 */
class Uploader {
private:
    //prime number for compute hash
    long prime_;

    //total number of clouds
    int total_;

    //number of a subset of clouds
    int subset_;

public:
    /* file metadata header structure */
    typedef struct {
        int fullNameSize;
        long fileSize;
        int numOfPastSecrets;
        long sizeOfPastSecrets;
        int numOfComingSecrets;
        long sizeOfComingSecrets;
    } fileShareMDHead_t;

    /* share metadata header structure */
    typedef struct {
        unsigned char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
        int shareSize;
    } shareMDEntry_t;

    /*the entry structure of the recipes of a file*/
    typedef struct {
        char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
    } fileRecipeEntry_t;

    /* file header object struct for ringbuffer */
    typedef struct {
        fileShareMDHead_t file_header;
        unsigned char data[RING_BUFFER_DATA_SIZE];
    } fileHeaderObj_t;

    /* share header object struct for ringbuffer */
    typedef struct {
        shareMDEntry_t share_header;
        unsigned char data[RING_BUFFER_DATA_SIZE];
    } shareHeaderObj_t;
    //meta chunk share
    typedef struct {
        shareMDEntry_t share_header;
        unsigned char data[RING_BUFFER_META_SIZE];
    } metaShareHeaderObj_t;

    /* union of objects for unifying ringbuffer objects */
    typedef struct {
        int type;
        union {
            fileHeaderObj_t fileObj;
            shareHeaderObj_t shareObj;
        };
    } Item_t;

    typedef struct {
        int type;
        union {
            fileHeaderObj_t fileObj;
            metaShareHeaderObj_t shareObj;
        };
    } ItemMeta_t;

    /* thread parameter structure */
    typedef struct {
        int cloudIndex;
        Uploader* obj;
    } param_t;

    /* file header pointer array for modifying header */
    fileShareMDHead_t** headerArray_;

    /* socket array */
    Socket** socketArray_;

    /* metadata buffer */
    char** uploadMetaBuffer_;

    /* container buffer */
    char** uploadContainer_;

    /* container write pointer */
    int* containerWP_;

    /* metadata write pointer */
    int* metaWP_;

    /* indicate the number of shares in a buffer */
    int* numOfShares_;

    /* array for record each share size */
    int** shareSizeArray_;

    /* size of file metadata header */
    int fileMDHeadSize_;

    /* size of share metadata header */
    int shareMDEntrySize_;

    /* thread id array */
    pthread_t tid_[UPLOAD_NUM_THREADS * 2];

    /* record accumulated processed data */
    long long accuData_[UPLOAD_NUM_THREADS * 2];

    /* record accumulated unique data */
    long long accuUnique_[UPLOAD_NUM_THREADS * 2];

    /* uploader ringbuffer array */
    RingBuffer<Item_t>** ringBuffer_;
    RingBuffer<ItemMeta_t>** ringBufferMeta_;
    char name_[256];

    /*
     * constructor
     *
     * @param p - input large prime number
     * @param total - input total number of clouds
     * @param subset - input number of clouds to be chosen
     *
     */
    Uploader(int total, int subset, int userID, char* fileName, int nameSize);

    /*
     * destructor
     */
    ~Uploader();

    /*
     * Initiate upload
     *
     * @param cloudIndex - indicate targeting cloud
     * 
     */
    int performUpload(int cloudIndex);

    /*
     * indicate the end of uploading a file
     * 
     * @return total - total amount of data that input to uploader
     * @return uniq - the amount of unique data that transferred in network
     *
     */
    int indicateEnd(long long* total, long long* uniq);

    /*
     * interface for adding object to ringbuffer
     *
     * @param item - the object to be added
     * @param size - the size of the object
     * @param index - the buffer index 
     *
     */
    int add(Item_t* item, int size, int index);

    /*
     * interface for adding object to ringbuffer
     *
     * @param item - the object to be added
     * @param size - the size of the object
     * @param index - the buffer index 
     *
     */
    int addMeta(ItemMeta_t* item, int size, int index);

    /*
     * procedure for update headers when upload finished
     * 
     * @param cloudIndex - indicating targeting cloud
     *
     *
     *
     */
    int updateHeader(int cloudIndex);

    /*
     * uploader thread handler
     *
     * @param param - input structure
     *
     */
    static void* thread_handler(void* param);

    /*
     * uploader thread handler
     *
     * @param param - input structure
     *
     */
    static void* thread_handler_meta(void* param);

    /*
     * upload keyRecipe to cloud server (contains metadata chunk encrypt AES key)
     * 
     * @param index - cloud server id 
     *
     */
    int uploadKeyFile(int index);
};
#endif
