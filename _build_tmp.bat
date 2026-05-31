@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\src\BasicGameEngine
MSBuild BasicGameEngine.sln /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal
