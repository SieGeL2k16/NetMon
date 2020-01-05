VERSION = 52
REVISION = 1

.macro DATE
.ascii "11.2.2007"
.endm

.macro VERS
.ascii "NetMon.docky 52.1"
.endm

.macro VSTRING
.ascii "NetMon.docky 52.1 (11.2.2007)"
.byte 13,10,0
.endm

.macro VERSTAG
.byte 0
.ascii "$VER: NetMon.docky 52.1 (11.2.2007)"
.byte 0
.endm
