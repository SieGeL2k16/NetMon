/**************************************************************************************************
 * FILENAME: rsfuncs.c
 *  PURPOSE: RoadShow Support functions
 *  CREATED: 21-Jun-2004
 * MODIFIED: 23-Oct-2004
 *   AUTHOR: SGL
 **************************************************************************************************/

#include "includes.h"
#include "globextvars.h"

#define MILLION	1000000
#define KILO		1000

/**************************************************************************************************
 * Static prototypes:
 **************************************************************************************************/

STATIC STRPTR 	TransformUnit(SBQUAD_T *bytes_u,SBQUAD_T *bytes_d,STRPTR buffer,BOOL onlyCalculate);
STATIC STRPTR 	TransformTime(ULONG seconds, STRPTR buffer);
STATIC STRPTR 	MakeHumanString(double bytes, STRPTR retbuffer);

// Taken from Olaf Barthels Roadshow-Tools

STATIC ULONG 		divide_64_by_32(SBQUAD_T * dividend,ULONG divisor,SBQUAD_T * quotient);
STATIC STRPTR 	convert_quad_to_string(const SBQUAD_T * const number,STRPTR string,LONG max_string_len);

/**************************************************************************************************
 * FUNCTION: RetrieveSocketData()
 *  PURPOSE: Updates internal Memory list with stats from all Interfaces
 *    INPUT: -
 *   RETURN: -
 *    NOTES: Updates internal list and gui at once!
 **************************************************************************************************/

void RetrieveSocketData(void)
	{
	struct		MInterfaces *mi;
	struct		timeval			ltv,current,calctv;
	struct 		sockaddr_in sin;
	struct 		sockaddr_in sin_mask;
	long			state;
	LONG 			bps;
	LONG 			mtu;
	SBQUAD_T	BytesIn 	= {0L,0L};
	SBQUAD_T	BytesOut	= {0L,0L};
	SBQUAD_T	MYNULL 		= {0L,0L};
	char 			volume[80],ontime[80],buf[80],buf1[30],buf2[30];
	STRPTR		IStates[] = {"Offline","Online","Inactive","Active",NULL};
	STRPTR 		bindstr;
	char 			buffer[2*LEN_DATSTRING+30];
	double		Sample,Speed,MaxIn,MaxOut,bin,bout,Stats_CurrSpeed,Stats_SpeedIn,Stats_SpeedOut;
	LONG 			bind_type;
	struct 		DateStamp lease_expire;
	time_t		currtime;

	GetSysTime(&current);
	for(mi=(struct MInterfaces *)InterfaceList->mlh_Head;mi->Node.mln_Succ!=NULL;mi=(struct MInterfaces *)mi->Node.mln_Succ)
		{
		bind_type = IFABT_Unknown;
		mtu = bps = state = 0;
		memcpy(&calctv,&current,sizeof(struct timeval));
		memset(&sin,0,sizeof(sin));
		memset(&sin_mask,0,sizeof(sin_mask));
		memset(&lease_expire,0,sizeof(lease_expire));
		if(QueryInterfaceTags(mi->Name,
				IFQ_GetBytesIn,						&BytesIn,
				IFQ_GetBytesOut,					&BytesOut,
				IFQ_LastStart,						&ltv,
				IFQ_State,								&state,
				IFQ_Address,							&sin,
				IFQ_NetMask,							&sin_mask,
				IFQ_MTU,									&mtu,
				IFQ_BPS,									&bps,
				IFQ_AddressBindType,			&bind_type,
				IFQ_AddressLeaseExpires,	&lease_expire,

		TAG_END) != 0)
			{
			continue;
			}
		SubTime(&calctv,&ltv);
		TransformTime(calctv.tv_sec,ontime);
		TransformUnit(&BytesIn,&BytesOut,volume,FALSE);
    set(mi->TimePtr,MUIA_Text_Contents,ontime);
    set(mi->VolumePtr,MUIA_Text_Contents,volume);
		set(mi->StatePtr,MUIA_Text_Contents,IStates[state]);
		if(bps > MILLION)
			{
			sprintf(ontime,"%ld MBit/sec",bps/MILLION);
			}
		else if(bps > KILO)
			{
      sprintf(ontime,"%ld KBit/sec",bps/KILO);
			}
    else
			{
      sprintf(ontime,"%ld Bit/sec",bps);
			}
		if(bind_type == IFABT_Dynamic)
			{
			if(lease_expire.ds_Days		== 0 && lease_expire.ds_Minute	== 0 && lease_expire.ds_Tick		== 0)
				{
				bindstr = "DHCP - Lease never expires";
				}
			else
				{
				struct DateTime dat;
				char date[LEN_DATSTRING+1];
				char time[LEN_DATSTRING+1];
				LONG len;

				memset(&dat,0,sizeof(dat));
				dat.dat_Stamp	= lease_expire;
				dat.dat_Format	= 4; /* FORMAT_DEF */
				dat.dat_StrDate	= date;
				dat.dat_StrTime	= time;
				DateToStr(&dat);
				len = strlen(date);
				while(len > 0 && date[len-1] == ' ') date[--len] = '\0';
				len = strlen(time);
				while(len > 0 && time[len-1] == ' ') time[--len] = '\0';
				strcpy(buffer,"DHCP - Lease expires at: ");
				strcat(buffer,time);
				strcat(buffer," ");
				strcat(buffer,date);
				bindstr = buffer;
				}
			}
		else
			{
      bindstr = "Static";
			}
		strcpy(volume,Inet_NtoA(sin.sin_addr.s_addr));
		strcpy(buf,Inet_NtoA(sin_mask.sin_addr.s_addr));
    sprintf(mi->NameBubble,"IP-addr: %s/%s\nSpeed: %s\nMTU: %ld Bytes\nBindtype: %s",volume,buf,ontime,mtu,bindstr);

		// V0.2: Display traffic broken down to up / down as Bubble, too:

		memset(volume,'\0',sizeof(volume));
		memset(buf,'\0',sizeof(buf));
 		TransformUnit(&BytesIn,&MYNULL,volume,FALSE); 		// Our total bytes downloaded.
 		TransformUnit(&BytesOut,&MYNULL,buf,FALSE); 			// Our total bytes uploaded.
    sprintf(mi->VolumeBubble,"\33rIN: %s\nOUT: %s",volume,buf);

		// Now finally calculate current transfer speed for this interface:

		Sample = atof(TransformUnit(&BytesIn,&BytesOut,volume,TRUE)); 		// Our total bytes, current value.
		MaxIn  = atof(TransformUnit(&BytesIn,&MYNULL,buf1,TRUE));					// Bytes In, current value
		MaxOut = atof(TransformUnit(&BytesOut,&MYNULL,buf2,TRUE));				// Bytes out, current value
		bin    = MaxIn;
		bout	 = MaxOut;

		if(!mi->FirstSample)	    // Must be first call, remember value and skip calc:
			{
			mi->FirstSample = Sample;
      mi->FirstMaxIn  = MaxIn;
			mi->FirstMaxOut = MaxOut;
			mi->MaxIn				= 0.00;
			mi->MaxOut      = 0.00;
			set(mi->SpeedPtr,MUIA_Text_Contents,"0.00 kB/s");
			return;
			}

    // We have a sample and know our Interval, lets try to math a bit :)

		mi->CurrentSample = Sample;
		mi->CurrentMaxIn 	= MaxIn;
		mi->CurrentMaxOut	= MaxOut;
		Sample 	= mi->CurrentSample - mi->FirstSample;		// Get Bytes transfered in "INTERVAL" time
		MaxIn		= mi->CurrentMaxIn - mi->FirstMaxIn;
		MaxOut  = mi->CurrentMaxOut - mi->FirstMaxOut;
		mi->FirstSample = mi->CurrentSample;						// Add current sample back for next run
    mi->FirstMaxIn	= mi->CurrentMaxIn;
		mi->FirstMaxOut	= mi->CurrentMaxOut;
		if(Sample)
			{
			Speed = (Sample / 1024) / RefreshInterval;
			}
    else
      {
			Speed = 0.00;
			}
		Stats_CurrSpeed = Speed;
		sprintf(volume, "%#04.2f kB/s",Speed);
		set(mi->SpeedPtr,MUIA_Text_Contents,volume);
    if(MaxIn)
			{
      Speed = (MaxIn / 1024) / RefreshInterval;
			}
		else
      {
			Speed = 0.00;
			}
		Stats_SpeedIn = Speed;
		sprintf(mi->CurrentInUnit,"%#04.2f kb/s",Speed);
		if(Speed > mi->MaxIn)
			{
			sprintf(mi->MaxInUnit,"%#04.2f kb/s",Speed);
			mi->MaxIn = Speed;
			}
		else
      {
			sprintf(mi->MaxInUnit,"%#04.2f kb/s",mi->MaxIn);
			}
    if(MaxOut)
			{
      Speed = (MaxOut / 1024) / RefreshInterval;
			}
		else
      {
			Speed = 0.00;
			}
		Stats_SpeedOut = Speed;
		sprintf(mi->CurrentOutUnit,"%#04.2f kb/s",Speed);
		if(Speed > mi->MaxOut)
			{
			sprintf(mi->MaxOutUnit,"%#04.2f kb/s",Speed);
			mi->MaxOut = Speed;
			}
		else
      {
			sprintf(mi->MaxOutUnit,"%#04.2f kb/s",mi->MaxOut);
			}
		sprintf(mi->SpeedRecord,"\33rCurrent IN: %s\nCurrent OUT: %s\n\nMaximum IN: %s\nMaximum OUT: %s",mi->CurrentInUnit,mi->CurrentOutUnit,mi->MaxInUnit,mi->MaxOutUnit);

		/**************************************************************************************************
 		 * Check if we should write down these stats to as Env-variable to ENV:
 		 * Filename format is: NetMon_Stats.%s
 		 * Format of written var is (separated by colons):
     * 1. TimeStamp
     * 2. Total Bytes in/out
     * 3. Total speed in kb/s
     * 4. Bytes In
     * 5. Speed in (current)
     * 6. Bytes Out
     * 7. Speed out (current)
     **************************************************************************************************/

		if(WriteStats == TRUE)
			{
			char	fname[128];

			memset(tempbuf1,'\0',TEMPBUFSIZE);
			currtime = time(&currtime);
			sprintf(tempbuf1,"%lu:%0.f:%#04.2f:%.0f:%#04.2f:%.0f:%#04.2f:%#04.2f:%#04.2f\n",
        currtime,
        (bin+bout),
        Stats_CurrSpeed,
        bin,
        Stats_SpeedIn,
        bout,
        Stats_SpeedOut,
        mi->MaxIn,
        mi->MaxOut
        );
			sprintf(fname,"NetMon_Stats.%s",mi->Name);
			SetVar(fname,tempbuf1,strlen(tempbuf1),GVF_GLOBAL_ONLY);
			}
		}
	}

/**************************************************************************************************
 * FUNCTION: TransformTime()
 *  PURPOSE: Returns given seconds in a Human-readable format: HH:MM.SS
 *    INPUT: seconds	=> Seconds to convert
 *           buffer		=> Buffer to take string
 *   RETURN: Converted String
 **************************************************************************************************/

STATIC STRPTR TransformTime(ULONG seconds, STRPTR buffer )
	{
  long	hours,minutes, secs, days;

	hours	= seconds / 3600;
	if((seconds / 60) >= 60) minutes = (seconds - ((floor((double)seconds / 3600) * 3600))) / 60;
	else minutes = seconds / 60;
	secs = seconds % 60;
	if(hours < 48)
		{
		sprintf(buffer,"%02ld:%02ld:%02ld",hours,minutes,secs);
  	}
	else
		{
    days = hours/24;
		sprintf(buffer,"%2ldd %02ldh",days,hours-(days*24));
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
  double		result=0;

	memset(bc_u,'\0',sizeof(bc_u));
	memset(bc_d,'\0',sizeof(bc_d));

  if(bytes_u->sbq_High)		// > 4.3GB
		{
		convert_quad_to_string(bytes_u,bc_u,sizeof(bc_u));
		}
	else
		{
    sprintf(bc_u,"%lu",bytes_u->sbq_Low);
		}
  if(bytes_d->sbq_High)		// > 4.3GB
		{
		convert_quad_to_string(bytes_d,bc_d,sizeof(bc_d));
		}
	else
		{
    sprintf(bc_d,"%lu",bytes_d->sbq_Low);
		}
  result = atof(bc_u) + atof(bc_d);
  if(onlyCalculate==TRUE)
		{
    sprintf(buffer,"%f",result);
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

STATIC STRPTR MakeHumanString(double bytes, STRPTR retbuffer)
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
	sprintf(retbuffer,"%#5.2f %s",bytes,units[lv]);
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
