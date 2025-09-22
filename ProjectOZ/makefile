#
# Copyright (c) Microsoft Corporation. All rights reserved.
#
# You may only use this code if you agree to the terms of the ProjectOZ License agreement (see License.txt).
# If you do not agree to the terms, do not use the code.
#

wrk=C:\Projects\WRK\WRK-V1.2
subargs=/$(MAKEFLAGS) wrk=$(wrk)

!if defined(x86) && !defined(amd64)
targ     = i386
machine = x86

!else if !defined(x86) &&  defined(amd64)
targ     = amd64
machine = amd64
!ERROR AMD64 support pending (Currently an exercise left to the reader).

!else
!ERROR Usage:  nmake (x86=|amd64=)  [clean]
!endif

filedir=$(MAKEDIR)\files\$(targ)
rundir=$(MAKEDIR)\run$(machine)
subargs  = $(subargs) targ=$(targ) machine=$(machine) rundir=$(rundir)

OBJ = obj$(targ)

subdirs = SPACE BasicOZ UserMode BasicPrograms
subdirs = SPACE BasicOZ

buildtargets = $(subdirs: =.build ).build
cleantargets = $(subdirs: =.clean ).clean

default: checktoolpath $(buildtargets) fileprep

checktoolpath:
	checktoolarch $(machine)

fileprep:
	xcopy /I /Y $(rundir)\BasicOZ.boz $(filedir)\.
	@echo DONE!

$(buildtargets):
	cd $(MAKEDIR)\$(@R)
		@$(MAKE) -f makefile.$(@R) $(subargs) subdir=$(@R) build
	@cd $(MAKEDIR)

clean: $(cleantargets) clean0

clean0:
	-del $(rundir)\** /Q

$(cleantargets):
	cd $(MAKEDIR)\$(@R)
		@$(MAKE) -f makefile.$(@R) $(subargs) subdir=$(@R) clean
	@cd $(MAKEDIR)
