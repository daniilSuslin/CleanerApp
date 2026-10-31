@echo off
g++ src/cleaner.cpp -o CleanerApp.exe -mwindows -municode -lole32 -lshell32 -luuid -O2 -s
if errorlevel 1 (echo Build failed) else (echo Done: flowgen.exe)
