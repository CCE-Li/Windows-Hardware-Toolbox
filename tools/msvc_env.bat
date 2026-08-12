@echo off
rem Sets up the MSVC x64 toolchain environment for this machine
rem (VS Build Tools 2026 installed without installer COM registration;
rem  VsDevCmd.bat cannot locate VC tools here, so paths are set manually).
rem Usage: tools\msvc_env.bat <command...>

set "VCToolsVersion=14.44.35207"
set "VCToolsInstallDir=D:\LenovoQMDownload\Tools\VisualStdio\Cpp-Build-Tools\VC\Tools\MSVC\%VCToolsVersion%\"
set "VCINSTALLDIR=D:\LenovoQMDownload\Tools\VisualStdio\Cpp-Build-Tools\VC\"
set "WindowsSdkDir=D:\Windows Kits\10\"
set "WindowsSdkVer=10.0.22621.0"
set "UniversalCRTSdkDir=%WindowsSdkDir%"

set "PATH=%VCToolsInstallDir%bin\HostX64\x64;%WindowsSdkDir%bin\%WindowsSdkVer%\x64;%WindowsSdkDir%bin\x64;%PATH%"
set "INCLUDE=%VCToolsInstallDir%include;%UniversalCRTSdkDir%Include\%WindowsSdkVer%\ucrt;%UniversalCRTSdkDir%Include\%WindowsSdkVer%\um;%UniversalCRTSdkDir%Include\%WindowsSdkVer%\shared;%UniversalCRTSdkDir%Include\%WindowsSdkVer%\winrt"
set "LIB=%VCToolsInstallDir%lib\x64;%UniversalCRTSdkDir%Lib\%WindowsSdkVer%\ucrt\x64;%UniversalCRTSdkDir%Lib\%WindowsSdkVer%\um\x64"

%*
