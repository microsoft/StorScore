@echo off

REM Avoid SysInternals EULA GUI popup
reg add HKCU\Software\Sysinternals\Sync /v EulaAccepted /t REG_DWORD /d 1 /f

REM Issue a disk flush every 5 seconds forever
:loop
    timeout 5
    sync %* 
goto loop
