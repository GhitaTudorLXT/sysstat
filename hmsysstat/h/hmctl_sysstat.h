/***************************** FILE HEADER ***********************************/
/*!
*  \file		hmctl_tmpl.h
*
*  \brief		<b>Header for component-control template application</b>\n
*
*  \author		Catalin Gheorghe\n
*				Copyright 2011 Hirschmann Automation & Control GmbH
*
*  \version	1a	2011-08-25 Catalin Gheorghe    created 
*
*//**************************** FILE HEADER **********************************/

#ifndef _HMCTL_TMPL_H_INCLUDE
#define _HMCTL_TMPL_H_INCLUDE

/******************************************************************************
* Includes
******************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <hm_esm.h>
#include <hmlog.h>

/******************************************************************************
* Defines
******************************************************************************/

/**
 * \def			HMCTL_TMPL_NAME
 *
 * Name of program
 *
 */
#define HMCTL_TMPL_NAME "[hmctl_sysstat]"

/**
 * \def			SYSLOG_TRACE
 *
 * Define for tracing file, function and line number.
 *
 */
#define SYSLOG_TRACE(msg) syslog(LOG_DEBUG, "%s: %s %s %d", msg, \
		__FILE__, __FUNCTION__, __LINE__)

/**
 * \def			SYSLOG_DEBUG
 *
 * Define for printing debug information.
 * Use: SYSLOG_DEBUG("value:%d", n)
 *
 */
#define SYSLOG_DEBUG(msg_format, args...) syslog(LOG_DEBUG, msg_format, ##args )

/**
 * \def			SYSLOG_INFO
 *
 * Define for printing INFO information.
 *
 */
#define SYSLOG_INFO(msg_format, args...) syslog(LOG_INFO, msg_format, ##args )

/**
 * \def			SYSLOG_ERR
 *
 * Define for printing ERR information.
 *
 */
#define SYSLOG_ERR(msg_format, args...) syslog(LOG_ERR, msg_format, ##args )

/**
 * \def			SYSLOG_TRACE_FUNCTION_IN
 *
 * Define for tracing entering a function.
 *
 */
#define SYSLOG_TRACE_FUNCTION_IN syslog(LOG_DEBUG, "enter function %s", \
		__FUNCTION__);

/**
 * \def			SYSLOG_TRACE_FUNCTION_OUT
 *
 * Define for tracing exiting from a function.
 *
 */
#define SYSLOG_TRACE_FUNCTION_OUT syslog(LOG_DEBUG, "exit function %s", \
		__FUNCTION__);

#define HM_SUCCESS 0
#define HM_FAILURE 1

/******************************************************************************
* Function declarations
******************************************************************************/


#endif /* _HMCTL_TMPL_H_INCLUDE */

