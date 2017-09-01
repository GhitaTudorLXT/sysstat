/*
* The SNMP Agent is implemented in this file
*/

#include "../h/hm2sysstat_snmp.h"
#include "../h/hm2sysstatMIB.h"
#include "../h/hm2sysstat_common.h"

static int keep_running;

static void stop_server(int sig)
{
    keep_running = 0;
}

void agentX() 
{
    sigset_t usigs;

    snmp_enable_stderrlog();

    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);

    SOCK_STARTUP;

    init_agent(AGENT_NAME);

    init_hm2sysstatMIB();

    init_snmp(AGENT_NAME);

    sigemptyset(&usigs);
    sigaddset(&usigs, SIGTERM);
    sigaddset(&usigs, SIGINT);
    sigaddset(&usigs, SIGABRT);
    pthread_sigmask(SIG_UNBLOCK, &usigs, NULL);

    signal(SIGTERM, stop_server);
    signal(SIGINT, stop_server);
    signal(SIGABRT, stop_server);

    char logInfo[64];
    sprintf(logInfo, "%s is up and running\n", AGENT_NAME);
    printf("SNMP TEST\n");
    snmp_log(LOG_INFO, logInfo);

    hm_esm_unblock();

    pthread_barrier_wait(&snmp_work_barrier);

    keep_running = 1;

    while (keep_running)
    {
        agent_check_and_process(1);
    }
}