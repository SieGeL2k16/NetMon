/**************************************************************************************************
 * FILENAME: structs.h
 *  PURPOSE: Struct definitions
 *  CREATED: 29-Jun-2004
 * MODIFIED: 30-Jun-2004
 *   AUTHOR: SGL
 **************************************************************************************************/

/*
 * This struct represents all available interfaces incl. required Object Pointers
 */

struct	MInterfaces
	{
	struct	MinNode	Node;
	char    Name[16];						// Name of Interface (max 16 chars according to docs)
	APTR    NamePtr,						// Pointer to Text Object
					StatePtr,						// Pointer to State Object
					SpeedPtr,						// Pointer to Speed Object
          VolumePtr,					// Pointer to Volume (Bytes) Object
					TimePtr;						// Pointer to Online Counter Object
	double	FirstSample,  			// Var stores bytes on first call, required to determine speed
					CurrentSample,			// Var stores bytes on next call, calc speed and copy it back to FirstSample
					FirstMaxIn, 				// First speed for inbound traffic
					FirstMaxOut,				// First speed for outbound traffic
					CurrentMaxIn,				// Actual value for calculating MaxIn Speed
					CurrentMaxOut,			// Actual value for calculating MaxOut Speed
					MaxIn, 							// Highest value
					MaxOut;             // Highest value
	char		NameBubble[250],		// Memory for our Bubble Helps (MUI takes them as static, so... ;)
					VolumeBubble[100],	// Memory for our bubble helps (MUI takes them as static, so... :)
					SpeedRecord[200],   // Memory for our bubble helps (MUI takes them as static, so... ;)
					MaxInUnit[20], 			// Human-readable data for MaxIN (xx kb etc.)
					MaxOutUnit[20],			// Human-readable data for Maxout (xx kb etc.)
					CurrentInUnit[20],	// Current In value (human readable)
					CurrentOutUnit[20];	// Current Out vlaue (human readable)
	};
