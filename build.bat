@echo off
set MSBUILD="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe"

echo Building Search Lobbys (GUI)...
%MSBUILD% "Search Lobbys.sln" /p:Configuration="Release" /p:Platform="x64" /t:Rebuild
if %errorlevel% neq 0 (
    echo Search Lobbys Build Failed
    exit /b %errorlevel%
)

echo Building LobbyFetcher (Console)...
%MSBUILD% "LobbyFetcher.vcxproj" /p:Configuration="Release" /p:Platform="x64" /t:Rebuild
if %errorlevel% neq 0 (
    echo LobbyFetcher Build Failed
    exit /b %errorlevel%
)

echo Build Succeeded
