@echo off
echo ===================================================
echo Building Gammie... :)
echo ===================================================

if not exist build mkdir build
cd build

echo Configuring with CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
echo Oh no!! CMake configuration failed! Make sure Visual Studio or MinGW is installed.
pause
exit /b %ERRORLEVEL%
)

echo Compiling Release Executable...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
echo Build failed!
pause
exit /b %ERRORLEVEL%
)

echo ===================================================
echo Build successful!
echo Executable generated at: build/Release/Gammie.exe
echo ===================================================
pause
