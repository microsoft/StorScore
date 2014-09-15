@echo off

REM Avoid SysInternals EULA GUI popup
reg add HKCU\Software\Sysinternals\Sync /v EulaAccepted /t REG_DWORD /d 1 /f

sync.exe %* 
