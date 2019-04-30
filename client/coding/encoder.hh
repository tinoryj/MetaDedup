/*
 *  encoder.hh 
 */

#ifndef __ENCODER_HH__
#define __ENCODER_HH__

//#define ENCODE_ONLY_MODE 1
#include "BasicRingBuffer.hh"
#include "CDCodec.hh"
#include "CryptoPrimitive.hh"
#include "conf.hh"
#include "uploader.hh"
#include <openssl/bn.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define HASH_SIZE 32
#define KEY_SIZE 32

/* num of encoder threads */
#define NUM_THREADS 2

/* ringbuffer size */
#define RB_SIZE (1024)

/* max secret size */
#define SECRET_SIZE (16 * 1024)
#define SECRET_SIZE_META (32 * 1024)
/* max share buffer size */
#define SHARE_BUFFER_SIZE (4 * 16 * 1024)

/* object type indicators */
#define FILE_OBJECT 1
#define FILE_HEADER (-9)
#define SHARE_OBJECT (-8)
#define SHARE_END (-27)

#define AVG_SEGMENT_SIZE ((2 << 19)) //0.5MB defaults
#define MIN_SEGMENT_SIZE ((2 << 18)) //256KB
#define MAX_SEGMENT_SIZE ((2 << 20)) //1MB

#define DIVISOR ((AVG_SEGMENT_SIZE - MIN_SEGMENT_SIZE) / (8 * (2 << 10)))
#define PATTERN ((AVG_SEGMENT_SIZE - MIN_SEGMENT_SIZE) / (8 * (2 << 10))) - 1

class Encoder {
public:
    /* threads parameter structure */
    typedef struct {
        int index; // thread number
        Encoder* obj; // encoder object pointer
    } param_encoder;

    /* file head structure */
    typedef struct {
        unsigned char data[SECRET_SIZE];
        int fullNameSize;
        int fileSize;
    } fileHead_t;

    /* secret metadata structure */
    typedef struct {
        unsigned char data[SECRET_SIZE];
        unsigned char key[KEY_SIZE];
        int secretID;
        int secretSize;
        int end;
    } Secret_t;

    /* share metadata structure */
    typedef struct {
        unsigned char data[SHARE_BUFFER_SIZE];
        unsigned char key[KEY_SIZE];
        char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
        int shareSize;
        int end;
    } ShareChunk_t;

    /*the entry structure of the recipes of a file*/
    typedef struct {
        char shareFP[FP_SIZE];
        int secretID;
        int secretSize;
    } fileRecipeEntry_t;

    /* union header for secret ringbuffer */
    typedef struct {
        union {
            Secret_t secret;
            fileHead_t file_header;
        };
        int type;
    } Secret_Item_t;

    /* union header for share ringbuffer */
    typedef struct {
        union {
            ShareChunk_t share_chunk;
            fileHead_t file_header;
        };
        int type;
    } ShareChunk_Item_t;

    /* the input secret ringbuffer */
    RingBuffer<Secret_Item_t>** inputbuffer_;

    /* the output share ringbuffer */
    RingBuffer<ShareChunk_Item_t>** outputbuffer_;

    /* thread id array */
    pthread_t tid_[NUM_THREADS + 1];

    /* the total number of clouds */
    int n_;

    /* index for sequencially adding object */
    int nextAddIndex_;

    /* coding object array */
    CDCodec* encodeObj_[NUM_THREADS];

    /* uploader object */
    Uploader* uploadObj_;

    /* crypto object array */
    CryptoPrimitive** cryptoObj_;

    // segment temp

    typedef struct {
        unsigned char shareFP[HASH_SIZE];
        unsigned char other[18];
        int secretID;
        int secretSize;
        int shareSize;
    } metaNode;

    /*
     * constructor of encoder
     *
     * @param type - convergent dispersal type
     * @param n - total number of shares generated from a secret
     * @param m - reliability degree
     * @param r - confidentiality degree
     * @param securetype - encryption and hash type
     * @param uploaderObj - pointer link to uploader object
     *
     *
     */
    Encoder(int type,
        int n,
        int m,
        int r,
        int securetype,
        Uploader* uploaderObj);

    /*
     * destructor of encoder
     */
    ~Encoder();

    /*
     * test if it's end of encoding a file
     */
    void indicateEnd();

    /*
     * add function for sequencially add items to each encode buffer
     *
     * @param item - input object
     */
    int add(Secret_Item_t* item);

    /*
     * thread handler for encoding secret into shares
     *
     * @param param - parameters for each thread
     */
    static void* thread_handler(void* param);

    /*
     * collect thread for getting share objects in order
     *
     * @param param - parameters for collect thread
     */
    static void* collect(void* param);
};

#endif
