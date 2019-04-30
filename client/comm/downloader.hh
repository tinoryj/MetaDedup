/*
 * downloader.hh
 */

#ifndef __DOWNLOADER_HH__
#define __DOWNLOADER_HH__

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

/* downloader ringbuffer size */
#define DOWNLOAD_RB_SIZE 2048

/* downloader ringbuffer data max size */
#define RING_BUFFER_DATA_SIZE (16 * 1024)
#define RING_BUFFER_META_SIZE (32 * 1024)

/* downloader buffer size */
#define DOWNLOAD_BUFFER_SIZE (4 * 1024 * 1024)

/* length of hash 256 */
#define HASH_LENGTH 32

/* fingerprint size*/
#define FP_SIZE 32
#define HASH_SIZE 32

#define MAX_NUMBER_OF_CLOUDS 16

/* number of download threads */
#define DOWNLOAD_NUM_THREADS 3

#define KEY_RECIPE (-101)
#define GET_KEY_RECIPE (-102)
#define FILE_RECIPE (-103)

#include "BasicRingBuffer.hh"
#include "CryptoPrimitive.hh"
#include "decoder.hh"
#include "socket.hh"

using namespace std;

/*
 * download module
 * handle share to its targeting cloud
 *
 */
class Downloader {
private:

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

    /* file share count struct for download */
    typedef struct {
        long fileSize;
        int numOfShares;
    } shareFileHead_t;

    /*the head structure of the recipes of a file*/
    typedef struct {
        int userID;
        long fileSize;
        int numOfShares;
    } fileRecipeHead_t;

    /*the entry structure of the recipes of a file*/
    typedef struct {
        char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
    } fileRecipeEntry_t;

    /* share detail struct for download */
    typedef struct {
        int secretID;
        int secretSize;
        int shareSize;
    } shareEntry_t;

    /* file header object structure for ringbuffer */
    typedef struct {
        shareFileHead_t file_header;
        char data[RING_BUFFER_DATA_SIZE];
    } fileHeaderObj_t;

    /* share header object structure for ringbuffer */
    typedef struct {
        shareEntry_t share_header;
        char data[RING_BUFFER_DATA_SIZE];
    } shareHeaderObj_t;

    /* share header object structure for ringbuffer */
    typedef struct {
        shareEntry_t share_header;
        char data[RING_BUFFER_META_SIZE];
    } metaShareHeaderObj_t;

    /* union of objects for unifying ringbuffer objects */
    typedef struct {
        int type;
        union {
            fileHeaderObj_t fileObj;
            shareHeaderObj_t shareObj;
        };
    } Item_t;

    /* union of objects for unifying ringbuffer objects */
    typedef struct {
        int type;
        union {
            fileHeaderObj_t fileObj;
            metaShareHeaderObj_t shareObj;
        };
    } ItemMeta_t;

    /* init object for initiating download */
    typedef struct {
        int type;
        char filename[256];
        int namesize;
    } init_t;

    /* thread parameter structure */
    typedef struct {
        int cloudIndex;
        Downloader* obj;
    } param_t;

    typedef struct {
        unsigned char shareFP[HASH_SIZE];
        unsigned char other[18];
        int secretID;
        int secretSize;
        int shareSize;
    } metaNode;

    /* file header pointer array for modifying header */
    fileShareMDHead_t** headerArray_;

    /* socket array */
    Socket** socketArray_;

    /* metadata buffer */
    char** downloadMetaBuffer_;

    /* container buffer */
    char** downloadContainer_;

    /* size of file header */
    int fileMDHeadSize_;

    /* size of share header */
    int shareMDEntrySize_;

    /* thread id array */
    pthread_t tid_[DOWNLOAD_NUM_THREADS * 2];

    /* decoder object pointer */
    Decoder* decodeObj_;

    /* signal buffer */
    RingBuffer<init_t>** signalBuffer_;

    /* download ringbuffer */
    RingBuffer<Item_t>** ringBuffer_;
    RingBuffer<ItemMeta_t>** ringBufferMeta_;
    char name_[256];

    int* fileSizeCounter;
    int userID_;

    /*
     * constructor
     *
     * @param userID - ID of the user who initiate download
     * @param total - input total number of clouds
     * @param subset - input number of clouds to be chosen
     * @param obj - decoder pointer
     */
    Downloader(int total, int subset, int userID, Decoder* obj, char* fileName, int nameSize);

    /*
     * destructor
     *
     */
    ~Downloader();

    /*
     * test if it's the end of downloading a file
     *
     */
    int indicateEnd();

    /*
     * main procedure for downloading a file
     *
     * @param filename - targeting filename
     * @param namesize - size of filename
     * @param numOfCloud - number of clouds that we download data
     *
     */
    int downloadFile(char* filename, int namesize, int numOfCloud);

    /*
     * main procedure for pre-downloading a file
     *
     * @param filename - targeting filename
     * @param namesize - size of filename
     * @param numOfCloud - number of clouds that we download data
     *
     */
    int preDownloadFile(char* filename, int namesize, int numOfCloud);
    /*
     * downloader thread handler
     * 
     * @param param - input param structure
     *
     */
    static void* thread_handler(void* param);

    /*
     * downloader thread handler
     * 
     * @param param - input param structure
     *
     */
    static void* thread_handler_meta(void* param);

    /*
     * download the file's keyRecipe from each cloud for decrypt metadata chunk
     *
     * @param name - targeting filename
     *
     */
    int downloadKeyFile(char* name);

    /*
     * rebuilt file recipe by downloaded metadata chunk & keyRecipe
     *
     * @param input - metadata chunk object
     * @param index - cloud id that metadata chunk download 
     * @param numOfCloud - number of clouds that we download data
     *
     */
    int writeRetrivedFileRecipe(ItemMeta_t& input, int index);

    /*
     * upload retrived file recipe to cloud server for next origin file retrive
     *
     * @param index - the cloud server index which need to upload recipe file
     *
     */
    int uploadRetrivedRecipeFile(int index);
};
#endif
