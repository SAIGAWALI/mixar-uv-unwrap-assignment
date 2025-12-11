@echo off
setlocal

cd /d "%~dp0"

echo Cleaning build directory...
if exist build rmdir /s /q build
mkdir build
cd build

echo Configuring with CMake...
echo Using Eigen at D:\ML\eigen-3.4.0

cmake .. ^
  -A x64 ^
  -DEIGEN3_INCLUDE_DIR=D:/ML/eigen-3.4.0 ^
  -DEigen3_DIR=D:/ML/eigen-3.4.0/cmake ^
  -DCMAKE_PREFIX_PATH=D:/ML/eigen-3.4.0

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Running tests...
if exist Release\test_unwrap.exe (
    Release\test_unwrap.exe
) else if exist Debug\test_unwrap.exe (
    Debug\test_unwrap.exe
) else (
    echo test_unwrap.exe not found!
)

echo.
pause