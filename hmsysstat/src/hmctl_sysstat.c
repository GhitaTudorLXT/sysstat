/***************************** FILE HEADER ***********************************/
/*!
*  \file		hmctl_tmpl.c
*
*  \brief		<b>Component control application template skeleton</b>\n
*
*  				A trimmed down version of hmctl_tmpl.c (stub functions for
*  				starting work-module, monitoring ...).
*
*
*  \author		Catalin Gheorghe\n
*				Copyright 2011 Hirschmann Automation & Control GmbH
*
*  \version	1a	2011-08-25 Catalin Gheorghe    created 
*
*//**************************** FILE HEADER **********************************/

/******************************************************************************
* Includes
******************************************************************************/

#include "../h/hmctl_sysstat.h"
#include "../h/hm2sysstat_snmp.h"
#include "../h/hm2sysstat_work.h"
#include "../h/hm2sysstat_common.h"

/******************************************************************************
* Defines
******************************************************************************/

/******************************************************************************
* Local variables
******************************************************************************/

/******************************************************************************
* Global variables
******************************************************************************/

pthread_t snmp_thread;
pthread_t work_thread;

/**
 * \var			progname
 *
 * Program name.
 */
const char *progname;

/******************************************************************************
* Externals
******************************************************************************/

/******************************************************************************
* Local functions
******************************************************************************/

static int setup_logging();
static int init_com(const char *arg);
static int local_init_snmp();
static int generate_configuration();
static int start_work();

static const char *get_program_name(const char *name);

/******************************************************************************
* Global functions
******************************************************************************/

HM_STATUS hmctl_daemonize(int noclose)
{
	int fd, max_fds; 	/* system maximum open file descriptors */

	/* first fork - background process */
	switch ( fork() ) {
		case -1:
			/* fork error */
			SYSLOG_ERR("daemonizing failed (first fork)");
			return HM_ERROR;
		case 0:
			/* child process - continue */
			printf("First fork done");
			break;
		default:
			/* parent process - exit */
			exit(EXIT_SUCCESS);
	}

	/* become session leader */
	if ( setsid() == -1 ) {
		SYSLOG_ERR("daemoniziong failed (session leader)");
		return HM_ERROR;
	}

	/* second fork - no controlling terminal */
	switch ( fork() ) {
		case -1:
			/* fork error */
			SYSLOG_ERR("daemonizing failed (second fork)");
			return HM_ERROR;
		case 0:
			/* child process - continue */
			printf("Second fork done");
			break;
		default:
			/* parent process - exit */
			exit(EXIT_SUCCESS);
	}

	/* clear umask */
	umask(0);

	/* change current directory */
	chdir("/");

	if ( noclose == 0 ) {
		/* close all open file descriptors */
		max_fds = sysconf(_SC_OPEN_MAX);
		if ( max_fds == -1 ) {
			/* undeterminate limit, use a predefined number */
			SYSLOG_DEBUG("undeterminate limit for open fd's (using %d)", 100);
			max_fds = 100;
		}
		for ( fd = 0; fd < max_fds; fd++ )
			close(fd);

		/* stdin, stdout, stderr to /dev/null */
		fd = open("/dev/null", O_RDWR);

		if ( fd != STDIN_FILENO ) {
			SYSLOG_ERR("daemonizing failed (redirect stdin)");
			return HM_ERROR;
		}
		if ( dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO ) {
			SYSLOG_ERR("daemonizing failed (redirect stdout)");
			close(fd);
			return HM_ERROR;
		}
		if ( dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO ) {
			SYSLOG_ERR("daemonizing failed (redirect stderr)");
			close(fd);
			return HM_ERROR;
		}
	}

	return HM_OK;
}

/******************************************************************************
 *  \fn					main
 ******************************************************************************/
/*!
 *  \brief				<b>Entry point to the component-control template\n</b>
 *  
 *  \return             does not return - daemon-like
 *
 *  \author             Catalin Gheorghe
 *
 *  \date               2011-08-26
 ******************************************************************************/
int main(int argc, char *argv[])
{	
	int value;
	HM_ESM_STATE_t current_state;	/* state of this component */
	HM_ESM_STATE_t system_state;	/* state messsage received from ESM */

	/* endless loop for component-control simple state machine */
	current_state = HM_ESM_STATE_INIT;

	/*
		Initialize common components
	*/
	pthread_barrier_init(&snmp_work_barrier, NULL, 2);
	pthread_mutex_init(&workStatus_mutex, NULL);
	workStatus = 0;

/*
	printf("CONTROL MODULE STARTED\n");
	printf("PID: %d\n", getpid());

	if (pthread_create(&work_thread, NULL, (void*(*)(void*))workThread, NULL) != 0)
	{
		printf("Error starting thread\n");
		return 0;
	}

	printf("WORK MODULE STARTING...\n");
	pthread_join(work_thread, NULL);

	return 0;
*/
		
	for (;;) {
		switch ( current_state ) {
			/********************************************************************************
			 * STATE INIT - first state
			 * - setup logging
			 * - initialize any other data/devices required
			 * - store program name (needed for generating state events) 
			 * - initialize communication with the ESM
			 * - setup SNMP
			 *
			 *   NOTE: this state is entered only once, at system startup
			 */
			case HM_ESM_STATE_INIT:
				/* setup logging */
				if ( setup_logging() != HM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: setting up logging failed");
					current_state = HM_ESM_STATE_FAULT;
					break;
				}

				hmctl_daemonize(1);

				int res = write(1,"Something",9);
				SYSLOG_INFO("RES: %d", res);

				syslog(LOG_INFO,"HMCTL_TMPL main: start of state %s", 
						hm_esm_state_print(current_state) );

				/* initialize communication with the ESM 
				 * - this starts a separate thread for dbus communication 
				 */
				if ( init_com(argv[0]) != HM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: initializing communication failed"); 	
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}

				/* SNMP thread 
				 * - start AgentX subagent here
				 */
				if ( local_init_snmp() != HM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: initializing SNMP failed");
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}

				/* setup monitoring of work-module
				 * ex: signal handler
				 */

				/* do other initialization actions, if needed
				 * ...
				 */

				/* set status of this component to OK */
				value = HM_ESM_CTRL_STATUS_OK;
				if ( hm_esm_event_send(hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
							&value) != HM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: sending event(%s,%d) failed",
							hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
							value);
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}
				SYSLOG_INFO("HMCTL_TMPL main: set status of this component to HM_ESM_CTRL_STATUS_OK ");

				/* tell the ESM this state is complete and move to the next one 
				 * - send the message/event CTRL_STARTED
				 */
				if ( hm_esm_state_notify(hm_esm_state_event_names[HM_ESM_CTRL_STARTED]) 
						!= HM_ESM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: notifying ESM with %s failed",
							hm_esm_state_event_names[HM_ESM_CTRL_STARTED]);
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}
				SYSLOG_INFO("HMCTL_TMPL main: notified ESM with %s", 
						hm_esm_state_event_names[HM_ESM_CTRL_STARTED]);

				/* move to next state */
				current_state = HM_ESM_STATE_LOAD_CFG; 
				break;

			/********************************************************************************
			 * STATE LOAD_CFG
			 * - notify ESM we are ready to receive configuration via SNMP (the SNMP thread
			 *   has been started)
			 * - wait for state-event from ESM to tell us when the CFG MGR finished loading
			 *   the configuration into our subtree
			 * - read the OIDs set and generate the configuration
			 * - signal the ESM that the configuration is ready
			 * - wait for state-event from ESM to enter into the next state
			 */	
			case HM_ESM_STATE_LOAD_CFG:
				syslog(LOG_INFO,"HMCTL_TMPL main: start of state %s", 
						hm_esm_state_print(current_state) );

				/* notify ESM we are ready to receive configuration */
				if ( hm_esm_state_notify(hm_esm_state_event_names[HM_ESM_CTRL_WAIT4CFG])
						!= HM_ESM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: notifying ESM with %s failed",
							hm_esm_state_event_names[HM_ESM_CTRL_WAIT4CFG]);
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}
				syslog(LOG_INFO,"HMCTL_TMPL main: notified ESM with %s", 
						hm_esm_state_event_names[HM_ESM_CTRL_WAIT4CFG]);

				/* 
				 * during this time, the CFG MGR should populate our SNMP sub-tree 
				 */

				/* wait for event from ESM 
				 * - HM_ESM_LOAD_CFG means the MIB was populated by the CFG MGR
				 *   and it can now be used to generate the configuration
				 * - ...
				 */
				system_state = hm_esm_state_recv(0);
				switch ( system_state ) {
					case HM_ESM_STATE_INVALID:
						SYSLOG_ERR("HMCTL_TMPL main: receiving state event from ESM failed");
						current_state = HM_ESM_STATE_FAULT;
						break;
					case HM_ESM_STATE_LOAD_CFG:
						; /* as expected; continue with this state */
						break;
					case HM_ESM_STATE_CONTINUE:
						/* monitor pipe triggered 
						 * - see what happened with the work-module
						 */

						/* if the work-module failed, you can notify the ESM
						 * of this with the status event
						 *
						 * value = HM_ESM_CTRL_STATUS_WORK_FAILED;
						 * if ( hm_esm_event_send(
						 * 			hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
						 * 			&value) != HM_SUCCESS ) {
						 * 		SYSLOG_ERR("HMCTL_TMPL main: sending event(%s, %d) failed",
						 * 			hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
						 * 			value);
						 * 		exit(EXIT_FAILURE);
						 * }
						 *
						 * see ctl_vrrp, ctl_ntp
						 */

						break;
					case HM_ESM_STATE_FAULT:
						/* error state transition */
						current_state = HM_ESM_STATE_FAULT;
						break;
					default:
						/* unknown or unexpected state */
						syslog(LOG_ERR,"should not get here");
						assert(FALSE);
				}
				syslog(LOG_INFO,"HMCTL_TMPL main: received from ESM system state %s", 
						hm_esm_state_names[system_state]);

				/* move to STATE FAULT, if needed */
				if ( current_state == HM_ESM_STATE_FAULT ) {
					SYSLOG_INFO("HMCTL_TMPL main: moving to STATE FAULT");
					break;
				}

				/* generate configuration for the working component */
				if ( generate_configuration() != HM_SUCCESS ) {
					/* set error status in ESM */
					SYSLOG_ERR("HMCTL_TMPL main: generating configuration failed");
					value = HM_ESM_CTRL_STATUS_CFG_FAILED;
					if ( hm_esm_event_send(hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
								&value ) != HM_SUCCESS ) {
						SYSLOG_ERR("HMCTL_TMPL main: sending event(%s,%d) failed",
								hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
								value);
					}
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}

				/* notify ESM that the configuration is ready */
				if ( hm_esm_state_notify(hm_esm_state_event_names[HM_ESM_CTRL_CFGREADY])
						!= HM_ESM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: notifying ESM with %s failed",
							hm_esm_state_event_names[HM_ESM_CTRL_CFGREADY]);
					current_state = HM_ESM_STATE_FAULT; 
					break;
				}
				syslog(LOG_INFO,"HMCTL_TMPL main: notified ESM with %s", 
						hm_esm_state_event_names[HM_ESM_CTRL_CFGREADY]);

				/* wait for event from ESM 
				 * - HM_ESM_STATE_RUN means that we can start the work-module
				 * - ...
				 */
				system_state = hm_esm_state_recv(0);
				switch ( system_state ) {
					case HM_ESM_STATE_INVALID:
						SYSLOG_ERR("HMCTL_TMPL main: receiving state event from ESM failed");
						current_state = HM_ESM_STATE_FAULT;
						break;
					case HM_ESM_STATE_RUN:
						current_state = HM_ESM_STATE_RUN;
						break;
					case HM_ESM_STATE_CONTINUE:
						/* monitor pipe triggered 
						 * - see what happened with the work-module
						 */

						/* if the work-module failed, you can notify the ESM
						 * of this with the status event
						 *
						 * value = HM_ESM_CTRL_STATUS_WORK_FAILED;
						 * if ( hm_esm_event_send(
						 * 			hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
						 * 			&value) != HM_SUCCESS ) {
						 * 		SYSLOG_ERR("HMCTL_TMPL main: sending event(%s, %d) failed",
						 * 			hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
						 * 			value);
						 * 		exit(EXIT_FAILURE);
						 * }
						 *
						 * see ctl_vrrp, ctl_ntp
						 */

						break;
					case HM_ESM_STATE_FAULT:
						/* error state transition */
						current_state = HM_ESM_STATE_FAULT;
						break;
					default:
						syslog(LOG_ERR,"should not get here");
						assert(FALSE);
				}
				syslog(LOG_INFO,"HMCTL_TMPL main: received from ESM system state %s", 
						hm_esm_state_names[system_state]);

				break;

			/********************************************************************************
			 * STATE RUN
			 * - start the work-module (thread/process single/multiple instance)
			 * - report to the ESM work-module started
			 * - move to next state (RUNNING)
			 */
			case HM_ESM_STATE_RUN:
				syslog(LOG_INFO,"HMCTL_TMPL main: start of state %s", 
						hm_esm_state_print(current_state) );

				/* start work-module 
				 * - this can be a separate process or a thread 
				 * - exemplify with process
				 */
				if ( start_work() != HM_SUCCESS ) {
					/* set error status in ESM */
					SYSLOG_ERR("HMCTL_TMPL main: starting work-module failed");
					value = HM_ESM_CTRL_STATUS_WORK_FAILED;
					if ( hm_esm_event_send(hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
								&value ) != HM_SUCCESS ) {
						SYSLOG_ERR("HMCTL_TMPL main: sending event(%s,%d) failed",
								hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
								value);
					}
					current_state = HM_ESM_STATE_FAULT; 
					break;
				}

				/* notify ESM that the component is running */
				if ( hm_esm_state_notify(hm_esm_state_event_names[HM_ESM_CTRL_COMPRUNNING])
						!= HM_ESM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: notifying ESM with %s failed",
							hm_esm_state_event_names[HM_ESM_CTRL_COMPRUNNING]);
					current_state = HM_ESM_STATE_FAULT; 
					break; 
				}
				syslog(LOG_INFO,"HMCTL_TMPL main: notified ESM with %s", 
						hm_esm_state_event_names[HM_ESM_CTRL_COMPRUNNING]);
						
				/* reconfiguration finished */
				hm_esm_reconf_status = HM_ESM_RECONF_NONE;

				/* move to next state */
				current_state = HM_ESM_STATE_RUNNING;

				break;

			/********************************************************************************
			 * STATE RUNNING
			 * - monitor work module (specific to each component)
			 * - wait for state events from ESM
			 */
			case HM_ESM_STATE_RUNNING:
				syslog(LOG_INFO,"HMCTL_TMPL main: start of state %s", 
						hm_esm_state_print(current_state) );

				/* wait for event from ESM 
				 * - HM_ESM_STATE_CONTINUE means that the hm_esm_state_recv function
				 *   was unblocked by writing on the monitor-pipe (verify status of 
				 *   work-module(s))
				 * - HM_ESM_STATE_RUNNING means to continue this state
				 * - ... 
				 */

				//HMLOG_USER_INFO("SYSSTAT daemon started");

				int workModuleStatus;

				for (;;) {
					system_state = hm_esm_state_recv(1 /* handle events */);
					switch ( system_state ) {
						case HM_ESM_STATE_INVALID:
							SYSLOG_ERR("HMCTL_TMPL main: receiving state event from ESM failed");
							current_state = HM_ESM_STATE_FAULT;
							break;
						case HM_ESM_STATE_CONTINUE:
							
							workModuleStatus = getWorkStatus();

							if (workModuleStatus == SYSSTAT_OFFLINE)
							{
								value = HM_ESM_CTRL_STATUS_WORK_FAILED;
								if ( hm_esm_event_send(
										hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
										&value) != HM_SUCCESS ) {
									SYSLOG_ERR("HMCTL_TMPL main: sending event(%s, %d) failed",
										hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
										value);
										}
									//HMLOG_USER_ALERT("SYSSTAT daemon failed");

									exit(EXIT_FAILURE);
							}

							break;
						case HM_ESM_STATE_EVENT:
							/* an non state-event has been received */
							/*
							 * react to it here, regenerate configuration,
							 * restart work module 
							 * ...
							 *
							 * we should only reach this if 1 was passed to hm_esm_state_recv()
							 */
							current_state = HM_ESM_STATE_RUNNING;
							break;
						case HM_ESM_STATE_RUNNING:
							current_state = HM_ESM_STATE_RUNNING;
							break;
						case HM_ESM_STATE_RECONF:
							/* reconfiguration */
							current_state = HM_ESM_STATE_RECONF;
							break;
						case HM_ESM_STATE_FAULT:
							/* error state transition */
							current_state = HM_ESM_STATE_FAULT;
							break;
						default:
							syslog(LOG_ERR,"should not get here");
							assert(FALSE);
					}
					syslog(LOG_INFO,"HMCTL_TMPL main: received from ESM system state %s", 
							hm_esm_state_names[system_state]);
					
					/* if we should do something else than listen for events */
					if ( current_state != HM_ESM_STATE_RUNNING ) {
						break;
					}
				}
				break;

			/********************************************************************************
			 * STATE RECONF
			 * - reconfiguration triggered
			 * - verify that the RECONFIGURATION system event has been set
			 */
			case HM_ESM_STATE_RECONF:
				/* get reconfiguration type */
				
				/* do something different if hard or soft reconfiguration
				 * TO BE DETERMINED
				 */
				hm_esm_reconf_status = hm_esm_get_reconf();
				switch ( hm_esm_reconf_status ){
					case HM_ESM_RECONF_INVALID:
					case HM_ESM_RECONF_NONE:
						/* error in API function or event not set */
						current_state = HM_ESM_STATE_FAULT;
						break;
					case HM_ESM_RECONF_SOFT:
						/* do something */
						break;
					case HM_ESM_RECONF_HARD:
						/* do something */
						break;
					default:
						/* should not get here */
						assert(FALSE);
				}

				/* move to STATE FAULT, if needed */
				if ( current_state == HM_ESM_STATE_FAULT ) {
					SYSLOG_INFO("HMCTL_TMPL main: moving to STATE FAULT");
					break;
				}

				/* move to LOAD CFG state */
				current_state = HM_ESM_STATE_LOAD_CFG;
				break;

			/********************************************************************************
			 * STATE FAULT
			 * - system fault reported 
			 */
			case HM_ESM_STATE_FAULT:
				syslog(LOG_INFO,"HMCTL_TMPL main: start of state %s", 
						hm_esm_state_print(current_state) );
				
				/*
				 * cleanup ...
				 *
				 */

				/* set status in ESM */
				value = HM_ESM_CTRL_STATUS_FAULT;
				if ( hm_esm_event_send(
							hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
							&value) != HM_SUCCESS ) {
					SYSLOG_ERR("HMCTL_TMPL main: sending event(%s, %d) failed",
							hm_esm_state_event_names[HM_ESM_CTRL_STATUS],
							value);
				}

				exit(EXIT_FAILURE);
				break;

			default:
				/* should not get here */
				assert(FALSE);
		}
	}

	/* will not reach this point */
	assert(FALSE);

	return EXIT_FAILURE;
}

/********************************************************************************
 *	Setup logging for the controlling component
 */
static int setup_logging()
{
	return HM_SUCCESS;
}

/********************************************************************************
 *	Initialize communication (dbus)
 */
static int init_com(const char *arg)
{
	int rc;
	SYSLOG_TRACE_FUNCTION_IN;

	/* program name */
	progname = get_program_name(arg);
	SYSLOG_INFO("HMCTL_TMPL main: program name is %s\n", progname);

	/* generate component state-notification events */
	rc = hm_esm_state_generate_event_names(progname);
	if ( rc != HM_ESM_SUCCESS ) {
		SYSLOG_ERR("ERR: failed to generate event names");
		SYSLOG_TRACE("error");
		SYSLOG_TRACE_FUNCTION_OUT;
		return HM_FAILURE;
	}

	/* component-specific event names */
	SYSLOG_INFO("HMCTL_TMPL init-com: events (%s) (%s) (%s) (%s) (%s)",
			hm_esm_state_event_names[HM_ESM_CTRL_STARTED],
			hm_esm_state_event_names[HM_ESM_CTRL_WAIT4CFG],
			hm_esm_state_event_names[HM_ESM_CTRL_CFGREADY],
			hm_esm_state_event_names[HM_ESM_CTRL_COMPRUNNING],
			hm_esm_state_event_names[HM_ESM_CTRL_STATUS]
			);	

	rc = hm_esm_state_register_for_events();
	if ( rc != HM_ESM_SUCCESS ) {
		SYSLOG_ERR("ERR: failed to register for state-events");
		SYSLOG_TRACE("error");
		SYSLOG_TRACE_FUNCTION_OUT;
		return HM_FAILURE;
	}
	SYSLOG_DEBUG("HMCTL_TMPL init_com: registered for state events");

	/* register for other events that the component is interested in */
	/*
	 * rc = hm_esm_event_register(4, "event1", "event2",
	 * 			"event3", "event4");
	 * if ( rc != HM_ESM_SUCCESS ) {
	 * 		...
	 * }
	 */

	/* start dbus main loop */
	rc = hm_esm_event_start_main_loop();
	if ( rc != HM_ESM_SUCCESS ) {
		SYSLOG_ERR("ERR: failed to start communication loop");
		SYSLOG_TRACE("error");
		SYSLOG_TRACE_FUNCTION_OUT;
		return HM_FAILURE;
	}
	SYSLOG_INFO("HMCTL_TMPL init-com: started communication loop");

	SYSLOG_TRACE_FUNCTION_OUT;
	return HM_SUCCESS;
}

/********************************************************************************
 *	Initialize SNMP agentX
 */
static int local_init_snmp()
{
	SYSLOG_TRACE_FUNCTION_IN;

    if (pthread_create(&snmp_thread, NULL, (void*(*)(void*))agentX, NULL) != 0)
    {
        SYSLOG_ERR("Failed creating SNMP subagent");
        
        return HM_ERROR;
    }

    hm_esm_block();

	SYSLOG_INFO("HMCTL_TMPL init_snmp: snmp agent started");

	SYSLOG_TRACE_FUNCTION_OUT;
	return HM_SUCCESS;
}

/********************************************************************************
 *	Generate working component configuration
 */
static int generate_configuration()
{
	SYSLOG_TRACE_FUNCTION_IN;

	/* 
	 * read OIDs and other dependencies (handled through the
	 * ESM event API) and generate the configuration
	 */

	/* you can differentiate between a system startup configuration 
	 * and a reconfiguration if necessary
	 */
	switch ( hm_esm_reconf_status ){
		case HM_ESM_RECONF_SOFT:
		case HM_ESM_RECONF_HARD:
			/* do something special here */
			break;
		case HM_ESM_RECONF_NONE:
			/* system startup configuration */
			SYSLOG_DEBUG("HMCTL_TMPL generate_configuration: initial configuration");
			break;
		default:
			/* should not reach here */
			assert(FALSE);
	}

	SYSLOG_TRACE_FUNCTION_OUT;
	return HM_SUCCESS;
}

/********************************************************************************
 *	Start working module
 */
static int start_work()
{
	SYSLOG_TRACE_FUNCTION_IN;
	
	/* see if it is a reconfiguration, reload config 
	 * - restart
	 * - send SIGHUP
	 */

	if (pthread_create(&work_thread, NULL, (void*(*)(void*))workThread, NULL) != 0)
	{
		SYSLOG_ERR("Failed creating SNMP subagent");
        
        return HM_ERROR;
	}

	SYSLOG_TRACE_FUNCTION_OUT;
	return HM_SUCCESS;
}

/********************************************************************************
 *	Get program name
 */
static const char * get_program_name(const char *name)
{
	const char * result;
	
	assert( name != NULL );
	assert( strlen(name) > 0 && strcmp(name,"/") != 0 );

	result = strrchr(name, '/');
	if ( result == NULL ) {
		return name;
	} else {
		result = result + 1;
	}

	return result;
}

