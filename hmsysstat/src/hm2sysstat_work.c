#include "../h/hm2sysstat_work.h"
#include "../h/hm2sysstat_common.h"

int mainSocket;
client *clients = NULL; /* Needs to be freed */
int clientCount;

static interfaceInfo *interfaces = NULL; /* Needs to be freed */
static int interfaceCount;
sig_atomic_t keepRunning;
pthread_mutex_t logMutex;

static void order(char *buffer, int size)
{
    int i;

    char placeHolder;
    for (i = 0; i < size / 2; i++)
    {
        placeHolder = buffer[i];
        buffer[i] = buffer[size - i -1];
        buffer[size - i - 1] = placeHolder;
    }
}

static void stop_server(int sig)
{
    keepRunning = SYSSTAT_OFFLINE;
    char message[128];
    sprintf(message, "SYSSTAT: Signal %d received", sig);
    SYSLOG_INFO(message);

    if (sig == SIGSEGV)
    {
        exit(0);
    }
}

void closeAllSockets()
{
    lockclient;

    client *c;
    client *toDel = NULL;

    for (c = clients; c != NULL; c = c->next)
    {
        if (toDel)
            free(toDel);
        shutdown(c->socket, SHUT_RDWR);
        close(c->socket);

        toDel = c;
    }
    free(toDel);

    unlockclient;
}

void retrieveInterfaceInfo()
{
    char *intfDirName = "/sys/class/net";
    char *indexFileName = "ifindex";
    DIR *intfDir = opendir(intfDirName);
    struct dirent *interfaceContent;

    while ((interfaceContent = readdir(intfDir)) != NULL)
    {
        if (strcmp(interfaceContent->d_name, ".") == 0 || strcmp(interfaceContent->d_name, "..") == 0)
            continue;
        interfaceCount++;
    }

    interfaces = (interfaceInfo*)malloc(interfaceCount * sizeof(interfaceInfo));

    interfaceCount = 0;

    rewinddir(intfDir);
    while ((interfaceContent = readdir(intfDir)) != NULL)
    {
        char *intfName = interfaceContent->d_name;

        if (strcmp(intfName, ".") == 0 || strcmp(intfName, "..") == 0)
            continue;

        char *indexFileFullName;
        indexFileFullName = (char*)malloc(strlen(intfDirName) + strlen(intfName) + strlen(indexFileName) + 3);
        sprintf(indexFileFullName, "%s/%s/%s%c", intfDirName, intfName, indexFileName, '\0');

        FILE *interfaceIndexFile = fopen(indexFileFullName, "r");
        int interfaceIndex;
        fscanf(interfaceIndexFile, "%d", &interfaceIndex);
        fclose(interfaceIndexFile);

        free(indexFileFullName);

        char *interfaceDirName = (char*)malloc(strlen(intfDirName) + strlen(intfName) + 2);
        sprintf(interfaceDirName, "%s/%s%c", intfDirName, intfName, '\0');

        interfaces[interfaceCount].path = interfaceDirName;
        interfaces[interfaceCount].index = interfaceIndex;

        interfaceCount++;
    }
}

int getClientCount()
{
    int count;
    lockclient;
        count = clientCount;
    unlockclient;

    return count;
}

int getInfodAdminStatus()
{
    pthread_mutex_lock(&status_mutex);
        int status = infodAdminStatus;
    pthread_mutex_unlock(&status_mutex);

    return status;
}

int getInfodNoConnections()
{
    pthread_mutex_lock(&noConnections_mutex);
        int noConnections = infodNoConnections;
    pthread_mutex_unlock(&noConnections_mutex);

    return noConnections;
}

int setInfodAdminStatus(int status)
{
    if (status != SERVER_UP && status != SERVER_DOWN)
        return -1;

    pthread_mutex_lock(&status_mutex);
        infodAdminStatus = status;
    pthread_mutex_unlock(&status_mutex);
    
    return 0;
}

void setInfodNoConnections(int noConnections)
{
    pthread_mutex_lock(&noConnections_mutex);
        infodNoConnections = noConnections;
    pthread_mutex_unlock(&noConnections_mutex);
}

void setCpuInfoStatus(int status)
{
    lockCpuInfo;
        cpuInfoStatus = status;
    unlockCpuInfo;
}

int getCpuInfoStatus()
{
    int status;

    lockCpuInfo;
        status = cpuInfoStatus;
    unlockCpuInfo;

    return status;
}

int readArgs(int clientSocket, char* buffer, int requestedSize)
{
    int len;

    int bytes = read(clientSocket, &len, sizeof(int));
    order((char*)&len, sizeof(len));
    if (bytes < sizeof(int))
    {
        return -2;
    }
    
    /* If buffer is NULL, the caller doesn't care about the arguments, just flush them */
    if (buffer == NULL)
    {
        char flusher[len];
        bytes = read(clientSocket, flusher, len);
    } else {
        bytes = read(clientSocket, buffer, requestedSize);
    }

    return bytes;
}

void setCpuUsage(float usage)
{
    lockCpuUsage;
        cpuUsage = usage;
    unlockCpuUsage;
}

float getCpuUsage()
{
    float usage;

    lockCpuUsage;
        usage = cpuUsage;
    unlockCpuUsage;

    return usage;
}

int getMemValueFromStr(char *memStr)
{
    int memValue;

    sscanf(memStr, "%*s %d", &memValue);

    return memValue;
}

int sendMemInfo(int clientSocket)
{
    size_t len = 1024;

    char totalMemStr[len];
    char freeMemStr[len];

    int totalMem;
    int freeMem;
    int i;

    readArgs(clientSocket, NULL, 0);

    FILE *f = fopen("/proc/meminfo","r");

    if (!f)
    {
        SYSLOG_ERR("SYSSTAT: Error openning /proc/meminfo");

        return SYSSTAT_ERR;
    }

    fgets(totalMemStr, len, f);
    fgets(freeMemStr, len, f);

    fclose(f);

    totalMem = getMemValueFromStr(totalMemStr);
    freeMem = getMemValueFromStr(freeMemStr);

    int buffer[MEMINFOPACKETSIZE];

    buffer[0] = MEMINFO;
    buffer[1] = 0;
    buffer[2] = 2 * sizeof(int);
    buffer[3] = totalMem;
    buffer[4] = freeMem;

    for (i = 0; i < MEMINFOPACKETSIZE; i++)
        order((char*)&(buffer[i]), sizeof(int));

    if (send(clientSocket, (char*)buffer, MEMINFOPACKETSIZE, 0) == MEMINFOPACKETSIZE) {
        return SYSSTAT_OK;
    } else {
        return SYSSTAT_ERR;
    }
}

int sendCpuInfo(int clientSocket)
{
    int id = CPUINFO;
    int err = 0;
    int len = sizeof(float);
    float usage = getCpuUsage();
    int ret;

    char *buffer = (char*)malloc(CPUINFOPACKETSIZE);
    char *advancer = buffer;

    readArgs(clientSocket, NULL, 0);

    order((char*)&id, sizeof(id));
    order((char*)&err, sizeof(err));
    order((char*)&len, sizeof(len));
    order((char*)&usage, sizeof(usage));

    memcpy(advancer, &id, sizeof(id)); advancer += sizeof(id);
    memcpy(advancer, &err, sizeof(err)); advancer += sizeof(err);
    memcpy(advancer, &len, sizeof(len)); advancer += sizeof(len);
    memcpy(advancer, &usage, sizeof(usage));

    if (write(clientSocket, (void*)buffer, CPUINFOPACKETSIZE) == CPUINFOPACKETSIZE) {
        ret = SYSSTAT_OK;
    } else {
        ret = SYSSTAT_ERR;
    }

    free(buffer);

    return ret;
}

int sendInterfaceStatus(int clientSocket)
{
    char *buffer = (char*)malloc(IFSTATUSPACKETSIZE);
    int ifIndex;
    int bytes;
    int i;
    int ret;
    char adminStatus;
    char linkStatus;
    FILE *info;

    bytes = readArgs(clientSocket, buffer, sizeof(int));

    if (bytes != sizeof(int))
        return SYSSTAT_ERR;

    order(buffer, sizeof(int));
    ifIndex = *(int*)buffer;

    for (i = 0; i < interfaceCount; i++)
    {
        if (ifIndex == interfaces[i].index)
            break;
    }

    if (i == interfaceCount)
    {
        int status = IFSTATUS;
        int error = NOSUCHINTF_ERRORCODE;

        memcpy((void*)buffer, &status, sizeof(status));
        memcpy((void*)(buffer + sizeof(status)), &error, sizeof(error));
        write(clientSocket, buffer, 2 * sizeof(int));

        free(buffer);

        return SYSSTAT_ERR;
    }

    char adminStatusFileName[strlen(interfaces[i].path) + strlen("operstate") + 1];
    sprintf(adminStatusFileName, "%s/%s", interfaces[i].path, "operstate");
    info = fopen(adminStatusFileName, "r");
    fscanf(info, "%s", buffer);
    if (strcmp(buffer, "down") == 0)
        adminStatus = 0;
    else
        adminStatus = 1;
    fclose(info);

    char linkStatusFileName[strlen(interfaces[i].path) + strlen("carrier") + 1];
    sprintf(linkStatusFileName, "%s/%s", interfaces[i].path, "carrier");
    info = fopen(linkStatusFileName, "r");
    fscanf(info, "%d", (int*)buffer);
    if (*(int*)buffer == 0)
        linkStatus = 0;
    else
        linkStatus = 1;

    *(int*)buffer = IFSTATUS;
    *(int*)(buffer + 4) = 0;
    *(int*)(buffer + 8) = 2;
    buffer[12] = adminStatus;
    buffer[13] = linkStatus;

    order(buffer, sizeof(int));
    order(buffer + 4, sizeof(int));
    order(buffer + 8, sizeof(int));

    if (write(clientSocket, buffer, IFSTATUSPACKETSIZE) == IFSTATUSPACKETSIZE)
        ret = SYSSTAT_OK;
    else
        ret = SYSSTAT_ERR;

    free(buffer);

    return ret;
}

int sendNoSuchId(int clientSocket, int id)
{
    int buffer[NOSUCHIDPACKETSIZE];

    buffer[0] = id;
    buffer[1] = NOSUCHID_ERRORCODE;

    if (write(clientSocket, (void*)buffer, NOSUCHIDPACKETSIZE) == NOSUCHIDPACKETSIZE) {
        return SYSSTAT_OK;
    } else {
        return SYSSTAT_ERR;
    }
}

void getCpuInfoThread()
{
    sigset_t usigs;

    sigemptyset(&usigs);
    sigaddset(&usigs, SIGTERM);
    sigaddset(&usigs, SIGINT);
    sigaddset(&usigs, SIGABRT);
    pthread_sigmask(SIG_UNBLOCK, &usigs, NULL);

    signal(SIGTERM, stop_server);
    signal(SIGINT, stop_server);
    signal(SIGABRT, stop_server);

    long long user, nice, sys, idle;
    long long oldUser, oldNice, oldSys, oldIdle;

    float usage;

    size_t len = 1024;
    char line[len];

    FILE *f = fopen("/proc/stat", "r");
    if (!f)
    {
        SYSLOG_ERR("SYSSTAT: Can't open /proc/stat. This is needed for the CPU info retrieval.");

        setCpuInfoStatus(SYSSTAT_ERR);
        pthread_barrier_wait(&cpuInfoBarrier);
        
        return;
    }

    fgets(line, len, f);

    fclose(f);

    sscanf(line, "%*s %lld %lld %lld %lld", &oldUser, &oldNice, &oldSys, &oldIdle);

    sleep(1);

    pthread_barrier_wait(&cpuInfoBarrier);

    SYSLOG_INFO("SYSSTAT: CPUINFO thread running");

    while (getWorkStatus() != SYSSTAT_OFFLINE)
    {
        f = fopen("/proc/stat", "r");
        if (!f)
        {
            SYSLOG_ERR("SYSSTAT: Can't open /proc/stat. This is needed for the CPU info retrieval.");

            setCpuInfoStatus(SYSSTAT_ERR);
            
            return;
        }

        fgets(line, len, f);
        fclose(f);

        sscanf(line, "%*s %lld %lld %lld %lld", &user, &nice, &sys, &idle);

        usage = (float)(( user + nice + sys ) - ( oldUser + oldNice + oldSys )) / ( ( user + nice + sys + idle ) - ( oldUser + oldNice + oldSys + oldIdle ) );
        setCpuUsage(usage);

        oldUser = user;
        oldNice = nice;
        oldSys = sys;
        oldIdle = idle;

        sleep(1);
    }

    SYSLOG_INFO("SYSSTAT: CPUINFO thread exited");
}

void addClient(int clientSocket, struct sockaddr_in clientAddr)
{
    lockclient;
    if (clients == NULL)
        {
            client *newClient = (client*)malloc(sizeof(client));

            newClient->socket = clientSocket;
            newClient->addr = clientAddr;
            newClient->next = NULL;
            newClient->prev = NULL;
            
            clients = newClient;
            clientCount++;
        }
    else
        {
            client *newClient = (client*)malloc(sizeof(client));

            newClient->socket = clientSocket;
            newClient->addr = clientAddr;
            newClient->next = NULL;

            client *clientIt = clients;

            int duplicateFound = 0;
            do
            {
                if (strcmp(inet_ntoa(newClient->addr.sin_addr), inet_ntoa(clientIt->addr.sin_addr)) == 0)
                {
                    duplicateFound = 1;
                    break;
                }
                clientIt = clientIt->next;
            }
            while (clientIt->next != NULL);

            if (duplicateFound)
            {
                shutdown(newClient->socket, SHUT_RDWR);
                free(newClient);
                SYSLOG_INFO("SYSSTAT: Client rejected: IP already connected");
                unlockclient;
                return;
            }

            clientIt->next = newClient;
            newClient->prev = clientIt;
            clientCount++;
        }
    unlockclient;

    SYSLOG_INFO("SYSSTAT: Client connected");

    setInfodNoConnections(clientCount);
}

void deleteClient(int sock)
{
    lockclient;
    client *c = clients;
    if (clients == NULL)
    {
        unlockclient;
        return;
    }

    while (c)
    {
        if (c->socket == sock)
        {
            shutdown(sock, SHUT_RDWR);
            close(sock);

            if (c->next)
                c->next->prev = c->prev;
            if (c->prev)
                c->prev->next = c->next;

            clientCount--;
            if (!clientCount)
                clients = NULL;

            break;
        }
        c = c->next;
    }
    unlockclient;

    setInfodNoConnections(clientCount);
}

int* getClientsSockets()
{
    lockclient;
    
    if (clients == NULL)
    {
        unlockclient;

        return NULL;
    }

    int *clsockets = (int*)malloc(clientCount * sizeof(int));
    client *c = clients;
    int i;

    for (i = 0; c != NULL; i++, c = c->next)
        clsockets[i] = c->socket;
    
    unlockclient;

    return clsockets; /* Needs to be freed */
}

void clientConnectionThread()
{
    struct sockaddr_in clientAddr;
    int clientSocket;
    unsigned int clientAddrLen;
    int status;

    clientAddrLen = sizeof(clientAddr);

    clientCount = 0;
    setInfodNoConnections(0);

    mainSocket = socket(AF_INET, SOCK_STREAM, 0);
    {
        if (mainSocket < 0)
        {
            SYSLOG_ERR("SYSSTAT: Failed to initialize mainSocket in work module");

            acceptStatus = -1;
            pthread_barrier_wait(&startBarrier);
            return;
        }
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(WORKPORT);

    if (bind (mainSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
    {
        SYSLOG_ERR("SYSSTAT: Failed to bind mainSocket in work module");
        printf("Errno: %d\n", errno);

        acceptStatus = -1;
        pthread_barrier_wait(&startBarrier);
        return;
    }

    listen(mainSocket, CLIENT_BACKLOG_SIZE);

    pthread_barrier_wait(&startBarrier);

    char logInfo[128];

    SYSLOG_INFO("SYSSTAT: New clients thread running");
    int pause = 0;
    while(getWorkStatus() == SYSSTAT_ONLINE)
    {
        if (getWorkStatus() == SYSSTAT_OFFLINE)
            break;

        if (pause)
        {
            sleep(1);
            pause = 0;
        }

        status = getInfodAdminStatus();
        if (status == SERVER_DOWN)
        {
            pause = 1;
            continue;
        }

        if (getInfodNoConnections() == MAX_CLIENTS)
            continue;

        memset(&clientAddr, 0, sizeof(clientAddr));

        SYSLOG_INFO("SYSSTAT: Waiting for client...");
        clientSocket = accept(mainSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket >= 0)
        {
            sprintf(logInfo, "SYSSTAT: Inbound connection from %s", inet_ntoa(clientAddr.sin_addr));
            SYSLOG_INFO(logInfo);

            addClient(clientSocket, clientAddr);
        }
        else 
            continue;
    }

    SYSLOG_INFO("SYSSTAT: New clients thread exited");
}

/*
* Main thread for work module. Also manages the other two threads.
*/
void workThread()
{
    int status;
    int i;
    sigset_t usigs;
    struct timeval selTimeout;

    sigemptyset(&usigs);
    sigaddset(&usigs, SIGTERM);
    sigaddset(&usigs, SIGINT);
    sigaddset(&usigs, SIGABRT);
    sigaddset(&usigs, SIGSEGV);
    sigaddset(&usigs, SIGPIPE);
    pthread_sigmask(SIG_UNBLOCK, &usigs, NULL);

    signal(SIGTERM, stop_server);
    signal(SIGINT, stop_server);
    signal(SIGABRT, stop_server);
    signal(SIGSEGV, stop_server);
    signal(SIGPIPE, SIG_IGN);

    infodAdminStatus = SERVER_DOWN;
    infodNoConnections = 0;
    setWorkStatus(SYSSTAT_ONLINE);
    clients = NULL;

    acceptStatus = SYSSTAT_OK;
    cpuInfoStatus = SYSSTAT_OK;

    pthread_mutex_init(&status_mutex, NULL);
    pthread_mutex_init(&noConnections_mutex, NULL);
    pthread_mutex_init(&clientsMutex, NULL);
    pthread_mutex_init(&logMutex, NULL);
    pthread_barrier_init(&startBarrier, NULL, 2);
    pthread_barrier_init(&cpuInfoBarrier, NULL, 2);

    retrieveInterfaceInfo();

    pthread_barrier_wait(&snmp_work_barrier);

    SYSLOG_INFO("SYSSTAT: Starting new clients thread");
    pthread_create(&clientsThread, NULL, (void*(*)(void*))clientConnectionThread, NULL);

    SYSLOG_INFO("SYSSTAT: Starting CPUINFO thread");
    pthread_create(&cpuInfoThread, NULL, (void*(*)(void*))getCpuInfoThread, NULL);

    pthread_barrier_wait(&startBarrier);

    pthread_barrier_wait(&cpuInfoBarrier);

    if (acceptStatus != SYSSTAT_OK)
    {
        SYSLOG_INFO("SYSSTAT: New clients thread failed. Exiting");
        notifyWorkExit();
    }

    setInfodAdminStatus(SERVER_UP); /* Letting everyone know we are alive */

    if (getWorkStatus() == SYSSTAT_ONLINE)
        SYSLOG_INFO("SYSSTAT: SYSSTAT is alive");

    keepRunning = SYSSTAT_ONLINE;
    int again = 1;

    /* --------------------------------------------------- Main loop ---------------------------------------------------------- */

    while(keepRunning == SYSSTAT_ONLINE)
    {
        if (!again)
            sleep(1);
        again = 1;
        
        if (getCpuInfoStatus() != SYSSTAT_OK)
        {
            notifyWorkExit();

            break;
        }

        status = getInfodAdminStatus();
        if (status == SERVER_DOWN)
        {
            again = 0;
            continue;
        }

        int *clientSockets = getClientsSockets();

        fd_set clSockets;
        FD_ZERO(&clSockets);

        int maxSocket = 0;
        int clCount = getClientCount();

        for (i = 0; i < clCount; i++)
        {
            FD_SET(clientSockets[i], &clSockets);
            if (clientSockets[i] > maxSocket)
                maxSocket = clientSockets[i];
        }
        maxSocket++;

        selTimeout.tv_usec = 100000;
        if (select(maxSocket, &clSockets, NULL, NULL, &selTimeout) <= 0)
            continue;

        for (i = 0; i < clCount; i++)
        {
            if (FD_ISSET(clientSockets[i], &clSockets))
            {
                int op;

                int bytes = recv(clientSockets[i], (char*)&op, sizeof(op), 0);
                order((char*)&op, sizeof(op));

                if (bytes < (int)sizeof(op))
                {
                    if (bytes == 0) /* Client has closed the socket. */
                    {
                        SYSLOG_INFO("SYSSTAT: Client closed the connection");
                        deleteClient(clientSockets[i]);
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EINTR)
                            continue;

                        if (errno == ENOTSOCK || errno == ENOTCONN || errno == ECONNREFUSED)
                        {
                            deleteClient(clientSockets[i]);
                            continue;
                        }

                        char log[128];
                        sprintf(log, "SYSSTAT: Invalid data from client. Errno: %d", errno);
                        SYSLOG_INFO(log);
                    }
                }
                else
                {
                    switch(op)
                    {
                        case (MEMINFO):
                            SYSLOG_INFO("SYSSTAT: Received MEMINFO request from client");
                            sendMemInfo(clientSockets[i]);
                            break;
                        case (CPUINFO):
                            SYSLOG_INFO("SYSSTAT: Received CPUINFO request from client");
                            sendCpuInfo(clientSockets[i]);
                            break;
                        case (IFSTATUS):
                            SYSLOG_INFO("SYSSTAT: Received IFSTATUS request from client");
                            sendInterfaceStatus(clientSockets[i]);
                            break;
                        default:
                            sendNoSuchId(clientSockets[i], op);
                    }
                }
            }
        }
        free(clientSockets);
    }

    notifyWorkExit();

    SYSLOG_INFO("SYSSTAT: Main thread exiting...");

    closeAllSockets();

    for (i = 0; i < interfaceCount; i++)
    {
        free(interfaces[i].path);
        free(interfaces);
    }

    notifyWorkExit();

    SYSLOG_INFO("SYSSTAT: Waiting for the other threads to exit...");
    shutdown(mainSocket, SHUT_RDWR);
    pthread_join(clientsThread, NULL);
    pthread_join(cpuInfoThread, NULL);
}