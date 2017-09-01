##############################################################################
# Copyright (C) 2011 Hirschmann Automation and Control GmbH
# 
# Makefile for sysstat
#
# modification history:
# 
##############################################################################

# define source directories for application
SRCDIR_APP = src

# name of application
NAME_APP = ctl_sysstat

# target to be done after component build targets
POSTTARGETS = upstart_conf

# make definitions
include $(HIRSCHMANN_BASE)/hmDefs.mak

# extra defines
#EXTRA_DEFINE += -D

# extra compiler flags
ADDED_CFLAGS += -Werror

# additional includes
EXTRA_INCLUDE += -I$(HIRSCHMANN_BASE)/ESM/h
#EXTRA_INCLUDE += -I/home/tudor/work/eagle/linux_eagle/GPL/dbus/dbus-1.4.6

# extra linking
EXTRA_LINK += -lESM
EXTRA_LINK += $(EXTRA_LINK_SNMPAGENT) $(EXTRA_LINK_LIC)

# make rules
include $(HIRSCHMANN_BASE)/hmRules.mak

# copy upstart conf file
upstart_conf:
	cp *ctl_sysstat*.conf $(OUTPUTDIR)/usr/etc/init/

clean:
	$(HM_CLEAN)
	-rm $(OUTPUTDIR)/usr/etc/init/*ctl_sysstat*.conf

