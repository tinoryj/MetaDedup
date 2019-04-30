/*
 * downloader.cc
 */

#include "downloader.hh"

using namespace std;

/*
 * downloader thread handler
 * 
 * @param param - input param structure
 *
 */
void* Downloader::thread_handler(void* param)
{

    /* parse parameters*/
    param_t* temp = (param_t*)param;
    int cloudIndex = temp->cloudIndex;
    Downloader* obj = temp->obj;
    free(temp);

    /* get the download initiate signal */
    init_t signal;
    obj->signalBuffer_[cloudIndex]->Extract(&signal);

    /* get filename & name size*/
    char* filename = signal.filename;
    int namesize = signal.namesize;
    int retSize;
    int index = 0;

    /* initiate download request */
    obj->socketArray_[cloudIndex]->initDownload(filename, namesize);

    /* start to download data into container */
    obj->socketArray_[cloudIndex]->downloadChunk(obj->downloadContainer_[cloudIndex], &retSize);
    /* get the header */
    shareFileHead_t* header = (shareFileHead_t*)obj->downloadContainer_[cloudIndex];
    index = sizeof(shareFileHead_t);

    /* parse the header object */
    Item_t headerObj;
    headerObj.type = 0;
    memcpy(&(headerObj.fileObj.file_header), header, sizeof(shareFileHead_t));

    /* add the header object into ringbuffer */
    obj->ringBuffer_[cloudIndex - 3]->Insert(&headerObj, sizeof(headerObj));
    /* main loop to get data */
    int count = 0;
    int numOfChunk = header->numOfShares;
    while (true) {

        /* if the current comtainer has been proceed, download next container */
        if (index == retSize) {
            obj->socketArray_[cloudIndex]->downloadChunk(obj->downloadContainer_[cloudIndex], &retSize);
            index = 0;
        }

        /* get the share object */
        shareEntry_t* temp = (shareEntry_t*)(obj->downloadContainer_[cloudIndex] + index);
        int shareSize = temp->shareSize;
        index += sizeof(shareEntry_t);

        /* parse the share object */
        Item_t output;
        output.type = 1;
        memcpy(&(output.shareObj.share_header), temp, sizeof(shareEntry_t));
        memcpy(output.shareObj.data, obj->downloadContainer_[cloudIndex] + index, shareSize);

        index += shareSize;

        /* add the share object to ringbuffer */
        obj->ringBuffer_[cloudIndex - 3]->Insert(&output, sizeof(output));
        count++;
        if (count == numOfChunk) {
            break;
        }
    }
    return NULL;
}

/*
 * downloader thread handler
 * 
 * @param param - input param structure
 *
 */
void* Downloader::thread_handler_meta(void* param)
{

    /* parse parameters*/
    param_t* temp = (param_t*)param;
    int cloudIndex = temp->cloudIndex;
    Downloader* obj = temp->obj;
    free(temp);

    /* get the download initiate signal */
    init_t signal;
    obj->signalBuffer_[cloudIndex]->Extract(&signal);

    /* get filename & name size*/
    char* filename = signal.filename;
    int namesize = signal.namesize;
    int retSize;
    int index = 0;

    /* initiate download request */
    obj->socketArray_[cloudIndex]->initDownload(filename, namesize);
    /* start to download data into container */
    obj->socketArray_[cloudIndex]->downloadChunk(obj->downloadContainer_[cloudIndex], &retSize);
    /* get the header */
    shareFileHead_t* header = (shareFileHead_t*)obj->downloadContainer_[cloudIndex];
    index = sizeof(shareFileHead_t);

    /* parse the header object */
    ItemMeta_t headerObj;
    headerObj.type = 0;
    memcpy(&(headerObj.fileObj.file_header), header, sizeof(shareFileHead_t));

    /* add the header object into ringbuffer */
    obj->ringBufferMeta_[cloudIndex]->Insert(&headerObj, sizeof(headerObj));
    /* main loop to get data */
    int count = 0;
    int numOfChunk = header->numOfShares;
    while (true) {

        /* if the current comtainer has been proceed, download next container */
        if (index == retSize) {
            obj->socketArray_[cloudIndex]->downloadChunk(obj->downloadContainer_[cloudIndex], &retSize);
            index = 0;
        }

        /* get the share object */
        shareEntry_t* temp = (shareEntry_t*)(obj->downloadContainer_[cloudIndex] + index);
        int shareSize = temp->shareSize;
        index += sizeof(shareEntry_t);

        /* parse the share object */
        ItemMeta_t output;
        output.type = 1;
        memcpy(&(output.shareObj.share_header), temp, sizeof(shareEntry_t));
        memcpy(output.shareObj.data, obj->downloadContainer_[cloudIndex] + index, shareSize);

        index += shareSize;

        /* add the share object to ringbuffer */
        obj->ringBufferMeta_[cloudIndex]->Insert(&output, sizeof(output));
        count++;
        if (count == numOfChunk) {
            break;
        }
    }
    return NULL;
}

/*
 * constructor
 *
 * @param userID - ID of the user who initiate download
 * @param total - input total number of clouds
 * @param subset - input number of clouds to be chosen
 * @param obj - decoder pointer
 */
Downloader::Downloader(int total, int subset, int userID, Decoder* obj, char* fileName, int nameSize)
{
    /* set private variables */
    total_ = total * 2;
    subset_ = subset;
    decodeObj_ = obj;
    memcpy(name_, fileName, nameSize);
    userID_ = userID;

    /* initialization*/
    ringBuffer_ = (RingBuffer<Item_t>**)malloc(sizeof(RingBuffer<Item_t>*) * total);
    ringBufferMeta_ = (RingBuffer<ItemMeta_t>**)malloc(sizeof(RingBuffer<ItemMeta_t>*) * total);
    signalBuffer_ = (RingBuffer<init_t>**)malloc(sizeof(RingBuffer<init_t>*) * total_);
    downloadMetaBuffer_ = (char**)malloc(sizeof(char*) * total_);
    downloadContainer_ = (char**)malloc(sizeof(char*) * total_);
    socketArray_ = (Socket**)malloc(sizeof(Socket*) * total_);
    headerArray_ = (fileShareMDHead_t**)malloc(sizeof(fileShareMDHead_t*) * total_);
    fileSizeCounter = (int*)malloc(sizeof(int) * total);

    /* open config file */
    FILE* fp = fopen("./config-d", "rb");
    char line[225];
    const char ch[2] = ":";

    /* initialization loop  */
    for (int i = 0; i < total; i++) {
        signalBuffer_[i] = new RingBuffer<init_t>(DOWNLOAD_RB_SIZE, true, 1);
        ringBufferMeta_[i] = new RingBuffer<ItemMeta_t>(DOWNLOAD_RB_SIZE, true, 1);
        downloadMetaBuffer_[i] = (char*)malloc(sizeof(char) * DOWNLOAD_BUFFER_SIZE);
        downloadContainer_[i] = (char*)malloc(sizeof(char) * DOWNLOAD_BUFFER_SIZE);

        /* create threads */
        param_t* param = (param_t*)malloc(sizeof(param_t)); // thread's parameter
        param->cloudIndex = i;
        param->obj = this;
        pthread_create(&tid_[i], 0, &thread_handler_meta, (void*)param);
        /* get config parameters */
        int ret = fscanf(fp, "%s", line);
        if (ret == 0)
            printf("fail to load config file\n");
        char* token = strtok(line, ch);
        char* ip = token;
        token = strtok(NULL, ch);
        int port = atoi(token);

        /* create sockets */
        socketArray_[i] = new Socket(ip, port, userID);
    }
    for (int i = total; i < total_; i++) {
        signalBuffer_[i] = new RingBuffer<init_t>(DOWNLOAD_RB_SIZE, true, 1);
        ringBuffer_[i - total] = new RingBuffer<Item_t>(DOWNLOAD_RB_SIZE, true, 1);
        downloadMetaBuffer_[i] = (char*)malloc(sizeof(char) * DOWNLOAD_BUFFER_SIZE);
        downloadContainer_[i] = (char*)malloc(sizeof(char) * DOWNLOAD_BUFFER_SIZE);

        /* create threads */
        param_t* param = (param_t*)malloc(sizeof(param_t)); // thread's parameter
        param->cloudIndex = i;
        param->obj = this;
        pthread_create(&tid_[i], 0, &thread_handler, (void*)param);
        /* get config parameters */
        int ret = fscanf(fp, "%s", line);
        if (ret == 0)
            printf("fail to load config file\n");
        char* token = strtok(line, ch);
        char* ip = token;
        token = strtok(NULL, ch);
        int port = atoi(token);

        /* create sockets */
        socketArray_[i] = new Socket(ip, port, userID);
    }
    fclose(fp);
    fileMDHeadSize_ = sizeof(fileShareMDHead_t);
    shareMDEntrySize_ = sizeof(shareMDEntry_t);
}

/*
 * destructor
 *
 */
Downloader::~Downloader()
{
    for (int i = 0; i < total_; i++) {
        delete (signalBuffer_[i]);
        free(downloadMetaBuffer_[i]);
        free(downloadContainer_[i]);
        delete (socketArray_[i]);
    }
    for (int i = 0; i < total_ / 2; i++) {
        delete (ringBufferMeta_[i]);
        delete (ringBuffer_[i]);
    }
    free(signalBuffer_);
    free(ringBuffer_);
    free(ringBufferMeta_);
    free(headerArray_);
    free(socketArray_);
    free(downloadContainer_);
    free(downloadMetaBuffer_);
}

/*
 * main procedure for downloading a file
 *
 * @param filename - targeting filename
 * @param namesize - size of filename
 * @param numOfCloud - number of clouds that we download data
 *
 */
int Downloader::downloadFile(char* filename, int namesize, int numOfCloud)
{

    /* temp share buffer for assemble the ring buffer data chunks*/
    unsigned char* shareBuffer;
    shareBuffer = (unsigned char*)malloc(sizeof(unsigned char) * RING_BUFFER_DATA_SIZE * MAX_NUMBER_OF_CLOUDS);

    char buffer[256];

    /* add init object for download */
    int recipeIndex = 0;
    init_t input;
    for (int i = total_ / 2; i < total_; i++) {

        input.type = 1;
        //copy the corresponding share as file name
        memset(buffer, 0, 256);
        sprintf(buffer, "%s-%d.recipe", name_, recipeIndex);
        string uploadRecipeFileName(buffer);
        input.namesize = uploadRecipeFileName.length();
        memcpy(&input.filename, uploadRecipeFileName.c_str(), input.namesize);
        signalBuffer_[i]->Insert(&input, sizeof(init_t));
        recipeIndex++;
    }
    printf("data download thread start\n");
    /* get the header object from buffer */
    Item_t headerObj;
    for (int i = 0; i < total_ / 2; i++) {
        ringBuffer_[i]->Extract(&headerObj);
    }
    /* parse header object, tell decoder the total number of secret */
    shareFileHead_t* header = &(headerObj.fileObj.file_header);
    int numOfShares = header->numOfShares;
    decodeObj_->setTotal(numOfShares);
    printf("number of chunks = %d\n", numOfShares);
    /* proceed each secret */
    int count = 0;
    while (count < numOfShares) {
        int secretSize = 0;
        int shareSize = 0;
        int id = 0;
        int shareBufferIndex = 0;
        for (int i = 0; i < total_ / 2; i++) {
            Item_t output;

            ringBuffer_[i]->Extract(&output);
            secretSize = output.shareObj.share_header.secretSize;
            shareSize = output.shareObj.share_header.shareSize;
            id = output.shareObj.share_header.secretID;
            memcpy(shareBuffer + shareBufferIndex * shareSize, output.shareObj.data, shareSize);
            shareBufferIndex++;
        }

        /* add the share buffer to the decoder ringbuffer */
        Decoder::ShareChunk_t package;
        package.secretSize = secretSize;
        package.shareSize = shareSize;
        package.secretID = id;
        memcpy(&(package.data), shareBuffer, numOfCloud * shareSize);
        decodeObj_->add(&package, count % DECODE_NUM_THREADS);
        count++;
    }
    free(shareBuffer);
    printf("download over!\n");
    return 0;
}

/*
 * main procedure for pre-downloading a file
 *
 * @param filename - targeting filename
 * @param namesize - size of filename
 * @param numOfCloud - number of clouds that we download data
 *
 */
int Downloader::preDownloadFile(char* filename, int namesize, int numOfCloud)
{

    /* temp share buffer for assemble the ring buffer data chunks*/
    unsigned char* shareBuffer;
    shareBuffer = (unsigned char*)malloc(sizeof(unsigned char) * RING_BUFFER_DATA_SIZE * MAX_NUMBER_OF_CLOUDS);

    unsigned char tmp[namesize * 32];
    int tmp_s;

    // encode the filepath into shares
    decodeObj_->decodeObj_[0]->encoding((unsigned char*)filename, namesize, tmp, &(tmp_s));

    /* add init object for download */
    init_t input;
    for (int i = 0; i < numOfCloud; i++) {
        input.type = 1;

        //copy the corresponding share as file name
        memcpy(&input.filename, tmp + i * tmp_s, tmp_s);
        input.namesize = tmp_s;
        signalBuffer_[i]->Insert(&input, sizeof(init_t));
    }

    /* get the header object from buffer */
    ItemMeta_t headerObj;
    int numOfShares[numOfCloud];

    for (int i = 0; i < numOfCloud; i++) {
        ringBufferMeta_[i]->Extract(&headerObj);
        /* parse header object, tell decoder the total number of secret */
        shareFileHead_t* header = &(headerObj.fileObj.file_header);
        numOfShares[i] = header->numOfShares;
    }
    /* proceed each secret */

    for (int i = 0; i < numOfCloud; i++) {
        for (int j = 0; j < numOfShares[i]; j++) {
            ItemMeta_t output;
            ringBufferMeta_[i]->Extract(&output);
            writeRetrivedFileRecipe(output, i);
        }
    }

    for (int i = 0; i < numOfCloud; i++) {

        uploadRetrivedRecipeFile(i);
    }
    printf("pre - download over!\n");
    free(shareBuffer);
    return -1;
}

/*
 * test if it's the end of downloading a file
 *
 */
int Downloader::indicateEnd()
{

    for (int i = 0; i < total_; i++) {
        /* trying to join all threads */
        pthread_join(tid_[i], NULL);
    }
    return 1;
}

/*
 * download the file's keyRecipe from each cloud for decrypt metadata chunk
 *
 * @param name - targeting filename
 *
 */
int Downloader::downloadKeyFile(char* name)
{

    int indicator = GET_KEY_RECIPE;

    char buffer[256];

    for (int i = 0; i < subset_; i++) {

        // download key recipe from each server
        memset(buffer, 0, 256);
        sprintf(buffer, "%s-share-%d-enc.key", name, i);
        string encFileName(buffer);

        int namesize = encFileName.length();
        socketArray_[i]->genericSend((char*)&indicator, sizeof(int));
        socketArray_[i]->genericSend((char*)&namesize, sizeof(int));
        socketArray_[i]->genericSend((char*)encFileName.c_str(), namesize);
        int length;
        socketArray_[i]->genericDownload((char*)&length, sizeof(int));
        char* keybuffer = (char*)malloc(sizeof(char) * length);
        socketArray_[i]->genericDownload(keybuffer, length);
        memset(buffer, 0, 256);
        sprintf(buffer, "%s-share-%d-enc.key.d", name, i);
        string downloadEncFileName(buffer);

        FILE* wp = fopen(downloadEncFileName.c_str(), "w");
        if (wp == NULL) {

            printf("key-%d.d file can not creat\n", i);
            return 0;
        } else {

            fwrite(keybuffer, 1, length, wp);
            fclose(wp);
        }

        // decode key recipe
        memset(buffer, 0, 256);
        sprintf(buffer, "%s-share-%d-dec.key", name, i);
        string keyFileName(buffer);

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "openssl enc -d -aes-128-cbc -out %s -in %s -pass pass:test", keyFileName.c_str(), downloadEncFileName.c_str());
        pid_t status = system(cmd);
        if (status == -1) {
            printf("error in system call function %s\n", cmd);
        }
        snprintf(cmd, sizeof(cmd), "rm -rf %s", encFileName.c_str());
        status = system(cmd);
        if (status == -1) {
            printf("error in system call function %s\n", cmd);
        }
        snprintf(cmd, sizeof(cmd), "rm -rf %s", downloadEncFileName.c_str());
        status = system(cmd);
        if (status == -1) {
            printf("error in system call function %s\n", cmd);
        }
    }
    return 1;
}

/*
 * rebuilt file recipe by downloaded metadata chunk & keyRecipe
 *
 * @param input - metadata chunk object
 * @param index - cloud id that metadata chunk download 
 * @param numOfCloud - number of clouds that we download data
 *
 */
int Downloader::writeRetrivedFileRecipe(ItemMeta_t& input, int index)
{

    char nameBuffer[256];
    memset(nameBuffer, 0, 256);
    sprintf(nameBuffer, "share-%d.recipe", index);
    string writeName(nameBuffer);

    int metaChunkID = ~input.shareObj.share_header.secretID + 1;

    memset(nameBuffer, 0, 256);
    sprintf(nameBuffer, "%s-share-%d-dec.key", name_, index);
    string keyFileName(nameBuffer);

    int metaKeySize = FP_SIZE * 2 + sizeof(int);

    FILE* kfp = fopen(keyFileName.c_str(), "rb+");
    if (kfp == NULL) {
        printf("can't open recipe file %s\n", writeName.c_str());
    } else {
        char keyBuffer[metaKeySize];
        for (int i = 0; i < metaChunkID; i++) {
            int realRead = fread(keyBuffer, metaKeySize, 1, kfp);
            if (realRead != 1) {
                printf("read file error %s\n", keyFileName.c_str());
            }
        }
        int metaChunkIDTemp;
        memcpy(&metaChunkIDTemp, keyBuffer, sizeof(int));
        metaChunkIDTemp = ~metaChunkIDTemp + 1;
        if (metaChunkIDTemp != metaChunkID) {
            printf("error in get metadata chunk key id = %d", metaChunkID);
            return 0;
        }
        char key[FP_SIZE];
        memcpy(key, keyBuffer + sizeof(int) + FP_SIZE, FP_SIZE);
        char decData[input.shareObj.share_header.shareSize];
        bool decFlag = decodeObj_->cryptoObj_[0]->decryptWithKey((unsigned char*)input.shareObj.data, input.shareObj.share_header.shareSize, (unsigned char*)key, (unsigned char*)decData);
        if (!decFlag) {
            printf("error in decrypt first 32B of metadata chunk id = %d\n", metaChunkID);
            return 0;
        }
        memcpy(input.shareObj.data, decData, input.shareObj.share_header.shareSize);
        fclose(kfp);
    }

    FILE* fp = fopen(writeName.c_str(), "ab+");
    if (fp == NULL) {
        printf("can't open recipe file %s\n", writeName.c_str());
        return 0;
    } else {
        fseek(fp, 0, SEEK_END);
        int nodeNumber = input.shareObj.share_header.shareSize / sizeof(metaNode);
        for (int i = 0; i < nodeNumber; i++) {

            metaNode newNode;
            memcpy(&newNode, input.shareObj.data + i * sizeof(metaNode), sizeof(metaNode));
            fileRecipeEntry_t writeEntryNode;
            memcpy(writeEntryNode.shareFP, newNode.shareFP, FP_SIZE);
            writeEntryNode.secretID = newNode.secretID;
            writeEntryNode.secretSize = newNode.secretSize;
            fileSizeCounter[index] += newNode.secretSize;
            fwrite(&writeEntryNode, sizeof(fileRecipeEntry_t), 1, fp);
        }
        fclose(fp);

        return 1;
    }
    return 1;
}

/*
 * upload retrived file recipe to cloud server for next origin file retrive
 *
 * @param index - the cloud server index which need to upload recipe file
 *
 */
int Downloader::uploadRetrivedRecipeFile(int index)
{

    char buffer[256];
    memset(buffer, 0, 256);
    sprintf(buffer, "share-%d.recipe", index);
    //client side recipe file name
    string recipeFileName(buffer);
    sprintf(buffer, "%s-%d.recipe", name_, index);
    //server side recipe file name
    string uploadRecipeFileName(buffer);
    memset(buffer, 0, 256);
    sprintf(buffer, "%s-share-%d-dec.key", name_, index);
    //the keyRecipe file name which need to clean
    string keyFileName(buffer);

    FILE* fp = fopen(recipeFileName.c_str(), "rb+");
    if (fp == NULL) {
        printf("can't open recipe file %d\n", index);
    } else {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        int shareChunkNumber = size / sizeof(fileRecipeEntry_t);

        fileRecipeHead_t fileRecipeHeader;
        fileRecipeHeader.userID = userID_;
        fileRecipeHeader.fileSize = fileSizeCounter[index];
        fileRecipeHeader.numOfShares = shareChunkNumber;

        fseek(fp, 0, SEEK_SET);
        int uploadSize = size + sizeof(fileRecipeHead_t);
        int indicator = FILE_RECIPE;
        int fileNameSizeTemp = uploadRecipeFileName.length();
        socketArray_[index]->genericSend((char*)&indicator, sizeof(int));
        socketArray_[index]->genericSend((char*)&uploadSize, sizeof(int));
        socketArray_[index]->genericSend((char*)&fileNameSizeTemp, sizeof(int));
        socketArray_[index]->genericSend((char*)uploadRecipeFileName.c_str(), fileNameSizeTemp);

        char uploadRecipeBuffer[sizeof(fileRecipeEntry_t) * 1000];
        char uploadRecipeHeaderBuffer[sizeof(fileRecipeHead_t)];
        memcpy(uploadRecipeHeaderBuffer, &fileRecipeHeader, sizeof(fileRecipeHead_t));
        socketArray_[index]->genericSend(uploadRecipeHeaderBuffer, sizeof(fileRecipeHead_t));
        int totalRead = 0;
        int realRead = 0;
        fseek(fp, 0, SEEK_SET);
        while (totalRead < size) {
            realRead = fread(uploadRecipeBuffer, sizeof(char), sizeof(fileRecipeEntry_t) * 1000, fp);
            socketArray_[index]->genericSend(uploadRecipeBuffer, realRead);
            totalRead += realRead;
        }
        fclose(fp);
    }

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", recipeFileName.c_str());
    pid_t status;
    status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
    }
    snprintf(cmd, sizeof(cmd), "rm -rf %s", uploadRecipeFileName.c_str());
    status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
    }
    snprintf(cmd, sizeof(cmd), "rm -rf %s", keyFileName.c_str());
    status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
    }

    return -2;
}
