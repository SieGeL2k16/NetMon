VERSION		EQU	52
REVISION	EQU	1

DATE	MACRO
		dc.b '11.2.2007'
		ENDM

VERS	MACRO
		dc.b 'NetMon.docky 52.1'
		ENDM

VSTRING	MACRO
		dc.b 'NetMon.docky 52.1 (11.2.2007)',13,10,0
		ENDM

VERSTAG	MACRO
		dc.b 0,'$VER: NetMon.docky 52.1 (11.2.2007)',0
		ENDM