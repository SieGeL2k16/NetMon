/**************************************************************************************************
 * FILENAME: global_defines.h
 *  PURPOSE: All defines used in NetMon
 *  CREATED: 21-Jun-2004
 * MODIFIED: 22-Oct-2004
 *   AUTHOR: SGL
 **************************************************************************************************/

#define MAKE_ID(a,b,c,d) ((ULONG) (a)<<24 | (ULONG) (b)<<16 | (ULONG) (c)<<8 | (ULONG) (d))
#define ID_CONFIGSAVE		1										/* "Save" Button */
#define ID_ABOUT 				2										/* "About" menu entry */
#define ID_ABOUT_MUI		3										/* "About MUI..." menu entry */
#define ID_ABOUTCLOSE		4                   /* "Cancel" button/CloseWindow/ESC */
#define ID_UPDATE				5										/* "Update Task Monitor" menu entry */
#define ID_CONFIG_MUI		6										/* MUI Settings */
#define ID_CONFIG				7										/* Configure options */
#define ID_CONFIGQUIT		8										/* Close config window */
#define ID_DOUBLESTART 	666									/* To recognize double app start */
#define PORT_SIG_MASK(mp) (1UL << (mp)->mp_SigBit)

#ifndef ZERO
#define ZERO NULL
#endif

#define TEMPBUFSIZE	1024
