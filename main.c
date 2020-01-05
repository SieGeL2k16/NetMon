/*****************************************************************************************
 * NetMon - Network Monitor for Roadshow
 * Based on sample from Olaf 'Ohlsen' Barthel
 * Written by Sascha 'SieGeL' Pfalz
 * $Id: main.c,v 1.3 2007/02/24 23:33:58 siegel Exp $
 *****************************************************************************************/

#include "includes.h"

/*****************************************************************************************
 * Global Variables:
 *****************************************************************************************/

struct 	Library 				*MUIMasterBase 			= NULL;
struct 	Library					*SocketBase 				= NULL;
struct	Device					*TimerBase					= NULL;

#ifdef __amigaos4__

#define DoMethod IDoMethod

struct  Library         *IntuitionBase 	    = NULL;
struct 	MUIMasterIFace 	*IMUIMaster 				= NULL;
struct  IntuitionIFace 	*IIntuition 				= NULL;
struct  SocketIFace    	*ISocket 	          = NULL;
struct	TimerIFace      *ITimer       	    = NULL;

#else

struct	IntuitionBase				*IntuitionBase	= NULL;
#define PutErrStr(x) FPuts(Output(),x)

LONG __stack = 10240L;               // MUI V3.x ABSOLUTLY REQUIRE THIS UNDER 68K!

#endif

// Timer related structs:

struct	MsgPort			*TimerMP		= NULL;
struct	timerequest *TimerIO 		= NULL;

ULONG 	sigs = 0;
ULONG		timersigs = 0;
BOOL 		running = TRUE,     								// Main Loop var
				WinGadgets = TRUE,									// Close/Size/Depth gadget display
				WriteStats = FALSE;									// If NetMon should dump out stats every interval
APTR		app = NULL,
				mainwindow=NULL,strip,
				aboutwin=NULL,
				basegroup = NULL;
APTR		mempool = NULL;											// Memory pool allocated by AllocSysObject()
struct 	List *interface_list;								// List of available interfaces
struct	MinList *InterfaceList;
long		RefreshInterval = 5;								// Defaults to 5 seconds
char		*tempbuf1 = NULL;

/**************************************************************************************************
 * Prototypes:
 **************************************************************************************************/

STATIC void ShowAbout(void);								// Displays the About Window
STATIC void RemoveAboutWin(void);     			// Removes About Window
STATIC void	OpenLibs(void);									// Opens all required resources
STATIC void CloseLibs(void);                // Removes all resources
STATIC void ReadOptions(char **argv);		    // Tries to read options from either Commandline or Icon
STRPTR GetRoadshowVersion(STRPTR buffer);	  // Reads TCP/IP Stack informations
APTR   MyWinStyle(BOOL how);                // Renders main frame either with or without text

#ifndef __amigaos4__
void __autoopenfail(void) { _XCEXIT(0);}		// Dummy function for SAS
#endif

/**************************************************************************************************
 * FUNCTION: main()
 *  PURPOSE: Entry Point of application
 *    INPUT: argc		=> Argument counter
 *           argv => Array of arguments
 *   RETURN: Exit Code
 *    NOTES: Arguments are ignored, if they are required ReadArgs() is used.
 **************************************************************************************************/

int main(int argc, char **argv)
	{
	struct 	Node *node;
	struct	MInterfaces *in, *mi;
	ULONG		time_mask;
	long		status;

	OpenLibs();
	ReadOptions(argv);
	for(node = interface_list->lh_Head ; node->ln_Succ != NULL ; node = node->ln_Succ)
	  {
    in = AllocPooled(mempool,sizeof(struct MInterfaces));
		if(!in)
		  {
			fail(NULL,"Cannot allocate Memory for Node?",TRUE);
			}
    strcpy(in->Name,node->ln_Name);
		in->FirstSample = in->CurrentSample = 0.00;
		strcpy(in->NameBubble, "IP: 0.0.0.0");
		strcpy(in->VolumeBubble,"IN: -\nOUT: -");
		strcpy(in->SpeedRecord,"Max. IN: -\nMax. OUT: -");
		AddTail((struct List *)InterfaceList, (struct Node *)in);
		}
	app = ApplicationObject,
				MUIA_Application_Title      , "NetMon",
				MUIA_Application_Version    , "$VER: "VERS" ("DATE")",
				MUIA_Application_Copyright  , "© 2004,2007 by Sascha 'SieGeL' Pfalz",
				MUIA_Application_Author     , "Sascha 'SieGeL' Pfalz",
				MUIA_Application_Description, "Network Monitor for RoadShow ["CPU_TYPE"]",
				MUIA_Application_Base       , "NetMon",
 				MUIA_Application_Menustrip	, strip = MenustripObject,
					MUIA_Family_Child					,  MenuObjectT("Project"),
			  	  MUIA_Family_Child				,  MenuitemObject,
			  	    MUIA_Menuitem_Title     , "About",
				      MUIA_Menuitem_Shortcut  , "?",
					    MUIA_UserData						,	ID_ABOUT,
			      End,
  		  	  MUIA_Family_Child					, MenuitemObject,
					    MUIA_Menuitem_Title			, NM_BARLABEL,
						End,
				  	MUIA_Family_Child					, MenuitemObject,
							MUIA_Menuitem_Title     , "Quit",
						  MUIA_Menuitem_Shortcut  , "Q",
							MUIA_UserData						,	MUIV_Application_ReturnID_Quit,
						End,
		      End,
					MUIA_Family_Child						, MenuObjectT("MUI"),
				  	MUIA_Family_Child					, MenuitemObject,
					  	MUIA_Menuitem_Title     , "About...",
						  MUIA_Menuitem_Shortcut  , "!",
							MUIA_UserData						,	ID_ABOUT_MUI,
					  End,
				  	MUIA_Family_Child					, MenuitemObject,
					  	MUIA_Menuitem_Title     , "Settings...",
						  MUIA_Menuitem_Shortcut  , "M",
							MUIA_UserData						,	ID_CONFIG_MUI,
					  End,
					End,
				End,
				SubWindow, mainwindow = WindowObject,
				MUIA_Window_ID         	, MAKE_ID('M','A','I','N'),
				MUIA_Window_ScreenTitle	, VERS" ["CPU_TYPE"] by Sascha 'SieGeL' Pfalz",
				MUIA_Window_SizeRight		, TRUE,
				MUIA_Window_CloseGadget , WinGadgets,
				MUIA_Window_DepthGadget , WinGadgets,
				MUIA_Window_DragBar 		, WinGadgets,
				MUIA_Window_SizeGadget	, WinGadgets,

				WindowContents, VGroup,
          Child, basegroup = MyWinStyle(WinGadgets),
			 End,
  	  End,
		End;
		if(!app)
		  {
			fail(NULL,"Cannot create main MUI window!",TRUE);
			}
		DoMethod(mainwindow,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,app,2,MUIM_Application_ReturnID,MUIV_Application_ReturnID_Quit);
		DoMethod(basegroup,MUIM_Group_InitChange);

		// Add the current socket data to the window:

		for(mi=(struct MInterfaces *)InterfaceList->mlh_Head;mi->Node.mln_Succ!=NULL;mi=(struct MInterfaces *)mi->Node.mln_Succ)
			{
			mi->NamePtr 		= TextObject, MUIA_Text_PreParse,"\33l", MUIA_Text_Contents,mi->Name,End;
			mi->StatePtr 		= TextObject, MUIA_Text_PreParse,"\33c", MUIA_Text_Contents,"---",End;
			mi->SpeedPtr		= TextObject, MUIA_Text_PreParse,"\33r", MUIA_Text_Contents,"---",End;
			mi->VolumePtr		= TextObject, MUIA_Text_PreParse,"\33r", MUIA_Text_Contents,"---",End;
			mi->TimePtr			= TextObject, MUIA_Text_PreParse,"\33r", MUIA_Text_Contents,"--:--:--",End;
			set(mi->NamePtr,MUIA_ShortHelp,mi->NameBubble);
			set(mi->VolumePtr,MUIA_ShortHelp,mi->VolumeBubble);
			set(mi->SpeedPtr, MUIA_ShortHelp,mi->SpeedRecord);
			DoMethod(basegroup,OM_ADDMEMBER,mi->NamePtr);
			DoMethod(basegroup,OM_ADDMEMBER,mi->StatePtr);
			DoMethod(basegroup,OM_ADDMEMBER,mi->SpeedPtr);
			DoMethod(basegroup,OM_ADDMEMBER,mi->VolumePtr);
			DoMethod(basegroup,OM_ADDMEMBER,mi->TimePtr);
			}
		DoMethod(basegroup,MUIM_Group_ExitChange);
		if(WinGadgets == TRUE)	set(mainwindow,MUIA_Window_Title, VERS" ("DATE")");
    set(mainwindow,MUIA_Window_Open,TRUE);
		get(mainwindow,MUIA_Window_Open,&status);
		if(!status)
			{
			fail(NULL,"Unable to create Main Window!!",TRUE);
			}
		time_mask = PORT_SIG_MASK(TimerMP);
    RetrieveSocketData();
		TimerIO->tr_node.io_Command	= TR_ADDREQUEST;

#ifdef __NEWLIB_H__
		TimerIO->tr_time.tv_sec	    = RefreshInterval;
		TimerIO->tr_time.tv_usec		= 0;
#else
		TimerIO->tr_time.tv_secs		=	RefreshInterval;
		TimerIO->tr_time.tv_micro		= 0;
#endif

		SendIO((struct IORequest *)TimerIO);
    while (running)
			{
      switch(DoMethod(app,MUIM_Application_Input,&sigs))
			  {
        case 	MUIV_Application_ReturnID_Quit:
         			running = FALSE;
         			break;

				case  ID_ABOUT:
						  ShowAbout();
							break;

				case  ID_ABOUTCLOSE:
							RemoveAboutWin();
							break;

				case  ID_ABOUT_MUI:
							if(!aboutwin)
  	            {
								aboutwin= AboutmuiObject,
									MUIA_Aboutmui_Application,app,
          	    End;
								}
							if(aboutwin) set(aboutwin,MUIA_Window_Open,TRUE);
							else fail(app,"Unable to open MUI About window",TRUE);
							break;

				case  ID_CONFIG_MUI:
							DoMethod(app,MUIM_Application_OpenConfigWindow,0L);
		          break;
				}
      if (running && sigs)
			  {
				sigs = Wait(sigs|time_mask|SIGBREAKF_CTRL_C);
				}
			if(sigs & time_mask)
				{
				WaitIO((struct IORequest *)TimerIO);
				RetrieveSocketData();
				TimerIO->tr_node.io_Command	= TR_ADDREQUEST;
#ifdef __NEWLIB_H__
				TimerIO->tr_time.tv_sec	    = RefreshInterval;
				TimerIO->tr_time.tv_usec		= 0;
#else
				TimerIO->tr_time.tv_secs		= RefreshInterval;
				TimerIO->tr_time.tv_micro		= 0;
#endif
				SendIO((struct IORequest *)TimerIO);
				}
			if(sigs & SIGBREAKF_CTRL_C) running = FALSE;
			}
		if(!CheckIO((struct IORequest *)TimerIO)) AbortIO((struct IORequest *)TimerIO);
		WaitIO((struct IORequest *)TimerIO);
 		set(mainwindow,MUIA_Window_Open,FALSE);
 		MUI_DisposeObject(app);
		CloseLibs();
    exit(RETURN_OK);
		}
/**************************************************************************************************
 * FUNCTION: MyWinStyle()
 *  PURPOSE: Decides what kind of Frame Border the window should use.
 *    INPUT: TRUE  => Full Textframe
 *           FALSE => Frame without text
 *   RETURN: APTR Pointer to group
 **************************************************************************************************/

APTR MyWinStyle(BOOL how)
  {
 if(how == TRUE)
    {
    return(VGroup,GroupFrameT("Available Interfaces:"),MUIA_Group_Columns, 5,End);
    }
  else
    {
    return(VGroup,GroupFrame,MUIA_Group_Columns, 5, End);
    }
  }


/**************************************************************************************************
 * FUNCTION: fail()
 *  PURPOSE: General failure function. Uses EasyRequest for warning infos if app is not loaded yet
 *    INPUT: app      => Application Pointer
 *           *str	    => Error string
 *					 withexit	=> TRUE = Exit after displaying message. FALSE = do not exit application
 *   RETURN: none
 * REQUIRES: Open intuition.library!
 **************************************************************************************************/

void fail(APTR thisapp,char *str,BOOL withexit)
	{
  if (*str)
    {
		if(thisapp) MUI_Request(app, mainwindow, 0,VERS" Warning:","Okay!","\33c%s", str);
		else
		  {
  		struct EasyStruct MyES=
			  {
				sizeof(struct EasyStruct),
				0,
				NULL,
        "%s",
        "Okay",
				};
     	EasyRequest(NULL,&MyES,NULL,str);
			}
		printf("\nERROR: %s\n",str);
  	}
	if(withexit==FALSE) return;
	CloseLibs();
	exit(RETURN_FAIL);
  }

/**************************************************************************************************
 * FUNCTION: OpenLibs()
 *  PURPOSE: Opens all required resources (Libraries, Interfaces, Memory)
 *    INPUT: none
 *   RETURN: none
 *    NOTES: In case of any error this function exists application with appropiate error message!
 **************************************************************************************************/

STATIC void OpenLibs(void)
		{
  ULONG have_ifaces=FALSE;
		char		buffer[256],rsversion[128];

#ifdef __amigaos4__

		if(!(DOSBase = OpenLibrary("dos.library",36)))
				{
				printf("ERROR: Cannot open dos.library v36+!\n");
				exit(RETURN_FAIL);
    }
		if(!(IDOS = (struct DOSIFace *)GetInterface(DOSBase, "main",1,NULL)))
				{
    printf("ERROR: Cannot obtain DOS Interface!\n");
				CloseLibs();
				exit(RETURN_FAIL);
				}
  if (!(IntuitionBase=OpenLibrary("intuition.library",39)))
				{
    PutErrStr("ERROR: Cannot open intuition.library V39+!\n");
    CloseLibs();
				exit(RETURN_FAIL);
				}
  if((IIntuition = (struct IntuitionIFace *)GetInterface(IntuitionBase, "main", 1, NULL))==0)
				{
    PutErrStr("ERROR: Cannot obtain Intuition Interface!\n");
				CloseLibs();
				exit(RETURN_FAIL);
				}
  if (!(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,19)) )
				{
    fail(NULL,"Cannot open MUIMaster.library V19+!",TRUE);
				}
  if((IMUIMaster = (struct MUIMasterIFace *)GetInterface(MUIMasterBase, "main", 1, NULL))==0)
				{
   	fail(NULL,"Cannot obtain MUIMaster Interface!",TRUE);
				}
  if (!(SocketBase=OpenLibrary("bsdsocket.library",4)) )
				{
    fail(NULL,"Cannot open bsdsocket.library V4+!",TRUE);
				}
  if((ISocket = (struct SocketIFace *)GetInterface(SocketBase, "main", 1, NULL))==0)
				{
   	fail(NULL,"Cannot obtain Socket Interface!",TRUE);
				}
		if(SocketBaseTags(SBTM_GETREF(SBTC_HAVE_INTERFACE_API),&have_ifaces, TAG_END)!=0)
  	{
				sprintf(buffer,"Cannot query TCP/IP stack for featurelist?\n\nUsing %s",GetRoadshowVersion(rsversion));
				fail(NULL,buffer,TRUE);
				}
		if(have_ifaces == FALSE)
				{
				sprintf(buffer,"This TCP/IP stack seems not to be Roadshow!\n\nDetected: %s",GetRoadshowVersion(rsversion));
				fail(NULL,buffer,TRUE);
    }
		if(!(mempool = AllocSysObjectTags(ASOT_MEMPOOL, ASOPOOL_Puddle, 20480, ASOPOOL_Threshold, 20480, TAG_DONE)))
				{
   	fail(NULL,"Cannot create memory pool?!",TRUE);
				}
		interface_list = ObtainInterfaceList();
		if(interface_list == NULL || (IsListEmpty(interface_list)))
				{
    fail(NULL,"No configured Interfaces found?\nPlease start Roadshow first.",TRUE);
				}
		InterfaceList = (struct MinList *)AllocSysObjectTags(ASOT_LIST, ASOLIST_Min, TRUE, TAG_DONE);
		if(!InterfaceList)
				{
				fail(NULL,"No memory left for Interface list??",TRUE);
				}
		TimerMP = AllocSysObject(ASOT_PORT,NULL);
		if(!TimerMP)
				{
				fail(NULL,"Cannot create Timer message port!",TRUE);
				}
		TimerIO = AllocSysObjectTags(ASOT_IOREQUEST,ASOIOR_Size,sizeof(struct timerequest),ASOIOR_ReplyPort,TimerMP,TAG_DONE);
		if(!TimerIO)
				{
				fail(NULL,"Cannot allocate timer request!",TRUE);
				}
		if(OpenDevice(TIMERNAME,UNIT_MICROHZ, (struct IORequest *)TimerIO, 0))
				{
				fail(NULL,"Cannot open timer.device!",TRUE);
				}
		TimerBase = (struct Device *)TimerIO->tr_node.io_Device;
		ITimer = (struct TimerIFace *)GetInterface((struct Library *)TimerBase,"main",1,NULL);
		if(!ITimer)
				{
    fail(NULL,"Cannot obtain Timer Interface!",TRUE);
				}

#else

		if(!(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",39L)))
				{
    PutErrStr("Cannot Open intuition.library?!");
				exit(20);
				}
		if(!(MUIMasterBase=OpenLibrary(MUIMASTER_NAME,MUIMASTER_VMIN)))
				{
				fail(NULL,"Failed to open "MUIMASTER_NAME" V12++ !",TRUE);
				}
 		if(!(mempool = CreatePool(MEMF_PUBLIC|MEMF_CLEAR,20480L, 20480L)))
				{
   	fail(NULL,"Cannot create memory pool?!",TRUE);
				}
		if(!(InterfaceList = AllocPooled(mempool,sizeof(struct MinList))))
				{
				fail(NULL,"Cannot allocate memory for interface list??",TRUE);
				}
		NewList((struct List *)InterfaceList);
  if (!(SocketBase=OpenLibrary("bsdsocket.library",4)) )
				{
    fail(NULL,"Cannot open bsdsocket.library V4+!",TRUE);
				}
		if(SocketBaseTags(SBTM_GETREF(SBTC_HAVE_INTERFACE_API),&have_ifaces, TAG_END)!=0)
  	{
				sprintf(buffer,"Cannot query TCP/IP stack for featurelist?\n\nUsing %s",GetRoadshowVersion(rsversion));
				fail(NULL,buffer,TRUE);
				}
		if(have_ifaces == FALSE)
				{
				sprintf(buffer,"This TCP/IP stack seems not to be Roadshow!\n\nFound: %s",GetRoadshowVersion(rsversion));
				fail(NULL,buffer,TRUE);
    }
		interface_list = ObtainInterfaceList();
		if(interface_list == NULL || (IsListEmpty(interface_list)))
				{
    fail(NULL,"No configured Interfaces found?\nPlease start Roadshow first!",TRUE);
				}
		TimerMP = CreateMsgPort();
		if(!TimerMP)
				{
				fail(NULL,"Cannot create Timer message port!",TRUE);
				}
		TimerIO = (struct timerequest *)CreateIORequest(TimerMP,sizeof(*TimerIO));
		if(!TimerIO)
				{
				fail(NULL,"Cannot allocate timer request!",TRUE);
				}
		if(OpenDevice(TIMERNAME,UNIT_MICROHZ, (struct IORequest *)TimerIO, 0))
				{
				fail(NULL,"Cannot open timer.device!",TRUE);
				}
		TimerBase = (struct Device *)TimerIO->tr_node.io_Device;

#endif

		if(!(tempbuf1 = AllocPooled(mempool,TEMPBUFSIZE)))
    {
				fail(NULL,"Cannot allocate 1k for buffers?",TRUE);
				}
		}

/**************************************************************************************************
 * FUNCTION: CloseLibs()
 *  PURPOSE: Frees all allocated resources (Libraries, Interfaces, Memory)
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void CloseLibs(void)
		{
#ifdef __amigaos4__
		if(ITimer)																	DropInterface((struct Interface *)ITimer);
  CloseDevice((struct IORequest *)TimerIO);
		if(TimerIO!=NULL)												FreeSysObject(ASOT_IOREQUEST,TimerIO);
		if(TimerMP!=NULL)												FreeSysObject(ASOT_PORT,TimerMP);
		if(interface_list != NULL) 	ReleaseInterfaceList(interface_list);
		if(InterfaceList!=NULL)					FreeSysObject(ASOT_LIST,(APTR)InterfaceList);
		if(mempool!=NULL)												FreeSysObject(ASOT_MEMPOOL, (APTR)mempool);
  if(ISocket!=NULL)       			DropInterface((struct Interface *)ISocket);
		if(IMUIMaster!=NULL) 								DropInterface((struct Interface *)IMUIMaster);
		if(IIntuition!=NULL) 								DropInterface((struct Interface *)IIntuition);
		if(IDOS!=NULL) 													DropInterface((struct Interface *)IDOS);
		if(SocketBase!=NULL)								CloseLibrary(SocketBase);
		if(MUIMasterBase!=NULL) 				CloseLibrary(MUIMasterBase);
		if(IntuitionBase!=NULL) 				CloseLibrary(IntuitionBase);
		if(DOSBase!=NULL) 									CloseLibrary(DOSBase);

#else

		if(TimerIO!=NULL)
		  {
				if(TimerIO->tr_node.io_Device != NULL) CloseDevice((struct IORequest *)TimerIO);
				DeleteIORequest((struct IORequest *)TimerIO);
				TimerIO = NULL;
		  }
		if(TimerMP != NULL) 								DeleteMsgPort(TimerMP);
		if(interface_list != NULL) 	ReleaseInterfaceList(interface_list);
		if(mempool!=NULL)												DeletePool(mempool);
		if(SocketBase!=NULL)								CloseLibrary(SocketBase);
		if(MUIMasterBase!=NULL) 				CloseLibrary(MUIMasterBase);
		if(IntuitionBase!=NULL) 				CloseLibrary((struct Library *)IntuitionBase);

#endif
		}

/**************************************************************************************************
 * FUNCTION: ShowAbout()
 *  PURPOSE: Opens the well-known About dialog :)
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void ShowAbout(void)
		{
		APTR 		titeltext;
		LONG		status;
		char		titlebuf[100],rsversion[256];

		sprintf(titlebuf,"About %s [%s]",VERS,CPU_TYPE);
		aboutwin = WindowObject,
				MUIA_Window_Title						,		titlebuf,
				MUIA_Window_NoMenus			,		TRUE,
				MUIA_Window_SizeGadget, FALSE,
				WindowContents, VGroup,
						Child, HGroup,
								Child, titeltext = TextObject, End,
						End,
				End,
		End;
		if(!aboutwin) fail(app, "Cannot open about window!",TRUE);
		GetRoadshowVersion(rsversion);
		DoMethod(aboutwin,MUIM_Notify,MUIA_Window_CloseRequest,TRUE,app,2,MUIM_Application_ReturnID,ID_ABOUTCLOSE);
		DoMethod(titeltext,MUIM_SetAsString,MUIA_Text_Contents,"\33c\33b%s (%s) [%s]\33n\n\nNetwork Monitor for Roadshow TCP/IP Stack\nwritten and (c) 2004,2007 by Sascha 'SieGeL' Pfalz\n\nUses codeparts from RoadShow examples written by Olaf 'Ohlsen' Barthel.\n\nBig thanks to Olaf for his great help and support and\nto Hyperion for making AmigaOS 4 finally possible!\n\nVisit http://www.saschapfalz.de for updates\n\n%s",VERS,DATE,CPU_TYPE,rsversion);
		DoMethod(app, OM_ADDMEMBER, aboutwin);
		set(app,MUIA_Application_Sleep,TRUE);
 		set(aboutwin,MUIA_Window_Open,TRUE);
		get(aboutwin,MUIA_Window_Open,&status);
		if(!status) fail(app,"Cannot open about window!!",TRUE);
		}

/**************************************************************************************************
 * FUNCTION: RemoveAboutWin()
 *  PURPOSE: Closes the About window
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void RemoveAboutWin(void)
		{
		set(aboutwin,MUIA_Window_Open,FALSE);
		DoMethod(app,OM_REMMEMBER, aboutwin);
		MUI_DisposeObject(aboutwin);aboutwin=NULL;
  set(app,MUIA_Application_Sleep,FALSE);
		aboutwin = NULL;
		}

/**************************************************************************************************
 * FUNCTION: ReadOptions()
 *  PURPOSE: Tries to read Config options from either Commandline or Icon. Commandline has precedence
 *    INPUT: none
 *   RETURN: none
 **************************************************************************************************/

STATIC void ReadOptions(char **argv)
	{
  struct 	RDArgs *rda;
  struct  Process *pr;
  STRPTR	arg_template = "NOWINBORDER/S,WRITESTATS/S,INTERVAL/K/N";
	STRPTR  ttcheck;
	LONG    ArgArray[3] = {0L,0L,0L};
	long		check,check2 = 5;
	struct	Library	*IconBase;
	struct	WBStartup *wst;
	struct 	WBArg *wba;

#ifdef __amigaos4__

  struct		IconIFace *IIcon 		= NULL;

#endif

	pr = (struct Process *)FindTask(NULL);
	if(pr->pr_CLI == ZERO)
		{
		char		fname[256];
    BPTR		myfile = GetProgramDir();
		if(myfile)
			{
			memset(tempbuf1,'\0',TEMPBUFSIZE);
      NameFromLock(myfile,tempbuf1,255);
			wst = (struct WBStartup *)argv;
			if(wst != NULL)
				{
				wba = &wst->sm_ArgList[0];
				strcpy(fname,wba->wa_Name);
				}
			else strcpy(fname,"NetMon");
      AddPart(tempbuf1,fname,255);
			if(IconBase = OpenLibrary("icon.library",0L))
				{
#ifdef __amigaos4__
        if(IIcon = (struct IconIFace *)GetInterface(IconBase, "main", 1, NULL))
					{
#endif
					struct	DiskObject *dobj;
          dobj = GetDiskObject(tempbuf1);
					if(dobj)
						{
						if(FindToolType((STRPTR *)dobj->do_ToolTypes,"NOWINBORDER")) WinGadgets = FALSE;
						else WinGadgets = TRUE;
						if(FindToolType((STRPTR *)dobj->do_ToolTypes,"WRITESTATS")) WriteStats = TRUE;
						else WriteStats = FALSE;
						ttcheck = FindToolType((STRPTR *)dobj->do_ToolTypes,"INTERVAL");
						if(ttcheck != NULL) RefreshInterval = atoi(ttcheck);
						if(RefreshInterval < 1) RefreshInterval = 5;
						FreeDiskObject(dobj);
						}
#ifdef __amigaos4__
					DropInterface((struct Interface *)IIcon);
					}
#endif
        CloseLibrary(IconBase);
				}
			}
		}
	else
		{
		rda=ReadArgs(arg_template,ArgArray,NULL);
		check = ArgArray[0];
		if(check)
			{
			WinGadgets 	= FALSE;
			}
		if(ArgArray[2]) check2 = *(long *)ArgArray[2];
		check = ArgArray[1];
		if(check)
			{
			WriteStats  = TRUE;
			}
		FreeArgs(rda);
		if(check2 < 1) check2 = 5;
		RefreshInterval = check2;
		}
	}

/**************************************************************************************************
 * FUNCTION: GetRoadshowVersion()
 *  PURPOSE: Reads Version string from TCP/IP stack
 *    INPUT: buffer to hold version string
 *   RETURN: none
 *    NOTES: Taken from GetNetStatus.c written by Olaf Barthel
 **************************************************************************************************/

STRPTR GetRoadshowVersion(STRPTR buffer)
		{
		char string[256],final[512],buf[128];
		LONG len;
		
		memset(string,'\0',sizeof(string));
		memset(final,'\0',sizeof(final));
		memset(buf,'\0',sizeof(buf));
		if(SocketBase->lib_IdString != NULL)
				{
				strncpy(string,SocketBase->lib_IdString,sizeof(string)-1);
				string[sizeof(string)-1] = '\0';
				}
		else
				{
				strcpy(string,"");
				}
		len = strlen(string);
		while(len > 0 && (string[len-1] == '\r' || string[len-1] == '\n')) string[--len] = '\0';
		if(len > 0)
				{
				STRPTR release_string = NULL;
				BOOL need_line_feed = FALSE;
				LONG i;
				for(i = 0 ; i < len ; i++)
						{
						if(string[i] == '\n')
								{
								need_line_feed = TRUE;
								break;
								}
						}
    strcpy(final,string);
				if(SocketBaseTags(SBTM_GETREF(SBTC_RELEASESTRPTR),&release_string,TAG_END) != 0)
						{
						release_string = NULL;
						}
				if(release_string != NULL)
						{
						strncpy(string,release_string,sizeof(string)-1);
						string[sizeof(string)-1] = '\0';
						}
				else
						{
						strcpy(string,"");
						}
				len = strlen(string);
				while(len > 0 && (string[len-1] == '\r' || string[len-1] == '\n')) string[--len] = '\0';
				if(len > 0)
						{
						if(need_line_feed) strcat(final,"\n");
						else strcat(final," ");
						sprintf(buf,"[%s]",string);
						strcat(final,buf);
						}
				strcpy(buffer,final);
				}
		return(buffer);
		}
