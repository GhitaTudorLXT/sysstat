#include "../h/hm2sysstat_common.h"
#include "../h/hm2sysstat_work.h"

void notifyWorkExit()
{
    pthread_mutex_lock(&workStatus_mutex);
        workStatus = SYSSTAT_OFFLINE;
    pthread_mutex_unlock(&workStatus_mutex);
}

int getWorkStatus()
{
    pthread_mutex_lock(&workStatus_mutex);
        int status = workStatus;
    pthread_mutex_unlock(&workStatus_mutex);

    return status;
}

void setWorkStatus(int status)
{
    pthread_mutex_lock(&workStatus_mutex);
        workStatus = status;
    pthread_mutex_unlock(&workStatus_mutex);
}