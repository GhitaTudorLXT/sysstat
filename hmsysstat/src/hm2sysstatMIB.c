/*
 * Note: this file originally auto-generated by mib2c using
 *        $
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "../h/hm2sysstatMIB.h"
#include "../h/hm2sysstat_work.h"

/** Initializes the hm2sysstatMIB module */
void
init_hm2sysstatMIB(void)
{
    const oid hm2AgentInfodAdminStatus_oid[] = { 1,3,6,1,4,1,248,12,20,1,1 };
    const oid hm2AgentInfodNoConnections_oid[] = { 1,3,6,1,4,1,248,12,20,1,2 };

  DEBUGMSGTL(("hm2sysstatMIB", "Initializing\n"));

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("hm2AgentInfodAdminStatus", handle_hm2AgentInfodAdminStatus,
                               hm2AgentInfodAdminStatus_oid, OID_LENGTH(hm2AgentInfodAdminStatus_oid),
                               HANDLER_CAN_RWRITE
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("hm2AgentInfodNoConnections", handle_hm2AgentInfodNoConnections,
                               hm2AgentInfodNoConnections_oid, OID_LENGTH(hm2AgentInfodNoConnections_oid),
                               HANDLER_CAN_RONLY
        ));
}

int
handle_hm2AgentInfodAdminStatus(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    int ret;
    int infodAdminStatus;
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    
    switch(reqinfo->mode) {

        case MODE_GET:

            infodAdminStatus = getInfodAdminStatus();
            snmp_set_var_typed_value(requests->requestvb, ASN_INTEGER,
                                     &infodAdminStatus,
                                     sizeof(infodAdminStatus));
            break;

        /*
         * SET REQUEST
         *
         * multiple states in the transaction.  See:
         * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
         */
        case MODE_SET_RESERVE1:
                /* or you could use netsnmp_check_vb_type_and_size instead */
            ret = netsnmp_check_vb_type(requests->requestvb, ASN_INTEGER);
            if ( ret != SNMP_ERR_NOERROR ) {
                netsnmp_set_request_error(reqinfo, requests, ret );
            }
            break;

        case MODE_SET_RESERVE2:
            /* XXX malloc "undo" storage buffer */
            if (0) {
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_RESOURCEUNAVAILABLE);
            }
            break;

        case MODE_SET_FREE:
            /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
            break;

        case MODE_SET_ACTION:
            
            memcpy(&infodAdminStatus, &(*(requests->requestvb->val.integer)), requests->requestvb->val_len);

            ret = setInfodAdminStatus(infodAdminStatus);

            if (ret != 0) {
                SYSLOG_ERR("Illegal value in snmpset of scalar infodAdminStatus --- Valid values: 0 or 1");
                netsnmp_set_request_error(reqinfo, requests, ret);
            }
            break;

        case MODE_SET_COMMIT:
            /* XXX: delete temporary storage */
            if (0) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_COMMITFAILED);
            }
            break;

        case MODE_SET_UNDO:
            /* XXX: UNDO and return to previous value for the object */
            if (0) {
                /* try _really_really_ hard to never get to this point */
                netsnmp_set_request_error(reqinfo, requests, SNMP_ERR_UNDOFAILED);
            }
            break;

        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_hm2AgentInfodAdminStatus\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}
int
handle_hm2AgentInfodNoConnections(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    int infodAdminNoConnections;
    
    switch(reqinfo->mode) {

        case MODE_GET:

            infodAdminNoConnections = getInfodNoConnections();
            snmp_set_var_typed_value(requests->requestvb, ASN_UNSIGNED,
                                     &infodAdminNoConnections,
                                     sizeof(infodAdminNoConnections));
            break;


        default:
            /* we should never get here, so this is a really bad error */
            snmp_log(LOG_ERR, "unknown mode (%d) in handle_hm2AgentInfodNoConnections\n", reqinfo->mode );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}