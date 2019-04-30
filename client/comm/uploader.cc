/*
 * uploader.cc
 */

#include "uploader.hh"

using namespace std;

/*
 * uploader thread handler
 *
 * @param param - input structure
 *
 */
void* Uploader::thread_handler(void* param)
{
    /* get input parameters */
    param_t* temp = (param_t*)param;
    int cloudIndex = temp->cloudIndex;
    Uploader* obj = temp->obj;
    free(temp);

    Item_t output;

    /* main loop for uploader, end when indicator recv.ed */
    while (true) {
        /* get object from ringbuffer */
        obj->ringBuffer_[cloudIndex - 4]->Extract(&output);

        /* IF this is a file header object.. */
        if (output.type == FILE_HEADER) {

            /* copy object content into metabuffer */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex],
                &(output.fileObj.file_header), obj->fileMDHeadSize_);

            /* head array point to new file header */
            obj->headerArray_[cloudIndex] = (fileShareMDHead_t*)(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex]);

            /* meta index update */
            obj->metaWP_[cloudIndex] += obj->fileMDHeadSize_;

            /* copy file full path name */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex], output.fileObj.data, output.fileObj.file_header.fullNameSize);

            /* meta index update */
            obj->metaWP_[cloudIndex] += obj->headerArray_[cloudIndex]->fullNameSize;

        } else if (output.type == SHARE_OBJECT || output.type == SHARE_END) {
            /* IF this is share object */
            int shareSize = output.shareObj.share_header.shareSize;

            /* see if the container buffer can hold the coming share, if not then perform upload */
            if (shareSize + obj->containerWP_[cloudIndex] > UPLOAD_BUFFER_SIZE) {
                obj->performUpload(cloudIndex);
                obj->updateHeader(cloudIndex);
            }

            /* copy share header into metabuffer */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex], &(output.shareObj.share_header), obj->shareMDEntrySize_);
            obj->metaWP_[cloudIndex] += obj->shareMDEntrySize_;

            /* copy share data into container buffer */
            memcpy(obj->uploadContainer_[cloudIndex] + obj->containerWP_[cloudIndex], output.shareObj.data, shareSize);
            obj->containerWP_[cloudIndex] += shareSize;

            /* record share size */
            obj->shareSizeArray_[cloudIndex][obj->numOfShares_[cloudIndex]] = shareSize;
            obj->numOfShares_[cloudIndex]++;

            /* update file header pointer */
            obj->headerArray_[cloudIndex]->numOfComingSecrets += 1;
            obj->headerArray_[cloudIndex]->sizeOfComingSecrets += output.shareObj.share_header.secretSize;

            /* IF this is the last share object, perform upload and exit thread */
            if (output.type == SHARE_END) {
                obj->performUpload(cloudIndex);
                break;
            }
        }
    }
    pthread_exit(NULL);
}

/*
 * uploader thread handler
 *
 * @param param - input structure
 *
 */
void* Uploader::thread_handler_meta(void* param)
{
    /* get input parameters */
    param_t* temp = (param_t*)param;
    int cloudIndex = temp->cloudIndex;
    Uploader* obj = temp->obj;
    free(temp);

    ItemMeta_t output;

    /* main loop for uploader, end when indicator recv.ed */
    while (true) {
        /* get object from ringbuffer */
        obj->ringBufferMeta_[cloudIndex]->Extract(&output);

        /* IF this is a file header object.. */
        if (output.type == FILE_HEADER) {

            /* copy object content into metabuffer */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex],
                &(output.fileObj.file_header), obj->fileMDHeadSize_);

            /* head array point to new file header */
            obj->headerArray_[cloudIndex] = (fileShareMDHead_t*)(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex]);

            /* meta index update */
            obj->metaWP_[cloudIndex] += obj->fileMDHeadSize_;

            /* copy file full path name */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex], output.fileObj.data, output.fileObj.file_header.fullNameSize);

            /* meta index update */
            obj->metaWP_[cloudIndex] += obj->headerArray_[cloudIndex]->fullNameSize;

        } else if (output.type == SHARE_OBJECT || output.type == SHARE_END) {
            /* IF this is share object */
            int shareSize = output.shareObj.share_header.shareSize;

            /* see if the container buffer can hold the coming share, if not then perform upload */
            if (shareSize + obj->containerWP_[cloudIndex] > UPLOAD_BUFFER_SIZE) {
                obj->performUpload(cloudIndex);
                obj->updateHeader(cloudIndex);
            }

            /* copy share header into metabuffer */
            memcpy(obj->uploadMetaBuffer_[cloudIndex] + obj->metaWP_[cloudIndex], &(output.shareObj.share_header), obj->shareMDEntrySize_);
            obj->metaWP_[cloudIndex] += obj->shareMDEntrySize_;

            /* copy share data into container buffer */
            memcpy(obj->uploadContainer_[cloudIndex] + obj->containerWP_[cloudIndex], output.shareObj.data, shareSize);
            obj->containerWP_[cloudIndex] += shareSize;

            /* record share size */
            obj->shareSizeArray_[cloudIndex][obj->numOfShares_[cloudIndex]] = shareSize;
            obj->numOfShares_[cloudIndex]++;

            /* update file header pointer */
            obj->headerArray_[cloudIndex]->numOfComingSecrets += 1;
            obj->headerArray_[cloudIndex]->sizeOfComingSecrets += output.shareObj.share_header.secretSize;

            /* IF this is the last share object, perform upload and exit thread */
            if (output.type == SHARE_END) {
                obj->performUpload(cloudIndex);
                break;
            }
        }
    }
    int flagTemp = obj->uploadKeyFile(cloudIndex);
    if (flagTemp == 0) {
        printf("error in upload KeyRecipe to cloud server id : %d\n", cloudIndex);
    } else {
        printf("upload KeyRecipe to cloud server id : %d done\n", cloudIndex);
    }
    pthread_exit(NULL);
}
/*
 * constructor
 *
 * @param p - input large prime number
 * @param total - input total number of clouds
 * @param subset - input number of clouds to be chosen
 *
 */
Uploader::Uploader(int total, int subset, int userID, char* fileName, int nameSize)
{

    total_ = total * 2;
    subset_ = subset;

    memcpy(name_, fileName, nameSize);

    /* initialization */
    ringBuffer_ = (RingBuffer<Item_t>**)malloc(sizeof(RingBuffer<Item_t>*) * total_);
    ringBufferMeta_ = (RingBuffer<ItemMeta_t>**)malloc(sizeof(RingBuffer<ItemMeta_t>*) * total);
    uploadMetaBuffer_ = (char**)malloc(sizeof(char*) * total_);
    uploadContainer_ = (char**)malloc(sizeof(char*) * total_);
    containerWP_ = (int*)malloc(sizeof(int) * total_);
    metaWP_ = (int*)malloc(sizeof(int) * total_);
    numOfShares_ = (int*)malloc(sizeof(int) * total_);
    socketArray_ = (Socket**)malloc(sizeof(Socket*) * total_);
    headerArray_ = (fileShareMDHead_t**)malloc(sizeof(fileShareMDHead_t*) * total_);
    shareSizeArray_ = (int**)malloc(sizeof(int*) * total_);

    /* read server ip & port from config file */
    FILE* fp = fopen("./config-u", "rb");
    char line[225];
    const char ch[2] = ":";

    for (int i = 0; i < total; i++) {
        ringBufferMeta_[i] = new RingBuffer<ItemMeta_t>(UPLOAD_RB_SIZE, true, 1);
        shareSizeArray_[i] = (int*)malloc(sizeof(int) * UPLOAD_BUFFER_SIZE);
        uploadMetaBuffer_[i] = (char*)malloc(sizeof(char) * UPLOAD_BUFFER_SIZE);
        uploadContainer_[i] = (char*)malloc(sizeof(char) * UPLOAD_BUFFER_SIZE);
        containerWP_[i] = 0;
        metaWP_[i] = 0;
        numOfShares_[i] = 0;

        param_t* param = (param_t*)malloc(sizeof(param_t)); // thread's parameter
        param->cloudIndex = i;
        param->obj = this;
        pthread_create(&tid_[i], 0, &thread_handler_meta, (void*)param);

        /* line by line read config file*/
        int ret = fscanf(fp, "%s", line);
        if (ret == 0)
            printf("fail to load config file\n");
        char* token = strtok(line, ch);
        char* ip = token;
        token = strtok(NULL, ch);
        int port = atoi(token);

        /* set sockets */
        socketArray_[i] = new Socket(ip, port, userID);
        accuData_[i] = 0;
        accuUnique_[i] = 0;
    }
    for (int i = total; i < total_; i++) {
        ringBuffer_[i - total] = new RingBuffer<Item_t>(UPLOAD_RB_SIZE, true, 1);
        shareSizeArray_[i] = (int*)malloc(sizeof(int) * UPLOAD_BUFFER_SIZE);
        uploadMetaBuffer_[i] = (char*)malloc(sizeof(char) * UPLOAD_BUFFER_SIZE);
        uploadContainer_[i] = (char*)malloc(sizeof(char) * UPLOAD_BUFFER_SIZE);
        containerWP_[i] = 0;
        metaWP_[i] = 0;
        numOfShares_[i] = 0;

        param_t* param = (param_t*)malloc(sizeof(param_t)); // thread's parameter
        param->cloudIndex = i;
        param->obj = this;
        pthread_create(&tid_[i], 0, &thread_handler, (void*)param);

        /* line by line read config file*/
        int ret = fscanf(fp, "%s", line);
        if (ret == 0)
            printf("fail to load config file\n");
        char* token = strtok(line, ch);
        char* ip = token;
        token = strtok(NULL, ch);
        int port = atoi(token);

        /* set sockets */
        socketArray_[i] = new Socket(ip, port, userID);
        accuData_[i] = 0;
        accuUnique_[i] = 0;
    }
    fclose(fp);
    fileMDHeadSize_ = sizeof(fileShareMDHead_t);
    shareMDEntrySize_ = sizeof(shareMDEntry_t);
}

/*
 * destructor
 */
Uploader::~Uploader()
{
    for (int i = 0; i < total_; i++) {
        free(shareSizeArray_[i]);
        free(uploadMetaBuffer_[i]);
        free(uploadContainer_[i]);
        delete (socketArray_[i]);
    }
    for (int i = 0; i < total_ / 2; i++) {
        delete (ringBuffer_[i]);
        delete (ringBufferMeta_[i]);
    }
    free(ringBuffer_);
    free(ringBufferMeta_);
    free(shareSizeArray_);
    free(headerArray_);
    free(socketArray_);
    free(numOfShares_);
    free(metaWP_);
    free(containerWP_);
    free(uploadContainer_);
    free(uploadMetaBuffer_);
}

/*
 * Initiate upload
 *
 * @param cloudIndex - indicate targeting cloud
 * 
 */
int Uploader::performUpload(int cloudIndex)
{
    /* 1st send metadata */
    socketArray_[cloudIndex]->sendMeta(uploadMetaBuffer_[cloudIndex], metaWP_[cloudIndex]);

    /* 2nd get back the status list */
    int numOfshares;
    bool* statusList = (bool*)malloc(sizeof(bool) * (numOfShares_[cloudIndex] + 1));
    socketArray_[cloudIndex]->getStatus(statusList, &numOfshares);

    /* 3rd according to status list, reconstruct the container buffer */
    int buffer_size = 0;
    if (cloudIndex < total_ / 2) {
        buffer_size = RING_BUFFER_META_SIZE;
    } else {
        buffer_size = RING_BUFFER_DATA_SIZE;
    }
    char temp[buffer_size];
    int indexCount = 0;
    int containerIndex = 0;
    int currentSize = 0;
    for (int i = 0; i < numOfshares; i++) {
        currentSize = shareSizeArray_[cloudIndex][i];
        if (statusList[i] == 0) {
            memcpy(temp, uploadContainer_[cloudIndex] + containerIndex, currentSize);
            memcpy(uploadContainer_[cloudIndex] + indexCount, temp, currentSize);
            indexCount += currentSize;
        }
        containerIndex += currentSize;
    }

    /* calculate the amount of sent data */
    accuData_[cloudIndex] += containerIndex;
    accuUnique_[cloudIndex] += indexCount;

    /* finally send the unique data to the cloud */
    socketArray_[cloudIndex]->sendData(uploadContainer_[cloudIndex], indexCount);

    free(statusList);
    return 0;
}

/*
 * procedure for update headers when upload finished
 * 
 * @param cloudIndex - indicating targeting cloud
 *
 *
 *
 */
// 	SEND 1: (4 byte) state update indicator
int Uploader::updateHeader(int cloudIndex)
{

    /* get the file name size */
    int offset = headerArray_[cloudIndex]->fullNameSize;

    /* update header counts */
    headerArray_[cloudIndex]->numOfPastSecrets += headerArray_[cloudIndex]->numOfComingSecrets;
    headerArray_[cloudIndex]->sizeOfPastSecrets += headerArray_[cloudIndex]->sizeOfComingSecrets;

    /* reset coming counts */
    headerArray_[cloudIndex]->numOfComingSecrets = 0;
    headerArray_[cloudIndex]->sizeOfComingSecrets = 0;

    /* reset all index (means buffers are empty) */
    containerWP_[cloudIndex] = 0;
    metaWP_[cloudIndex] = 0;
    numOfShares_[cloudIndex] = 0;

    /* copy the header into metabuffer */
    memcpy(uploadMetaBuffer_[cloudIndex], headerArray_[cloudIndex], fileMDHeadSize_ + offset);
    metaWP_[cloudIndex] += fileMDHeadSize_ + offset;

    return 1;
}

/*
 * interface for adding object to ringbuffer
 *
 * @param item - the object to be added
 * @param size - the size of the object
 * @param index - the buffer index 
 *
 */
int Uploader::add(Item_t* item, int size, int index)
{
    ringBuffer_[index]->Insert(item, size);
    return 1;
}

/*
 * interface for adding object to ringbuffer
 *
 * @param item - the object to be added
 * @param size - the size of the object
 * @param index - the buffer index 
 *
 */
int Uploader::addMeta(ItemMeta_t* item, int size, int index)
{
    ringBufferMeta_[index]->Insert(item, size);
    return 1;
}

/*
 * indicate the end of uploading a file
 * 
 * @return total - total amount of data that input to uploader
 * @return uniq - the amount of unique data that transferred in network
 *
 */
int Uploader::indicateEnd(long long* total, long long* uniq)
{

    for (int i = 0; i < UPLOAD_NUM_THREADS * 2; i++) {

        pthread_join(tid_[i], NULL);
        *total += accuData_[i];
        *uniq += accuUnique_[i];
    }
    return 1;
}

/*
 * upload keyRecipe to cloud server (contains metadata chunk encrypt AES key)
 * 
 * @param index - cloud server id 
 *
 */
int Uploader::uploadKeyFile(int index)
{

    char buffer[256];
    memset(buffer, 0, 256);
    sprintf(buffer, "share-%d.key", index);
    string keyFileName(buffer);
    sprintf(buffer, "%s-share-%d-enc.key", name_, index);
    string encFileName(buffer);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "openssl enc -aes-128-cbc -in %s -out %s -pass pass:test", keyFileName.c_str(), encFileName.c_str());
    pid_t status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "rm -rf %s", keyFileName.c_str());
    status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
        return 0;
    }

    FILE* fp = fopen(encFileName.c_str(), "rb+");
    if (fp == NULL)
        printf("can't open key file\n");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char uploadEncBuffer[size + 1];
    int realRead = fread(uploadEncBuffer, sizeof(char), size, fp);
    if (realRead != size) {
        printf("error in read file %s", encFileName.c_str());
    }
    uploadEncBuffer[size] = '\0';
    fclose(fp);

    int indicator = KEY_RECIPE;
    int fileNameSizeTemp = encFileName.length();
    socketArray_[index]->genericSend((char*)&indicator, sizeof(int));
    socketArray_[index]->genericSend((char*)&size, sizeof(int));
    socketArray_[index]->genericSend((char*)&fileNameSizeTemp, sizeof(int));
    socketArray_[index]->genericSend((char*)encFileName.c_str(), fileNameSizeTemp);
    socketArray_[index]->genericSend(uploadEncBuffer, size);

    snprintf(cmd, sizeof(cmd), "rm -rf %s", encFileName.c_str());
    status = system(cmd);
    if (status == -1) {
        printf("error in system call function %s\n", cmd);
        return 0;
    }
    return 1;
}
