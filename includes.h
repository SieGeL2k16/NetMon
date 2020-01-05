/**************************************************************************************************
 * FILENAME: includes.h
 *  PURPOSE: Main include file, includes all other
 *  CREATED: 21-Jun-2004
 * MODIFIED: 26-Dec-2006
 *   AUTHOR: SGL
 **************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#ifdef __amigaos4__
#include <sys/time.h>
#endif

#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/gadtools.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <proto/timer.h>
#include <proto/icon.h>
#include <devices/timer.h>
#include <workbench/startup.h>

#ifndef __amigaos4__
#include <clib/alib_protos.h>
#include <libraries/mui.h>
#include <math.h>
#define int8 long
#include <stdio.h>
#endif

// And include now also our own headerfiles:

#include "proto.h"
#include "global_defines.h"
#include "structs.h"
#include "netmon_rev.h"
