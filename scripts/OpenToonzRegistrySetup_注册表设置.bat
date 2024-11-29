@echo off
chcp 65001 >nul
setlocal

REM 设置颜色 / Set colors
REM 颜色代码示例：0 = 黑色背景, A = 绿色文字, C = 红色文字, E = 黄色文字

REM 获取当前目录 / Get the current directory
set CURRENT_PATH=%cd%

REM 构建stuff文件夹的完整路径 / Build the complete path to the stuff folder
set STUFF_PATH=%CURRENT_PATH%\..\stuff

REM 分隔线 / Divider line
echo ========================================
color 0E
echo              OpenToonz Registry Setup
echo ========================================

REM 检查注册表项是否存在 / Check if the registry key exists
REG QUERY "HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz" /v TOONZROOT >nul 2>&1
IF %ERRORLEVEL% EQU 0 (
    REM 使用黄色输出已存在的注册表项 / Use yellow color to indicate existing registry key
    color 0E
    echo [警告] 注册表项已存在，不需要添加！ 
    echo [WARNING] Registry key already exists, no need to add!
) ELSE (
    REM 添加注册表项 / Add the registry key
    REG ADD "HKEY_LOCAL_MACHINE\SOFTWARE\OpenToonz\OpenToonz" /v TOONZROOT /t REG_SZ /d "%STUFF_PATH%" /f
    IF %ERRORLEVEL% EQU 0 (
        REM 使用绿色输出成功添加注册表项 / Use green color to indicate successful addition of registry key
        color 0A
        echo [成功] 注册表已成功添加！路径：%STUFF_PATH% 
        echo [SUCCESS] Registry key successfully added! Path: %STUFF_PATH%
    ) ELSE (
        REM 使用红色输出注册表项添加失败 / Use red color to indicate failure to add registry key
        color 0C
        echo [错误] 注册表添加失败！ 
        echo [ERROR] Registry key addition failed!
    )
)

REM 分隔线 / Divider line
echo ========================================
color 0A
echo              操作完成 / Operation Completed
echo ========================================
pause
