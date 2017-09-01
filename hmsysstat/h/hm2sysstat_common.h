#ifndef HM2SYSSTATCOMMON_H
#define HM2SYSSTATCOMMON_H

#include <pthread.h>

pthread_barrier_t snmp_work_barrier;

/*
* Status of the work module
* If workStatus < 0, the work module has exited
*/
int workStatus;
pthread_mutex_t workStatus_mutex;

int getWorkStatus();
void setWorkStatus(int);

#endif