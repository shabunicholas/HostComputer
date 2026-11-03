@echo off
chcp 65001 >nul
cd /d "%~dp0build"

if not exist ModbusSlaveTool.exe (
    echo 找不到 ModbusSlaveTool.exe，请先编译：qmake ^&^& mingw32-make
    pause
    exit /b 1
)

echo ============================================================
echo  虚拟从站：COM2   19200 8E1   从站地址 1
echo  上位机请连 COM1，并在 config.ini 里设置：
echo      [Serial]  Port=COM1  BaudRate=19200
echo      [Protocol] Name=modbus
echo ============================================================
echo.

ModbusSlaveTool.exe --port COM2 --baud 19200 --parity even --slave 1 --temp 0 --press 16

echo.
echo 从站已退出。
pause
