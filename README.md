# Metadedup: Deduplicating Metadata in Encrypted Deduplication via Indirection

## Introduction

Encrypted deduplication combines encryption and deduplication in a seamless way to provide confidentiality guarantees for the physical data in deduplication storage, yet it incurs substantial metadata storage overhead due to the additional storage of keys. We present a new encrypted deduplication storage system called Metadedup, which suppresses metadata storage by also applying deduplication to metadata. Its idea builds on indirection, which adds another level of metadata chunks that record metadata information. We find that metadata chunks are highly redundant in real-world workloads and hence can be effectively deduplicated. In addition, metadata chunks can be protected under the same encrypted deduplication framework, thereby providing confidentiality guarantees for metadata as well. 

### Publication

*  Jingwei Li, Patrick P. C. Lee, Yanjing Ren, and Xiaosong Zhang. **Metadedup: Deduplicating Metadata in Encrypted Deduplication via Indirection**. Proceedings of the 35th International Conference on Massive Storage Systems and Technology (MSST 2019), Santa Clara, U.S.A, May 2019.

## Dependencies

Metadedup is built on Ubuntu 14.04.5 LTS with GNU gcc version 6.5.0. It requires the following libraries:

* OpenSSL (version: 1.0.2g)
* Boost C++ library 
* GF-Complete 
* Leveldb 

To install OpenSSL, Boost C++ library, GF-complete and snappy (that is necessary for Leveldb), type the following command:
```shell
$ sudo apt-get install libboost-all-dev libsnappy-dev libssl-dev libgf-complete-dev 
```
Leveldb is packed in `server/lib/`, and compile it using the following command:  
```shell
$ make -C server/lib/leveldb/
```

## Configurations

Metadedup distributes data across n (4 by default) servers for storage, such that the original data can be retrieved provided that any k (3 by default) out of the n servers are available. 

### Manual Configuration

#### Server

Metadedup server processes data and metadata, separately. Compile the server program via the following command: 
```shell
$ make -C server/
```

Start a Metadedup server by the following command. Here `meta port` and `data port` indicate the ports that are listened for data and metadata processing, respectively.  
```shell
$ server/SERVER [meta port] [data port]
```

Then, follow the above instructions, and start another n-1 servers.

#### Client

Edit the configuration file `client/config-u` to set server information for upload. Note that the configuration should be consistent with the IPs and ports of the n running servers. 

An example of `client/config-u` is shown as follows:
```
0.0.0.0:11030 
0.0.0.0:11031
0.0.0.0:11032 
0.0.0.0:11033
0.0.0.0:11034
0.0.0.0:11035
0.0.0.0:11036
0.0.0.0:11037
```
* Line 1-4 specify the IP addresses of 4 running servers, as well as corresponding meta ports.
* Line 5-8 specify the IP addresses of 4 running servers, as well as corresponding data ports. 
* In this example, Line 1 and Line 5, Line 2 and Line 6, Line 3 and Line 7, and Line 4 and Line 8 correspond to the same server yet different ports for data and metadata processing, respectively.  

In addition, edit the configuration file `client/config-d` based on the same instruction above, in order to set server information for download. Note that Metadedup only needs to download data from any k out of n servers for retrieval. Thus, `client/config-d` includes the configuration for k necessary servers: the first k lines indicate the IP addresses and meta ports, and the last k lines indicate the IP addresses and data ports.   

Then, compile and generate an executable program for client.
```
$ make -C client/
```
Note that any changes of `client/config-u` or `client/config-d` do not require to re-compile client program. 

#### Changing (n, k)

You can modify the variables `n_` and `m_` in lines 47-50 in `client/utils/conf.hh`, in order to change the parameter setting (n, k) of Metadedup. 
```c
// default (n, k) configuration of Metadedup 
n_ = 4;
m_ = 1;
k_ = n_ - m_;
r_ = k_ - 1;
```
* `n_` is the total number of servers.
* `m_` is the fault tolerance degree.
* `k_` is the smallest number of servers required for successful retrieval.
* `r_` is the confidentiality degree.

Note that the change of (n, k) parameter setting requires to re-compile server and client programs. 

### Auto Configuration

We provide a shell script to automatically config Metadedup in default setting (i.e., n = 4 and k = 3): 
```shell
$ auto/auto_config.sh
```
The shell script `auto/auto_config.sh` makes the following actions: (i) compile client; (ii) compile server; and (iii) replicate the compiled server folder to another three different folders (`server2/`,`server3/` and `server4/`), in order to config multiple servers. After that, each server program can be run by following the SERVER command.     

We also provide a shell script to clean the configuration.
```shell
$ auto/auto_clean.sh
```

## Usage

Use the executable program `client/CLIENT` in the following way:

```shell
usage: CLIENT [filename] [userID] [action] [secutiyType]

- [filename]: full path of the file;
- [userID]: user ID of current client;
- [action]: [-u] upload; [-d] download;
- [securityType]: [HIGH] AES-256 & SHA-256; [LOW] AES-128 & SHA-1
```

As an example, to upload a file `test` from user 0 using high security mechanism (e.g., AES-256 & SHA-256), type the following command:

```shell
$ client/CLIENT test 0 -u HIGH
```

To download further the file `test`, follow the command:
```shell
$ client/CLIENT test 0 -d HIGH
```

## Compatiability

### New Versions of OpenSSL 

We develop Metadedup in Ubuntu 14.04 that can only be compatiable with an old version (e.g., 1.0.2) of OpenSSL. However, such old version is not supported by  recent Ubuntu systems. We provide a patch (that is tested on Ubuntu 18.04) to ensure that Metadedup can work with a new version of OpenSSL. 

Before patching, check the version of OpenSSL installed in your system:
```shell
$ openssl version
```

You will get OpenSSL version like follows: 
```shell
OpenSSL 1.1.0g 2 Nov 2017
```
If the version number is lower than 1.1.0 (e.g., version 1.0.2/1.0.1), you do not need to put the patch.

If the version number is higher than 1.1.0, uncomment line 20 of `client/utils/CryptoPrimitive.hh`:    
```c
#define OPENSSL_VERSION_1_1 1
```
You also need to uncomment line 20 of `server/utils/CryptoPrimitive.hh`.


### GF-Complete Installation from Source 

Some Linux systems do not support to install GF-Complete from package managers. We provide instructions to install GF-Complete from source code. 

Download the source code of GF-Complete from [here](http://lab.jerasure.org/jerasure/gf-complete/tree/master), go into GF-Complete folder, and follow the commands for installation:  
```shell
$ ./configure
$ make
$ sudo make install
```

## Limitation

We assume that the upload and download channels are secure (e.g., encrypted and authenticated), and do not implement  SSL/TLS mechanisms upon our protection (e.g., CAONT-RS) of data. 

## Maintainer

* Yanjing Ren, UESTC, tinoryj@gmail.com
