/*************************************************************************\
* Copyright (c) 2003 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2003 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7 and higher are distributed subject to the
* Software License Agreement found in the file LICENSE that is included
* with this distribution.
\*************************************************************************/

/* $Id$ */

/* Author: Andrew Johnson	Date: 2003-04-08 */

/* Usage:
 *  softIoc [-D softIoc.dbd] [-h] [-s] [-a ascf]
 *	[-m macro=value,macro2=value2] [-d file.db]
 *	[st.cmd]
 *
 *  If used the -D option must come first, and specify the
 *  path to the softIoc.dbd file.  The compile-time install
 *  location is saved in the binary as a default.
 *
 *  Usage information will be printed if -h is given, then
 *  the program will exit normally.
 *
 *  The -s option causes an interactive shell to be started
 *  after all arguments have been processed.
 *
 *  Access Security can be enabled with the -a option giving
 *  the name of the configuration file; if any macros were
 *  set with -m before the -a option was given, they will be
 *  used as access security substitution macros.
 *
 *  Any number of -m and -d arguments can be interspersed;
 *  the macros are applied to the following .db files.  Each
 *  later -m option causes earlier macros to be discarded.
 *
 *  A st.cmd file is optional.  If any databases were loaded
 *  the st.cmd file will be run *after* iocInit.  To perform
 *  iocsh commands before iocInit, all database loading must
 *  be performed by the script itself, or by the user from
 *  the interactive IOC shell.
 *
 *  It is possible for a database to cause the IOC to exit:
 *  Create a subroutine record with SNAM="exit".  When this
 *  record processes, the OS' exit() routine will be called.
 *  The value in field A determines whether the exit status
 *  is EXIT_SUCCESS (A==0.0) or EXIT_FAILURE (A!=0.0).
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "registerRecordDeviceDriver.h"
#include "registryFunction.h"
#include "epicsThread.h"
#include "dbStaticLib.h"
#include "subRecord.h"
#include "dbAccess.h"
#include "asDbLib.h"
#include "iocInit.h"
#include "iocsh.h"

#define QUOTE(x) #x
#define DBD_FILE(top) QUOTE(top) "/dbd/softIoc.dbd"

const char *arg0;
const char *base_dbd = DBD_FILE(EPICS_BASE);


static void exitSubroutine(subRecord *precord) {
    exit((precord->a == 0.0) ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void usage(int status) {
    printf("Usage: %s [-D softIoc.dbd] [-h] [-s] [-a ascf]\n", arg0);
    puts("\t[-m macro=value,macro2=value2] [-d file.db]");
    puts("\t[st.cmd]");
    puts("Compiled-in default path to softIoc.dbd is:");
    printf("\t%s\n", base_dbd);
    exit(status);
}


int main(int argc, char *argv[])
{
    char *dbd_file = const_cast<char*>(base_dbd);
    char *macros = NULL;
    int startIocsh = 0;	/* default = no shell */
    int loadedDb = 0;
    
    arg0 = strrchr(*argv, '/');
    if (!arg0) {
	arg0 = *argv;
    } else {
	++arg0;	/* skip the '/' */
    }
    
    --argc, ++argv;
    
    /* Do this here in case the dbd file not available */
    if (argc>0 && **argv=='-' && (*argv)[1]=='h') {
	usage(EXIT_SUCCESS);
    }
    
    if (argc>1 && **argv=='-' && (*argv)[1]=='D') {
	dbd_file = *++argv;
	argc -= 2;
	++argv;
    }
    
    if (dbLoadDatabase(dbd_file, NULL, NULL)) {
	exit(EXIT_FAILURE);
    }
    
    registerRecordDeviceDriver(pdbbase);
    registryFunctionAdd("exit", (REGISTRYFUNCTION) exitSubroutine);
    
    while (argc>1 && **argv == '-') {
	switch ((*argv)[1]) {
	case 'a':
	    if (macros) asSetSubstitutions(macros);
	    asSetFilename(*++argv);
	    --argc;
	    break;
	
	case 'd':
	    if (dbLoadRecords(*++argv, macros)) {
		exit(EXIT_FAILURE);
	    }
	    loadedDb = 1;
	    --argc;
	    break;
	
	case 'h':
	    usage(EXIT_SUCCESS);
	
	case 'm':
	    macros = *++argv;
	    --argc;
	    break;
	
	case 's':
	    startIocsh = 1;
	    break;
	
	default:
	    printf("%s: option '%s' not recognized\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	}
	--argc;
	++argv;
    }
    
    if (argc>0 && **argv=='-') {
	switch((*argv)[1]) {
	case 'a':
	case 'd':
	case 'm':
	    printf("%s: missing argument to option '%s'\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	
	case 'h':
	    usage(EXIT_SUCCESS);
	
	case 's':
	    startIocsh = 1;
	    break;
	
	default:
	    printf("%s: option '%s' not recognized\n", arg0, *argv);
	    usage(EXIT_FAILURE);
	}
	--argc;
	++argv;
    }
    
    if (loadedDb) {
	iocInit();
	epicsThreadSleep(0.2);
    }
    
    /* run user's startup script */
    if (argc>0) {
	if (iocsh(*argv)) exit(EXIT_FAILURE);
	epicsThreadSleep(0.2);
	loadedDb = 1;	/* Give it the benefit of the doubt... */
    }
    
    /* start an interactive shell if it was requested */
    if (startIocsh) {
	iocsh(NULL);
    } else {
	if (loadedDb) {
	    epicsThreadExitMain();
	} else {
	    printf("%s: Nothing to do!\n", arg0);
	    usage(EXIT_FAILURE);
	}
    }
    return EXIT_SUCCESS;
}