# VAX/VMS MMS (makefile) for ADD

#####(LIBRARIES/INCLUDES)#######################################################

#####(COMPILE/OPTIONS)##########################################################
CFLAGS	=-
	/G_FLOAT -
	/Listing -
	/Diagnostics /Debug

################################################################################

ALL	= add.exe

all :	$(ALL)
	@ write sys$output "** made $@"

clean :
	@- if f$search("*.lis") .nes. "" then delete *.lis;*
	@- if f$search("*.obj") .nes. "" then delete *.obj;*
	@- if f$search("*.out") .nes. "" then delete *.out;*
	@- if f$search("*.log") .nes. "" then delete *.log;*
	@- if f$search("*.map") .nes. "" then delete *.map;*

clobber :	clean
	@- if f$search("*.exe") .nes. "" then delete *.exe;*

.first :
	@ define/nolog SYS SYS$LIBRARY
.last :
	@- if f$search("*.dia") .nes. "" then delete *.dia;*
	@- if f$search("*.lis") .nes. "" then purge *.lis
	@- if f$search("*.map") .nes. "" then purge *.map
	@ ADD :== $SYS$DISK:'F$DIRECTORY()$(ALL)

$(ALL) :	add.obj
	LINK/map/debug ADD.OBJ, VMS_LINK.OPT/OPT

.C.OBJ :
	$(CC) $(CFLAGS) $(MMS$SOURCE)
	@- delete $(MMS$TARGET_NAME).dia;*
