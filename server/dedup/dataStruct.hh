#ifndef __DATASTRUCT_HH__
#define __DATASTRUCT_HH__

/*macros for LevelDB option settings*/
#define MEM_TABLE_SIZE (16 << 20)
#define BLOCK_CACHE_SIZE (32 << 20)
#define BLOOM_FILTER_KEY_BITS 10

/*macro for the name size of internal files (recipe  or container files)*/
#define INTERNAL_FILE_NAME_SIZE 16

/*macros for inodeType*/
#define DIR_TYPE 0
#define FILE_TYPE 1

/*macros for per-user buffer*/
#define RECIPE_BUFFER_SIZE (4 << 20)
#define CONTAINER_BUFFER_SIZE (4 << 20)
#define MAX_BUFFER_WAIT_SECS 18

/*macro for fingerprint size with the use of SHA-256 CryptoPrimitive instance*/
#define FP_SIZE 32

/*macro for key and key size*/
#define KEY_SIZE (FP_SIZE + 1)
#define MAX_VALUE_SIZE (FP_SIZE + 1)

/*macro for share file buffer size*/
#define SHARE_FILE_BUFFER_SIZE (4 << 20)

/*macro for the number of cached share containers*/
#define NUM_OF_CACHED_CONTAINERS 4

using namespace std;

/*shareMDBuffer format: [fileShareMDHead_t + full file name + shareMDEntry_t ... shareMDEntry_t] ...*/
/*the full file name includes the prefix path*/

/*the head structure of the file share metadata*/
typedef struct {
    int fullNameSize;
    long fileSize;
    int numOfPastSecrets;
    long sizeOfPastSecrets;
    int numOfComingSecrets;
    long sizeOfComingSecrets;
} fileShareMDHead_t;

/*the entry structure of the file share metadata*/
typedef struct {
    char shareFP[FP_SIZE];
    int secretID;
    int secretSize;
    int shareSize;
} shareMDEntry_t;

/*dir inode value format: [inodeIndexValueHead_t + short name + inodeDirEntry_t ... inodeDirEntry_t]*/
/*file inode value format: [inodeIndexValueHead_t + short name + inodeFileEntry_t ... inodeFileEntry_t]*/
/*the short name excludes the prefix path*/

/*the head structure of the value of the inode index*/
typedef struct {
    int userID;
    int shortNameSize;
    bool inodeType;
    int numOfChildren;
} inodeIndexValueHead_t;

/*the dir entry structure of the value of the inode index*/
typedef struct {
    char inodeFP[FP_SIZE];
} inodeDirEntry_t;

/*the file entry structure of the value of the inode index*/
typedef struct {
    char recipeFileName[INTERNAL_FILE_NAME_SIZE];
    int recipeFileOffset;
} inodeFileEntry_t;

/*share index value format: [shareIndexValueHead_t + shareUserRefEntry_t ... shareUserRefEntry_t]*/

/*the head structure of the value of the share index*/
typedef struct {
    char shareContainerName[INTERNAL_FILE_NAME_SIZE];
    int shareContainerOffset;
    int shareSize;
    int numOfUsers;
} shareIndexValueHead_t;

/*the user reference entry structure of the value of the share index*/
typedef struct {
    int userID;
    int refCnt;
} shareUserRefEntry_t;

/*file recipe format: [fileRecipeHead_t + fileRecipeEntry_t ... fileRecipeEntry_t]*/

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

/*the per-user buffer node structure*/
typedef struct perUserBufferNode {
    int userID;
    char recipeFileName[INTERNAL_FILE_NAME_SIZE];
    unsigned char recipeFileBuffer[RECIPE_BUFFER_SIZE];
    int recipeFileBufferCurrLen;
    int lastRecipeHeadPos;
    char lastInodeFP[FP_SIZE];
    char shareContainerName[INTERNAL_FILE_NAME_SIZE];
    unsigned char shareContainerBuffer[CONTAINER_BUFFER_SIZE];
    int shareContainerBufferCurrLen;
    double lastUseTime;
    struct perUserBufferNode* next;
} perUserBufferNode_t;

/*restored share file format: [shareFileHead_t + shareEntry_t + share data + ... + shareEntry_t + share data]*/

/*the head structure of the restored share file*/
typedef struct {
    long fileSize;
    int numOfShares;
} shareFileHead_t;

/*the entry structure of the restored share file*/
typedef struct {
    int secretID;
    int secretSize;
    int shareSize;
} shareEntry_t;

/*the share container cache node structure*/
typedef struct {
    char shareContainerName[INTERNAL_FILE_NAME_SIZE];
    unsigned char shareContainer[CONTAINER_BUFFER_SIZE];
} shareContainerCacheNode_t;

#endif
