@echo off
REM low bandwidth (100 KB/sec) sequential 64K writes
DiskSpd.exe -W10 -w100 -b64K -h -g100 -d2000000000 %*
