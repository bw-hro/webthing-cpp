@echo off
setlocal

set "tools_dir=%~dp0"
set "vcpkg_dir=%tools_dir%vcpkg"

if not exist "%vcpkg_dir%" (
    echo clone vcpkg from GitHub
    git clone https://github.com/Microsoft/vcpkg.git "%vcpkg_dir%"
)

echo install vcpkg
"%vcpkg_dir%\bootstrap-vcpkg.bat" -disableMetrics