@echo off
rem Starts bootpd with the configuration file next to this script.
rem Allow bootpd through the Windows firewall (UDP 67 and 69) when asked.
cd /d "%~dp0"
"%~dp0..\..\..\bin\bootpd.exe" -C config.ini %*
pause
