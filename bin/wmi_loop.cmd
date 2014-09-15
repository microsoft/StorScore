@echo off

REM Issue a WMI get for Win32_DiskDrive every 5 seconds forever
REM Idea is to issue a SMART Return Status command.

:loop
    timeout 5
    wmic path Win32_DiskDrive get
goto loop
