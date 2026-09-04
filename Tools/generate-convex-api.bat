@echo off
setlocal
::=============================================================================
:: generate-convex-api.bat - regenerate typed Convex API wrappers without
:: opening the Unreal Editor UI.
::
::   generate-convex-api.bat [-out <dir>] [-script-out <dir>] [extra commandlet args]
::
:: Self-contained: runs the ConvexCodegen commandlet (the emission core is
:: vendored inside the plugin's ConvexEditor module), so only the engine and
:: this repository are needed. When a standalone convex-ue-codegen build is
:: available (CONVEX_UE_CODEGEN env var, or the sibling checkout), it is used
:: instead — same core, same bytes, but skips booting headless UE.
::
:: Deployment credentials resolve like everywhere else: CONVEX_DEPLOY_KEY et
:: al from the environment, else .env.local / convex.env.local / .env found
:: near the project.
::
:: Defaults: output Example\Source\ConvexExample\ConvexApi, prefix ConvexApi,
:: AngelScript wrappers into Example\Script (pass -script-out "" to skip them).
:: Engine root: CONVEX_UE_ENGINE or F:\UE5\UE_5.8.
::=============================================================================

set "REPO_ROOT=%~dp0.."
set "OUT_DIR=%REPO_ROOT%\Example\Source\ConvexExample\ConvexApi"
set "SCRIPT_OUT_DIR=%REPO_ROOT%\Example\Script"
:parse_args
if /I "%~1"=="-out" (
    set "OUT_DIR=%~2"
    shift & shift
    goto parse_args
)
if /I "%~1"=="-script-out" (
    set "SCRIPT_OUT_DIR=%~2"
    shift & shift
    goto parse_args
)
set "SCRIPT_ARG="
set "SCRIPT_CMDLET_ARG="
if not "%SCRIPT_OUT_DIR%"=="" (
    set "SCRIPT_ARG=--script-out "%SCRIPT_OUT_DIR%""
    set "SCRIPT_CMDLET_ARG=-ScriptOut="%SCRIPT_OUT_DIR%""
)

:: --- Fast path: standalone CLI if available --------------------------------
set "EXE=%CONVEX_UE_CODEGEN%"
if "%EXE%"=="" set "EXE=%REPO_ROOT%\..\convex-ue-codegen\build\cli\Release\convex-ue-codegen.exe"
if exist "%EXE%" (
    echo [generate-convex-api] Using standalone CLI: %EXE%
    "%EXE%" --out "%OUT_DIR%" --prefix ConvexApi %SCRIPT_ARG%
    exit /b %errorlevel%
)

:: --- Self-contained path: the ConvexCodegen commandlet ----------------------
set "ENGINE=%CONVEX_UE_ENGINE%"
if "%ENGINE%"=="" set "ENGINE=F:\UE5\UE_5.8"
set "EDITOR_CMD=%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if not exist "%EDITOR_CMD%" (
    echo [generate-convex-api] UnrealEditor-Cmd.exe not found at %EDITOR_CMD%.
    echo   Set CONVEX_UE_ENGINE to your engine root.
    exit /b 1
)

echo [generate-convex-api] Using the ConvexCodegen commandlet (headless UE).
"%EDITOR_CMD%" "%REPO_ROOT%\Example\ConvexExample.uproject" -run=ConvexCodegen ^
    -Out="%OUT_DIR%" -Prefix=ConvexApi %SCRIPT_CMDLET_ARG% -unattended -nosplash -nullrhi %*
exit /b %errorlevel%
