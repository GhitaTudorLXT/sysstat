#ifndef HM2SYSSTATWORK_H
#define HM2SYSSTATWORK_H

#include <pthread.h>
#include <syslog.h>
#include <atomic.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <stdio.h>

#define SYSLOG_ERR(msg_format, args...) syslog(LOG_ERR, msg_format, ##args )
#define SYSLOG_INFO(msg_format, args...) syslog(LOG_INFO, msg_format, ##args )

#define lockclient pthread_mutex_lock(&clientsMutex)
#define unlockclient pthread_mutex_unlock(&clientsMutex)
#define lockCpuInfo pthread_mutex_lock(&cpuInfoMutex)
#define unlockCpuInfo pthread_mutex_unlock(&cpuInfoMutex)
#define lockCpuUsage pthread_mutex_lock(&cpuUsageMutex)
#define unlockCpuUsage pthread_mutex_unlock(&cpuUsageMutex)

#define MEMINFO 1
#define CPUINFO 2
#define IFSTATUS 3
#define WORKPORT 40000
#define CLIENT_BACKLOG_SIZE 10
#define SERVER_UP 1
#define SERVER_DOWN 0
#define SYSSTAT_ERR -1
#define SYSSTAT_OK 0
#define SYSSTAT_ONLINE 0
#define SYSSTAT_OFFLINE -1
#define MEMINFOPACKETSIZE (5 * sizeof(int))
#define CPUINFOPACKETSIZE (3 * sizeof(int) + sizeof(float))
#define IFSTATUSPACKETSIZE (3 * sizeof(int) + 2 * sizeof(char))
#define NOSUCHIDPACKETSIZE (2 * sizeof(int))
#define NOSUCHID_ERRORCODE 100
#define NOSUCHINTF_ERRORCODE 101
#define MAX_CLIENTS 100

typedef struct client client;

typedef struct
{
    char *path;
    int index;
} interfaceInfo;

struct client
{
    int socket;
    struct sockaddr_in addr;

    client *next;
    client *prev;
};

pthread_mutex_t status_mutex;
pthread_mutex_t noConnections_mutex;
pthread_mutex_t clientsMutex;
pthread_mutex_t cpuInfoMutex;
pthread_mutex_t cpuUsageMutex;
pthread_barrier_t startBarrier;
pthread_barrier_t cpuInfoBarrier;
pthread_t clientsThread;
pthread_t cpuInfoThread;

int infodAdminStatus;
int infodNoConnections;

int acceptStatus;
int cpuInfoStatus;

float cpuUsage;

/* Functions used also by the SNMP handler
* ------------------------------------ */
int getInfodAdminStatus();
int getInfodNoConnections();
int setInfodAdminStatus(int);
/* ------------------------------------ */

void setInfodNoConnections(int);
int getWorkStatus();
void notifyWorkExit();

void clientConnectionThread();
void workThread();

#endif
