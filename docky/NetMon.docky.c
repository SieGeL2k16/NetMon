/*
 * NetMon.docky by Sascha 'SieGeL' Pfalz
 * Based on Test Docky by Joerg Strohmeyer 
 * 
 * bumprev 52 Test.docky
 * gcc Test.docky.c -o Test.docky -O3 -nostartfiles
 *
 * $Id: NetMon.docky.c,v 1.4 2007/03/13 22:54:33 siegel Exp $
 */
#include <proto/docky.h>
#define __NOGLOBALIFACE__
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/bsdsocket.h>
#include <proto/icon.h>
#include <proto/timer.h>

#include <libraries/docky.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//#include <sys/time.h>

#include <devices/timer.h>

#include "NetMon.docky_rev.h"

#define	REFRESH_INTERVAL	5														// We want a 5 second delay
#define	REFRESH_DOCKY_INTERVAL REFRESH_INTERVAL * 50	// Delay() requires 1/50 seconds

#define DEBUG 1

#ifdef DEBUG
	#define DBG IExec->DebugPrintF
#else
	#define DBG
#endif

static struct ExecIFace 		*IExec;
static struct GraphicsIFace *IGraphics;
static struct UtilityIFace 	*IUtility;
struct Interface 						*INewlib;

// The libbase is stored inside the Interface, so we require only interfaces here:

struct SocketIFace 			*ISocket		= NULL;
struct 	Library					*SocketBase = NULL;

struct 	List 		*interface_list = NULL;								// List of available interfaces
struct	MinList *InterfaceList;

#define OT_SIZE	40

#define VOL_SIZE 80

// Stores the statistics for the interfaces

struct InterfaceStats
	{
	uint64		BytesUp,
						BytesDown;
	uint32		FirstSample,
						CurrentSample;
	char			OnlineTime[OT_SIZE],
						Volume[VOL_SIZE];
	};
struct InterfaceStats *IS = NULL;

// Stores our complete filename (used to refer to our NetMon.Docky.info file)

char DockyFileName[256];

// This stores either the Interface name to use (from icon) or contains NULL to indicate all interfaces

char InterfaceToUse[32];

// Timer related structs:

struct	MsgPort			*TimerMP		= NULL;
struct	timerequest *TimerIO 		= NULL;
struct	Device			*TimerBase	= NULL;
struct	TimerIFace  *ITimer     = NULL;

/******************************************************************************
 * Prototypes
 ******************************************************************************/

static void GetDockyConfig(char *dockyfilename);
void ReadInterfaceStats(void);
static STRPTR TransformTime(ULONG seconds, STRPTR buffer, ULONG bufsize);
STATIC STRPTR TransformUnit(SBQUAD_T *bytes_u,SBQUAD_T *bytes_d,STRPTR buffer, BOOL onlyCalculate);
STATIC STRPTR MakeHumanString(long long bytes, STRPTR retbuffer);

STATIC ULONG divide_64_by_32(SBQUAD_T * dividend,ULONG divisor,SBQUAD_T * quotient);
STATIC STRPTR convert_quad_to_string(const SBQUAD_T * const number,STRPTR string,LONG max_string_len);

/******************************************************************************
 * Main entry (no startup files here), simply return FAIL to shell:
 ******************************************************************************/

int _start(void)
	{
  return 20;
	}

struct DockyBase
	{
  struct Library libBase;
  APTR SegList;
	};

struct DockyData
	{
  struct DockyObjectNr self;
  struct DockySize ds;
  struct RastPort *rp;
	};

/******************************************************************************
 * Open the library 
 ******************************************************************************/

struct Library *libOpen(struct LibraryManagerInterface *Self, ULONG version UNUSED)
	{
  struct Library *libBase = (struct Library *)Self->Data.LibBase;

  libBase->lib_Flags |= LIBF_DELEXP;

  /* 
   * Add any specific open code here
   * Return 0 before incrementing OpenCnt to fail opening 
   */

	/* Now allocate memory for our stat structure: */

	if(!(IS = IExec->AllocVec(sizeof(struct InterfaceStats),MEMF_VIRTUAL|MEMF_CLEAR)))
		{
		DBG("[NM]: %s","ERROR: NO MEMORY FOR STATS STRUCTURE?");
		return(0);
		}

  /* Add up the open count */
  ((struct Library *)libBase)->lib_OpenCnt++;
  return libBase;
	}

/******************************************************************************
 * Close the library 
 ******************************************************************************/

APTR libClose(struct LibraryManagerInterface *Self)
	{
  struct Library *libBase = (struct Library *)Self->Data.LibBase;

  /* Make sure to undo what open did */

	if(IS)
		{
		IExec->FreeVec(IS);
		IS = NULL;
		}

  /* Make the close count */
  ((struct Library *)libBase)->lib_OpenCnt--;

  return 0;
	}

/******************************************************************************
 * Expunge the library 
 ******************************************************************************/

APTR libExpunge(struct LibraryManagerInterface *Self)
	{
  /* If your library cannot be expunged, return 0 */
  APTR result = (APTR)0;
  struct Library *libBase = (struct Library *)Self->Data.LibBase;
  struct DockyBase *DockyLibBase = (struct DockyBase *)libBase;

  if (libBase->lib_OpenCnt == 0)
  	{
    struct Library *libbase;
    result = (APTR)DockyLibBase->SegList;
    /* Undo what the init code did */

		if(ITimer)
			{
			IExec->DropInterface((struct Interface *)ITimer);
			ITimer = NULL;
			}
		IExec->CloseDevice((struct IORequest *)TimerIO);
		if(TimerIO!=NULL)
			{
			IExec->FreeSysObject(ASOT_IOREQUEST,TimerIO);
			TimerIO = NULL;
			}
		if(TimerMP!=NULL)
			{
			IExec->FreeSysObject(ASOT_PORT,TimerMP);
			TimerMP = NULL;
			}
    libbase = IUtility->Data.LibBase;
    IExec->DropInterface((struct Interface *)IUtility);
    IUtility = NULL;
    IExec->CloseLibrary(libbase);

    libbase = IGraphics->Data.LibBase;
    IExec->DropInterface((struct Interface *)IGraphics);
    IGraphics = NULL;
    IExec->CloseLibrary(libbase);

    libbase = INewlib->Data.LibBase;
    IExec->DropInterface((struct Interface *)INewlib);
    INewlib = NULL;
    IExec->CloseLibrary(libbase);

    IExec->Remove((struct Node *)libBase);
    IExec->DeleteLibrary(libBase);
    IExec->Release();
    }
  else
    {
    result = (APTR)0;
    libBase->lib_Flags |= LIBF_DELEXP;
    }
	return result;
	}

/******************************************************************************
 * The ROMTAG Init Function 
 ******************************************************************************/

struct Library *libInit(struct Library *libBase, APTR seglist, struct Interface *exec)
	{
  struct DockyBase *LibraryBase = (struct DockyBase *)libBase;
  IExec = (struct ExecIFace *)exec;
  struct Library *libbase;

  libBase->lib_Revision     = REVISION;

  /* This assumes your library base has a seglist field */
  LibraryBase->SegList = seglist;

  /* Additional libraries: */
  IExec->Obtain();

  if ((libbase = IExec->OpenLibrary("newlib.library", 52)))
  	{
    INewlib = IExec->GetInterface(libbase, "main", 1, NULL);
    if (INewlib)
    	{
      if ((libbase = IExec->OpenLibrary("graphics.library", 52)))
      	{
        IGraphics = (struct GraphicsIFace *)IExec->GetInterface(libbase, "main", 1, NULL);
        if (IGraphics)
        	{
          if ((libbase = IExec->OpenLibrary("utility.library", 52)))
          	{
            IUtility = (struct UtilityIFace *)IExec->GetInterface(libbase, "main", 1, NULL);
            if (IUtility)
            	{
							TimerMP = IExec->AllocSysObject(ASOT_PORT,NULL);
							if(TimerMP)
								{
								TimerIO = IExec->AllocSysObjectTags(ASOT_IOREQUEST,ASOIOR_Size,sizeof(struct timerequest),ASOIOR_ReplyPort,TimerMP,TAG_DONE);
								if(TimerIO)
									{
									if(!(IExec->OpenDevice(TIMERNAME,UNIT_MICROHZ, (struct IORequest *)TimerIO,0)))
										{
										TimerBase = (struct Device *)TimerIO->tr_node.io_Device;
										ITimer = (struct TimerIFace *)IExec->GetInterface((struct Library *)TimerBase, "main", 1, NULL);
										if(ITimer)
											{
			        				return libBase;
											}
										}
									}
								}
              }
            }
          }
        }
      }
    }
	return NULL;
	}

/******************************************************************************
 * ------------------- Manager Interface ------------------------ 
 * These are generic. Replace if you need more fancy stuff 
 ******************************************************************************/

static LONG _manager_Obtain(struct LibraryManagerInterface *Self)
	{
  Self->Data.RefCount++;
  return Self->Data.RefCount;
	}

static ULONG _manager_Release(struct LibraryManagerInterface *Self)
	{
  Self->Data.RefCount--;
  return Self->Data.RefCount;
	}

/******************************************************************************
 * Manager interface vectors 
 ******************************************************************************/

static void *lib_manager_vectors[] =
	{
  (void *)_manager_Obtain,
  (void *)_manager_Release,
  (void *)0,
  (void *)0,
  (void *)libOpen,
  (void *)libClose,
  (void *)libExpunge,
  (void *)0,
  (void *)-1,
	};

/******************************************************************************
 * "__library" interface tag list 
 ******************************************************************************/

static struct TagItem lib_managerTags[] =
	{
  {MIT_Name,          (ULONG)"__library"},
  {MIT_VectorTable,   (ULONG)lib_manager_vectors},
  {MIT_Version,       1},
  {TAG_DONE,          0}
	};

/******************************************************************************
 * ------------------- Library Interface(s) ------------------------ 
 ******************************************************************************/

static struct DockyIFace * _docky_Clone(struct DockyIFace *Self);

/******************************************************************************
 * DockyGet() Method 
 ******************************************************************************/

static BOOL _docky_DockyGet(struct DockyIFace *Self,uint32 msgType,uint32 * msgData)
	{
  struct DockyData *dd = (struct DockyData *) ((uint32)Self - Self->Data.NegativeSize);
  BOOL result=TRUE;

  switch (msgType)
  	{
    case 	DOCKYGET_Version:
    			*msgData = DOCKYVERSION;
    			break;

    case 	DOCKYGET_GetSize:
          *((struct DockySize *)msgData) = dd->ds;
          break;

		case 	DOCKYGET_FrameDelay:
          *msgData = REFRESH_DOCKY_INTERVAL;					// Interval is set to 5 seconds
          break;

    case 	DOCKYGET_RenderMode:
          *msgData = DOCKYRENDERMODE_RPPA;
          break;

    case 	DOCKYGET_Notifications:
          *msgData = 0;
          break;

    default:
          result=FALSE;
          break;
		}
	return result;
	}

/******************************************************************************
 * DockyProcess() method 
 ******************************************************************************/

static BOOL _docky_DockyProcess(struct DockyIFace *Self,uint32 turnCount,uint32 * msgType,uint32 * msgData,BOOL * anotherTurn)
	{
  //struct DockyData *dd = (struct DockyData *) ((uint32)Self - Self->Data.NegativeSize);

  // we need no messages
  // msgType, msgData and anotherTurn are set by AmiDock already to NULL, NULL and FALSE
  // *msgType=0;
  // *msgData=0;
  // *anotherTurn=FALSE;

  return TRUE; // needs drawing?
	}

/******************************************************************************
 * drawImage() method - Renders the Docky
 ******************************************************************************/

static void drawImage(struct DockyData *dd)
	{
	static int i = 0;
  TEXT temp[60], temp2[60];

	ReadInterfaceStats();
	if(InterfaceToUse[0] == '\0')
		{
  	IUtility->SNPrintf(temp, 60, "(%ld) Uptime: %s", i++,IS->OnlineTime);
		}
	else
		{
  	IUtility->SNPrintf(temp, 60, "(%ld) Online: %s", i++,IS->OnlineTime);
		}
	IUtility->SNPrintf(temp2, 60, "%s",IS->Volume);
	DBG("VOLUME: %s",IS->Volume);
  if (dd->rp)
  	{
    IGraphics->Move(dd->rp, 12, 10);
    IGraphics->SetABPenDrMd(dd->rp, 1, 0, JAM1);
    IGraphics->Text(dd->rp, temp, strlen(temp));
    IGraphics->Move(dd->rp, 12-1, 10-1);
    IGraphics->SetABPenDrMd(dd->rp, 2, 0, JAM1);
    IGraphics->Text(dd->rp, temp, strlen(temp));
		IGraphics->Move(dd->rp, 12,20);
    IGraphics->SetABPenDrMd(dd->rp, 1, 0, JAM1);
		IGraphics->Text(dd->rp, temp2, strlen(temp2));
    }
	}

/******************************************************************************
 * DockySet() Method 
 ******************************************************************************/

static BOOL _docky_DockySet(struct DockyIFace *Self,uint32 msgType,uint32 msgData)
	{
  struct DockyData *dd = (struct DockyData *) ((uint32)Self - Self->Data.NegativeSize);
  BOOL result=TRUE;

  switch (msgType)
  	{
    case 	DOCKYSET_RenderDestination:
    			{
          struct DockyRenderDestination *drd = (struct DockyRenderDestination *)msgData;

          switch (drd->renderMode)
          	{
            case 	DOCKYRENDERMODE_RPPA:
                  dd->ds = drd->renderSize;
                  dd->rp = drd->render.RP;
                  break;

            default:
                  dd->rp = NULL;
                  result = FALSE;
                  break;
            }
          }
          break;

		case 	DOCKYSET_RedrawNow:
          drawImage(dd);
          break;

    case 	DOCKYSET_DockTypeChange:
          	{
            struct DockyDockType *ddt=(struct DockyDockType *)msgData;
            if (dd->self.dockNr==ddt->dockNr && ddt->dockType!=AMIDOCK_DockType_Icons)
            result=FALSE;
            }
          break;

		case 	DOCKYSET_DockyChange:
          dd->self=*((struct DockyObjectNr *)msgData);
          break;

		case	DOCKYSET_FileName:
					IUtility->Strlcpy(DockyFileName,(STRPTR)msgData,255);
					GetDockyConfig(DockyFileName);
					break;

    default:
          result=FALSE;
          break;
		}
	return result;
	}

/******************************************************************************
 * Expunge() Method 
 ******************************************************************************/

static void _docky_Expunge(struct DockyIFace *Self)
	{
  if (!Self->Data.RefCount)
  	{
    IExec->FreeVec((void *)((uint32)Self - Self->Data.NegativeSize));
    }
	}

/******************************************************************************
 * Obtain() Method 
 ******************************************************************************/

static uint32 _docky_Obtain(struct DockyIFace *Self)
	{
  Self->Data.RefCount++;
  return Self->Data.RefCount;
	}

/******************************************************************************
 * Release() Method 
 ******************************************************************************/

static uint32 _docky_Release(struct DockyIFace *Self)
	{
	if(interface_list != NULL) 	
		{
		ISocket->ReleaseInterfaceList(interface_list);
		interface_list = NULL;
		}
	if(ISocket)
		{
    IExec->DropInterface((struct Interface *)ISocket);
    ISocket = NULL;
		}
	if(SocketBase)
		{
    IExec->CloseLibrary(SocketBase);
		SocketBase = NULL;
		}
  Self->Data.RefCount--;

  if (!Self->Data.RefCount && (Self->Data.Flags & IFLF_CLONED))
  	{
    IExec->DeleteInterface((struct Interface *)Self);
    return 0;
   	}
	return Self->Data.RefCount;
	}

static void *docky_vectors[] =
	{
  (void *)_docky_Obtain,
  (void *)_docky_Release,
  (void *)_docky_Expunge,
  (void *)_docky_Clone,
  (void *)_docky_DockyGet,
  (void *)_docky_DockySet,
  (void *)_docky_DockyProcess,
  (void *)-1
	};

static struct TagItem dockyTags[] =
	{
  {MIT_Name,          (uint32)"docky"},
  {MIT_VectorTable,   (uint32)docky_vectors},
  {MIT_DataSize,      (uint32)(sizeof(struct DockyData))},
  {MIT_Flags,         IFLF_PRIVATE},
  {MIT_Version,       1},
  {TAG_DONE,          0}
	};

/******************************************************************************
 * Clone() Method 
 ******************************************************************************/

static struct DockyIFace * _docky_Clone(struct DockyIFace *Self)
	{
  struct DockyIFace *docky = NULL;

#ifdef DEBUG
	struct 	Node *node;
#endif

	if(!(SocketBase = IExec->OpenLibrary("bsdsocket.library",4)))
		{
		return(NULL);
		}
	ISocket = (struct SocketIFace *)IExec->GetInterface(SocketBase, "main", 1, NULL);
	if(!ISocket)
		{
		return(NULL);
		}
	interface_list = ISocket->ObtainInterfaceList();
	if(interface_list == NULL || (IsListEmpty(interface_list)))
		{
		return(NULL);
		}

#ifdef DEBUG

	for(node = interface_list->lh_Head ; node->ln_Succ != NULL ; node = node->ln_Succ)
	  {
		DBG("[NM]: FOUND INTERFACE %s\n",node->ln_Name);
		}

#endif

  docky = (struct DockyIFace *)IExec->MakeInterface(Self->Data.LibBase, dockyTags);
  if (docky)
  	{
    struct DockyData *dd = (struct DockyData *) ((uint32)docky - docky->Data.NegativeSize);

    docky->Data.Flags |= IFLF_CLONED;

    dd->ds.width  = 200;
    dd->ds.height = 32;
    dd->rp        = NULL;
    }
	return docky;
	}

static uint32 libInterfaces[] =
	{
  (uint32)lib_managerTags,
  (uint32)dockyTags,
  (uint32)0
	};

static struct TagItem libCreateTags[] =
	{
  {CLT_DataSize,      (uint32)(sizeof(struct DockyBase))},
  {CLT_InitFunc,      (uint32)libInit},
  {CLT_Interfaces,    (uint32)libInterfaces},
  {TAG_DONE,          0}
	};

/******************************************************************************
 * ------------------- ROM Tag ------------------------ 
 ******************************************************************************/

static const struct Resident lib_res USED =
	{
  RTC_MATCHWORD,
  (struct Resident *)&lib_res,
  (APTR)(&lib_res+1),
  RTF_NATIVE|RTF_AUTOINIT,
  VERSION,
  NT_LIBRARY,
  0, /* PRI */
  "NetMon.docky",
  VSTRING,
  libCreateTags
	};

/******************************************************************************
 * Tries to determine the configuration for this docky from the according icon.
 * If no icon exists we assume interfaces = ALL, else the user has to supply
 * a list with Interface names separated by a comma to decide which interfaces
 * should be accounted.
 * NOTE: This function is called directly after recieving the filename in DOCKYSet(),
 *       Any error must be silently ignored!
 ******************************************************************************/

static void GetDockyConfig(char *dockyfilename)
	{
	struct 	Library 		*IconBase = NULL;
	struct 	IconIFace 	*IIcon 		= NULL;
	struct	DiskObject	*dobj			= NULL;
	STRPTR	icondata = NULL;

	if(IconBase = IExec->OpenLibrary("icon.library",0L))
		{
		if(IIcon = (struct IconIFace *)IExec->GetInterface(IconBase, "main", 1, NULL))
			{
			dobj = IIcon->GetDiskObject(dockyfilename);
			if(dobj)
				{
				icondata = IIcon->FindToolType((STRPTR *)dobj->do_ToolTypes,"INTERFACE");
				IIcon->FreeDiskObject(dobj);
				}
			}
		}
	if(IIcon)			IExec->DropInterface((struct Interface *)IIcon);
	IIcon = NULL;
	if(IconBase)	IExec->CloseLibrary(IconBase);
	IconBase = NULL;
	if(icondata) 
		{
		IUtility->Strlcpy(InterfaceToUse,icondata,32);	
		}
	else 
		{
		DBG("[NM]: Using all interfaces\n");
		IUtility->ClearMem(InterfaceToUse,32);
		}
	}

/******************************************************************************
 * Function reads interface statistics and put the collected data inside the
 * global struct "InterfaceStats". The contents of this struct is written to
 * Docky rastport in "drawImage"
 ******************************************************************************/

void ReadInterfaceStats(void)
	{
	struct		timeval	ltv,current,calctv;
	SBQUAD_T	BytesIn 	= {0L,0L};
	SBQUAD_T	BytesOut	= {0L,0L};
	long			errn;

	ITimer->GetSysTime(&current);

	if(InterfaceToUse[0]=='\0')
		{
		if(ISocket->SocketBaseTags(
				SBTM_GETREF(SBTC_GET_BYTES_RECEIVED),	&BytesIn,
				SBTM_GETREF(SBTC_GET_BYTES_SENT),		&BytesOut,
			TAG_END) != 0)
			{
			errn = ISocket->Errno();
			IUtility->SNPrintf(IS->OnlineTime,OT_SIZE,"ERROR %ld !",errn);
			DBG("[NM]: SocketBaseTags() returned %ld!\n",errn);
			return;
			}
		ITimer->GetUpTime(&calctv);
		}
	else
		{
		if(ISocket->QueryInterfaceTags(InterfaceToUse,
						IFQ_GetBytesIn,		&BytesIn,
						IFQ_GetBytesOut,	&BytesOut,
						IFQ_LastStart,		&ltv,
						TAG_END) != 0)
			{
			errn = ISocket->Errno();
			IUtility->SNPrintf(IS->OnlineTime,OT_SIZE,"ERROR %ld !!",errn);
			DBG("[NM]: QueryInterfaceTags() returned %ld for IFACE=%s!\n",errn,InterfaceToUse);
			return;
			}
		IExec->CopyMem(&current,&calctv,sizeof(struct timeval));
		ITimer->SubTime(&calctv,&ltv);
		}	
	TransformTime(calctv.tv_secs, IS->OnlineTime,OT_SIZE);
	TransformUnit(&BytesIn,&BytesOut,IS->Volume,FALSE);
	}

/**************************************************************************************************
 * FUNCTION: TransformTime()
 *  PURPOSE: Returns given seconds in a Human-readable format: HH:MM.SS
 *    INPUT: seconds	=> Seconds to convert
 *           buffer		=> Buffer to take string
 *   RETURN: Converted String
 **************************************************************************************************/

static STRPTR TransformTime(ULONG seconds, STRPTR buffer, ULONG bufsize)
	{
  ULONG hours,minutes, secs, days;

	hours	= seconds / 3600;
	if((seconds / 60) >= 60) minutes = (seconds - ((floor(seconds / 3600) * 3600))) / 60;
	else minutes = seconds / 60;
	secs = seconds % 60;
	if(hours < 48)
		{
		IUtility->SNPrintf(buffer,bufsize,"%02ld:%02ld:%02ld",hours,minutes,secs);
  	}
	else
		{
    days = hours/24;
		IUtility->SNPrintf(buffer,bufsize,"%2ldd %02ldh",days,hours-(days*24));
		}
	return(buffer);
	}

/**************************************************************************************************
 * FUNCTION: TransformUnit()
 *  PURPOSE: Returns given Bytes in a Human-readable format, i.e. 12.4GB
 *    INPUT: bytes					=> Bytes to convert (64)
 *           buffer					=> Buffer to take string
 *           onlyCalculate 	=> TRUE  = Returns only byte string and check for new max. transfer rate
 *                             FALSE = Returns traffic as human-readable string
 *   RETURN: Converted String
 **************************************************************************************************/

STATIC STRPTR TransformUnit(SBQUAD_T *bytes_u,SBQUAD_T *bytes_d,STRPTR buffer, BOOL onlyCalculate)
	{
	char			bc_u[40],bc_d[40];
  long long	result=0;

	IUtility->ClearMem(bc_u,sizeof(bc_u));
	IUtility->ClearMem(bc_d,sizeof(bc_d));

  if(bytes_u->sbq_High)		// > 4.3GB
		{
		convert_quad_to_string(bytes_u,bc_u,sizeof(bc_u));
		}
	else
		{
    IUtility->SNPrintf(bc_u,40,"%lu",bytes_u->sbq_Low);
		}
  if(bytes_d->sbq_High)		// > 4.3GB
		{
		convert_quad_to_string(bytes_d,bc_d,sizeof(bc_d));
		}
	else
		{
    IUtility->SNPrintf(bc_d,40,"%lu",bytes_d->sbq_Low);
		}
  result = atoll(bc_u) + atof(bc_d);
  if(onlyCalculate==TRUE)
		{
    IUtility->SNPrintf(buffer,38,"%lld",(long long)result);
		return(buffer);
		}
	MakeHumanString(result,buffer);
	}

/**************************************************************************************************
 * FUNCTION: MakeHumanString
 *  PURPOSE: Converts bytes to human readable string including unit (kb, MB,GB etc.)
 *    INPUT: bytes 			=> Bytes to convert
 *           retbuffer 	=> Buffer to hold converted string
 *   RETURN: retbuffer
 **************************************************************************************************/

STATIC STRPTR MakeHumanString(long long bytes, STRPTR retbuffer)
	{
	int8	lv = 0;
	char 	*units[] = {"","kB","MB","GB","TB"};

	if(bytes >= 1024)
		{
  	while(bytes >= 1024)
			{
			bytes = bytes / 1024;
    	lv++;
			}
		}
	else
		{
		bytes = bytes / 1024;
		lv = 1;
		}
	if(lv > 4) lv = 4;
	IUtility->SNPrintf(retbuffer,VOL_SIZE,"%#5.2f %s",bytes,units[lv]);
	return(retbuffer);
	}

/**************************************************************************************************
 * FUNCTION: divide_64_by_32()
 *  PURPOSE: Divide a 64 bit integer by a 32 bit integer, filling in a 64 bit quotient
 *           and returning a 32 bit remainder.
 *    INPUT: dividend (64),divisor(32), quotient(64)
 *   RETURN: Remainder (32)
 *    NOTES: Taken from "ShowNetStatus.c" written by Olaf Barthel.
 **************************************************************************************************/

STATIC ULONG divide_64_by_32(SBQUAD_T * dividend,ULONG divisor,SBQUAD_T * quotient)
{
	SBQUAD_T dividend_cdef = (*dividend);
	ULONG dividend_ab = 0;
	LONG i;

	quotient->sbq_High = quotient->sbq_Low = 0;

	for(i = 0 ; i < 64 ; i++)
	{
		/* Shift the quotient left by one bit. */
		quotient->sbq_High = (quotient->sbq_High << 1);

		if((quotient->sbq_Low & 0x80000000UL) != 0)
			quotient->sbq_High |= 1;

		quotient->sbq_Low = (quotient->sbq_Low << 1);

		/* Shift the dividend left by one bit. We start
		 * with the most significant 32 bit portion.
		 */
		dividend_ab = (dividend_ab << 1);

		if((dividend_cdef.sbq_High & 0x80000000UL) != 0)
			dividend_ab |= 1;

		/* Now for the middle 32 bit portion. */
		dividend_cdef.sbq_High = (dividend_cdef.sbq_High << 1);

		if((dividend_cdef.sbq_Low & 0x80000000UL) != 0)
			dividend_cdef.sbq_High |= 1;

		/* Finally, the least significant portion. */
		dividend_cdef.sbq_Low = (dividend_cdef.sbq_Low << 1);

		/* Does the divisor actually divide the dividend? */
		if(dividend_ab >= divisor)
		{
			dividend_ab -= divisor;

			/* We could divide the divisor. Keep track of
			 * this and take care of an overflow condition.
			 */
			quotient->sbq_Low++;
			if(quotient->sbq_Low == 0)
				quotient->sbq_High++;
		}
	}
	return(dividend_ab);
}

/**************************************************************************************************
 * FUNCTION: convert_quad_to_string()
 *  PURPOSE: Convert a 64 bit number into a textual representation, using base=10, just like
 *           sprintf(...,"%lD",...) would do if it could handle multiprecision numbers...
 *    INPUT: number(64)			=> SBQUAD_T Number to convert
 *           string					=> Target String
 *           max_string_len	=> Maximum target string length
 *   RETURN: Converted number as string
 *    NOTES: Taken from "ShowNetStatus.c" written by Olaf Barthel.
 **************************************************************************************************/

STATIC STRPTR convert_quad_to_string(const SBQUAD_T * const number,STRPTR string,LONG max_string_len)
{
	SBQUAD_T q;
	STRPTR s;
	UBYTE c;
	ULONG r;
	LONG i,len;

	/* Make a local copy of the number. */
	q = (*number);

	s = string;

	len = 0;

	/* Build the number string in reverse order, calculating
	 * the single digits:
	 */
	while(len < max_string_len)
	{
		/* Divide by ten and remember the remainder. */
		r = divide_64_by_32(&q,10,&q);

		(*s++) = '0' + r;
		len++;

		/* Stop when we hit zero. */
		if(q.sbq_High == 0 && q.sbq_Low == 0)
			break;
	}

	/* Don't forget to terminate the string. */
	(*s) = '\0';

	/* Reverse the string in place. */
	for(i = 0 ; i < len/2 ; i++)
	{
		c = string[len-1-i];
		string[len-1-i] = string[i];
		string[i] = c;
	}
	return(string);
}
