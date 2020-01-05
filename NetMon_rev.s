VERSION = 0
REVISION = 35

.macro DATE
.ascii "26.12.2006"
.endm

.macro VERS
.ascii "NetMon 35.35"
.endm

.macro VSTRING
.ascii "NetMon 0.35 (26.12.2006)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: NetMon 0.35 (26.12.2006)"
.byte 0
.endm
