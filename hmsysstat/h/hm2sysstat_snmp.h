#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <hm_esm.h>

#define AGENT_NAME "HMCTL_SYSSTAT"

void agentX();