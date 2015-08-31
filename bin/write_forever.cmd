@echo off
REM Poor man's "run forever": pass a giant duration
set DISKSPD_CMD=DiskSpd.exe -W10 -w100 -h -d2000000000 %*
echo %DISKSPD_CMD%
%DISKSPD_CMD%
