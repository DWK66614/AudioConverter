@echo off
setlocal
cd /d "%~dp0"
echo ============================================
echo   音频格式转换器 - 一键构建
echo ============================================
echo.

set "VSVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set "NSIS=C:\Program Files (x86)\NSIS\makensis.exe"

if not exist "%VSVARS%" (
    echo [错误] 未找到 Visual Studio Build Tools
    pause & exit /b 1
)

echo [1/4] 编译资源文件...
call "%VSVARS%" >NUL 2>&1
rc /nologo /fo resource.res resource.rc
if %ERRORLEVEL% neq 0 (echo 资源编译失败! & pause & exit /b 1)

echo [2/4] 编译主程序...
cl /nologo /EHsc /std:c++17 /O2 /W3 /utf-8 main.cpp resource.res /Fe:AudioConverter.exe /link user32.lib gdi32.lib comctl32.lib comdlg32.lib uxtheme.lib shell32.lib
if %ERRORLEVEL% neq 0 (echo 编译失败! & pause & exit /b 1)

echo [3/4] 清理临时文件...
del resource.res main.obj *.exp *.lib *.manifest 2>nul

echo [4/4] 生成安装包...
if exist "%NSIS%" (
    "%NSIS%" /INPUTCHARSET UTF8 setup.nsi
    if %ERRORLEVEL% equ 0 (
        echo.
        echo ============================================
        echo   构建完成!
        echo   AudioConverter.exe        - 主程序
        echo   AudioConverter_Setup.exe  - 安装包
        echo ============================================
    ) else (
        echo 安装包生成失败!
    )
) else (
    echo [跳过] NSIS 未安装，安装包未生成
    echo 下载地址: https://nsis.sourceforge.io/Download
    echo.
    echo 主程序 AudioConverter.exe 已编译完成
)

pause
endlocal
