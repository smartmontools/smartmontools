# Microsoft Developer Studio Project File - Name="smartctl_vc6" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=smartctl_vc6 - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "smartctl_vc6.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "smartctl_vc6.mak" CFG="smartctl_vc6 - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "smartctl_vc6 - Win32 Release" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE "smartctl_vc6 - Win32 Debug" (basierend auf  "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "smartctl_vc6 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "smartctl.r"
# PROP Intermediate_Dir "smartctl.r"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O1 /I "." /I ".." /I "..\posix" /D "NDEBUG" /D "HAVE_CONFIG_H" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"smartctl.exe"

!ELSEIF  "$(CFG)" == "smartctl_vc6 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "smartctl.d"
# PROP Intermediate_Dir "smartctl.d"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "." /I ".." /I "..\posix" /D "_DEBUG" /D "HAVE_CONFIG_H" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "smartctl_vc6 - Win32 Release"
# Name "smartctl_vc6 - Win32 Debug"
# Begin Group "posix"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\posix\getopt.c
# End Source File
# Begin Source File

SOURCE=..\posix\getopt.h
# End Source File
# Begin Source File

SOURCE=..\posix\getopt1.c
# End Source File
# Begin Source File

SOURCE=..\posix\regcomp.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\posix\regex.c
# ADD CPP /w /W0
# End Source File
# Begin Source File

SOURCE=..\posix\regex.h
# End Source File
# Begin Source File

SOURCE=..\posix\regex_internal.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=..\posix\regex_internal.h
# End Source File
# Begin Source File

SOURCE=..\posix\regexec.c
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Source File

SOURCE=..\atacmdnames.cpp
# End Source File
# Begin Source File

SOURCE=..\atacmdnames.h
# End Source File
# Begin Source File

SOURCE=..\atacmds.cpp
# End Source File
# Begin Source File

SOURCE=..\atacmds.h
# End Source File
# Begin Source File

SOURCE=..\ataprint.cpp
# End Source File
# Begin Source File

SOURCE=..\ataprint.h
# End Source File
# Begin Source File

SOURCE=.\config_vc6.h

!IF  "$(CFG)" == "smartctl_vc6 - Win32 Release"

# Begin Custom Build - Copy $(InputPath) config.h
InputPath=.\config_vc6.h

"config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(InputPath) config.h

# End Custom Build

!ELSEIF  "$(CFG)" == "smartctl_vc6 - Win32 Debug"

# Begin Custom Build - Copy $(InputPath) config.h
InputPath=.\config_vc6.h

"config.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(InputPath) config.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\extern.h
# End Source File
# Begin Source File

SOURCE=..\int64.h
# End Source File
# Begin Source File

SOURCE=..\knowndrives.cpp
# End Source File
# Begin Source File

SOURCE=..\knowndrives.h
# End Source File
# Begin Source File

SOURCE=..\os_win32.cpp
# End Source File
# Begin Source File

SOURCE=..\scsiata.cpp
# End Source File
# Begin Source File

SOURCE=..\scsiata.h
# End Source File
# Begin Source File

SOURCE=..\scsicmds.cpp
# End Source File
# Begin Source File

SOURCE=..\scsicmds.h
# End Source File
# Begin Source File

SOURCE=..\scsiprint.cpp
# End Source File
# Begin Source File

SOURCE=..\scsiprint.h
# End Source File
# Begin Source File

SOURCE=..\smartctl.cpp
# End Source File
# Begin Source File

SOURCE=..\smartctl.h
# End Source File
# Begin Source File

SOURCE=.\syslog.h
# End Source File
# Begin Source File

SOURCE=..\utility.cpp
# End Source File
# Begin Source File

SOURCE=..\utility.h
# End Source File
# End Target
# End Project
