/*
 * main test program
 */
#include <bits/stdc++.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "CDCodec.hh"
#include "CryptoPrimitive.hh"
#include "chunker.hh"
#include "conf.hh"
#include "decoder.hh"
#include "downloader.hh"
#include "encoder.hh"
#include "uploader.hh"

#define MAIN_CHUNK

using namespace std;

Chunker* chunkerObj;
Decoder* decoderObj;
Encoder* encoderObj;
Uploader* uploaderObj;
CryptoPrimitive* cryptoObj;
CDCodec* cdCodecObj;
Downloader* downloaderObj;
Configuration* confObj;

struct timeval timestart;
struct timeval timeend;

void usage(char* s)
{

    printf("usage: ./CLIENT [filename] [userID] [action] [secutiyType]\n");
    printf("\t- [filename]: full path of the file;\n");
    printf("\t- [userID]: use ID of current client;\n");
    printf("\t- [action]: [-u] upload; [-d] download;\n");
    printf("\t- [securityType]: [HIGH] AES-256 & SHA-256; [LOW] AES-128 & SHA-1\n");
    exit(1);
}

int main(int argc, char* argv[])
{

    gettimeofday(&timestart, NULL);
    /* argument test */
    if (argc != 5)
        usage(NULL);
    /* get options */
    int userID = atoi(argv[2]);
    char* opt = argv[3];
    char* securesetting = argv[4];
    /* read file */
    unsigned char* buffer;
    int* chunkEndIndexList;
    int numOfChunks;
    int n, m, k, r, *kShareIDList;
    /* initialize openssl locks */
    if (!CryptoPrimitive::opensslLockSetup()) {
        printf("fail to set up OpenSSL locks\n");
        return 0;
    }

    confObj = new Configuration();
    /* fix parameters here */
    n = confObj->getN();
    m = confObj->getM();
    k = confObj->getK();
    r = confObj->getR();
    /* initialize buffers */
    int bufferSize = confObj->getBufferSize();
    int chunkEndIndexListSize = confObj->getListSize();
    int secretBufferSize = confObj->getSecretBufferSize();
    int shareBufferSize = confObj->getShareBufferSize();
    unsigned char *secretBuffer, *shareBuffer;

    delete confObj;
    buffer = (unsigned char*)malloc(sizeof(unsigned char) * bufferSize);
    chunkEndIndexList = (int*)malloc(sizeof(int) * chunkEndIndexListSize);
    secretBuffer = (unsigned char*)malloc(sizeof(unsigned char) * secretBufferSize);
    shareBuffer = (unsigned char*)malloc(sizeof(unsigned char) * shareBufferSize);
    /* initialize share ID list */
    kShareIDList = (int*)malloc(sizeof(int) * k);
    for (int i = 0; i < k; i++)
        kShareIDList[i] = i;

    /* full file name size process */
    int namesize = 0;
    while (argv[1][namesize] != '\0') {
        namesize++;
    }
    namesize++;
    /* parse secure parameters */
    int securetype = LOW_SEC_PAIR_TYPE;
    if (strncmp(securesetting, "HIGH", 4) == 0)
        securetype = HIGH_SEC_PAIR_TYPE;

    if (strncmp(opt, "-u", 2) == 0 || strncmp(opt, "-a", 2) == 0) {

        FILE* fin = fopen(argv[1], "r");
        /* get file size */
        fseek(fin, 0, SEEK_END);
        long size = ftell(fin);
        fseek(fin, 0, SEEK_SET);

        uploaderObj = new Uploader(n, n, userID, argv[1], namesize);
        encoderObj = new Encoder(CAONT_RS_TYPE, n, m, r, securetype, uploaderObj);
        chunkerObj = new Chunker(VAR_SIZE_TYPE);

        //chunking
        Encoder::Secret_Item_t header;
        header.type = 1;
        memcpy(header.file_header.data, argv[1], namesize);
        header.file_header.fullNameSize = namesize;
        header.file_header.fileSize = size;

        // do encode
        encoderObj->add(&header);

        long total = 0;
        int totalChunks = 0;
        while (total < size) {

            int ret = fread(buffer, 1, bufferSize, fin);
            chunkerObj->chunking(buffer, ret, chunkEndIndexList, &numOfChunks);
            int count = 0;
            int preEnd = -1;
            while (count < numOfChunks) {

                Encoder::Secret_Item_t input;
                input.type = 0;
                input.secret.secretID = totalChunks;
                input.secret.secretSize = chunkEndIndexList[count] - preEnd;
                memcpy(input.secret.data, buffer + preEnd + 1, input.secret.secretSize);
                input.secret.end = 0;

                if (total + ret == size && count + 1 == numOfChunks)
                    input.secret.end = 1;
                encoderObj->add(&input);

                totalChunks++;
                preEnd = chunkEndIndexList[count];
                count++;
            }
            total += ret;
        }
        long long tt = 0, unique = 0;
        uploaderObj->indicateEnd(&tt, &unique);

        delete uploaderObj;
        delete chunkerObj;
        delete encoderObj;
        fclose(fin);
    }

    if (strncmp(opt, "-d", 2) == 0 || strncmp(opt, "-a", 2) == 0) {

        decoderObj = new Decoder(CAONT_RS_TYPE, n, m, r, securetype);
        downloaderObj = new Downloader(k, k, userID, decoderObj, argv[1], namesize);
        char nameBuffer[256];
        sprintf(nameBuffer, "%s.d", argv[1]);
        downloaderObj->downloadKeyFile(argv[1]);
        FILE* fw = fopen(nameBuffer, "wb");

        decoderObj->setFilePointer(fw);
        decoderObj->setShareIDList(kShareIDList);
        int preFlag = downloaderObj->preDownloadFile(argv[1], namesize, k);
        if (preFlag == -1) {
            downloaderObj->downloadFile(argv[1], namesize, k);
        }
        decoderObj->indicateEnd();
        downloaderObj->indicateEnd();
        fclose(fw);
        delete downloaderObj;
        delete decoderObj;
    }

    free(buffer);
    free(chunkEndIndexList);
    free(secretBuffer);
    free(shareBuffer);
    free(kShareIDList);
    CryptoPrimitive::opensslLockCleanup();
    gettimeofday(&timeend, NULL);
    long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) + timeend.tv_usec - timestart.tv_usec;
    double second = diff / 1000000.0;
    printf("the total work time is %ld us = %lf s\n", diff, second);

    return 0;
}
