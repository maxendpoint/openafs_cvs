@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html


REM AFS build environment variables for Windows NT.
REM Modify for local configuration; common defaults shown.
REM ########################################################################

REM ########################################################################
REM
REM NOTE: You should run NTLANG.REG before attempting to build localized
REM language files! Failure to do so will cause the resource compiler
REM and message-catalog compiler to choke when they hit unknown code pages.
REM
REM NOTE: You will need to copy the NLS files into your windows\system32 
REM directory prior to building non-english files.
REM
REM ########################################################################


REM ########################################################################
REM Accept build type as an argument; default to checked.

if "%1"=="" goto checked
if "%1"=="checked" goto checked
if "%1"=="CHECKED" goto checked

if "%1"=="free" goto free
if "%1"=="FREE" goto free

if "%1"=="wspp" goto wspp
if "%1"=="WSPP" goto wspp

goto usage

:checked
set AFSBLD_TYPE=CHECKED
set AFSBLD_IS_WSPP=
set AFSDEV_CRTDEBUG=1
goto args_done

:free
set AFSBLD_TYPE=FREE
set AFSBLD_IS_WSPP=
set AFSDEV_CRTDEBUG=0
goto args_done

:wspp
set AFSBLD_TYPE=FREE
set AFSBLD_IS_WSPP=1
set AFSDEV_CRTDEBUG=0
goto args_done



:args_done
REM ########################################################################
REM General required definitions:
REM     SYS_NAME = AFS system name
REM Choose one of "i386_win95" or "i386_nt40"

SET SYS_NAME=i386_nt40

REM Specify the targeted version of Windows and IE: 0x400 for Win9x/NT4 
REM and above; 0x500 for Windows 2000 and above

SET _WIN32_IE=0x400

REM ########################################################################
REM NTMakefile required definitions:
REM     AFSDEV_BUILDTYPE = CHECKED / FREE 
REM     AFSDEV_INCLUDE = default include directories
REM     AFSDEV_LIB = default library directories
REM     AFSDEV_BIN = default build binary directories
REM     AFSVER_CL  = version of the Microsoft compiler "1200" for VC6;
REM                  or "1300" for VC7 (.NET)
REM                  or "1310" for .NET 2003

set AFSDEV_BUILDTYPE=%AFSBLD_TYPE%

REM Location of Microsoft Visual C++ development folder (8.3 short name)
set MSVCDIR=c:\progra~1\micros~2\vc98

REM Location of Microsoft Platform SDK (8.3 short name)
set MSSDKDIR=c:\progra~1\micros~4

REM Location of npapi.h (from DDK or Platform SDK samples - 8.3 short name)
set NTDDKDIR=c:\progra~1\micros~5

REM Location of netmpr.h/netspi.h (from Windows 95/98 DDK - 8.3 short name)
SET 9XDDKDIR=c:\progra~1\micros~6

set AFSDEV_INCLUDE=%MSSDKDIR%\include;%MSVCDIR%\include;%MSVCDIR%\mfc\include
set AFSDEV_INCLUDE=%AFSDEV_INCLUDE%;%NTDDKDIR%\include;%9XDDKDIR%\include
set AFSDEV_LIB=%MSSDKDIR%\lib;%MSVCDIR%\lib;%MSVCDIR%\mfc\lib
set AFSDEV_BIN=%MSSDKDIR%\bin;%MSVCDIR%\bin

REM ########################################################################
REM Location of base folder where source lies, build directory
REM e.g. AFSROOT\SRC is source directory of the build tree (8.3 short name)

set AFSROOT=D:\Dev\AfsSorce\OpenAF~2.2

REM ########################################################################
REM NTMakefile optional definitions:
REM
REM See NTMakefile.SYS_NAME; will normally use defaults.
REM

IF [%HOMEDRIVE%]==[] SET HOMEDRIVE=C:

REM ########################################################################
REM Options necessary when using bison
REM

set BISON_SIMPLE=c:\bin\bison.simple
set BISON_HAIRY=c:\bin\bison.hairy

goto end


:usage
echo.
echo Usage: %0 [free^|^checked^|^wspp]
echo.

:end
