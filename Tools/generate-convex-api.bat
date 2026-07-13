@echo off
setlocal
::=============================================================================
:: generate-convex-api.bat - regenerate typed Convex API wrappers without
:: opening Unreal. Thin wrapper over the convex-ue-codegen CLI.
::
::   generate-convex-api.bat [codegen args...]
::
:: With no args, generates into Example\Source\ConvexExample\ConvexApi using
:: deployment credentials resolved the usual way (CONVEX_DEPLOY_KEY et al from
:: the environment or .env.local / convex.env.local / .env next to the
:: current directory). Any arguments are forwarded verbatim, overriding the
:: defaults.
::
:: The codegen executable is located via:
::   1. CONVEX_UE_CODEGEN (full path to convex-ue-codegen.exe)
::   2. the sibling checkout ..\convex-ue-codegen (built via its Tools\*.bat)
::=============================================================================

set "REPO_ROOT=%~dp0.."
set "EXE=%CONVEX_UE_CODEGEN%"

if "%EXE%"=="" (
    set "EXE=%REPO_ROOT%\..\convex-ue-codegen\build\cli\Release\convex-ue-codegen.exe"
)
if not exist "%EXE%" (
    echo [generate-convex-api] convex-ue-codegen.exe not found.
    echo   Looked at: %EXE%
    echo   Build it via ..\convex-ue-codegen\Tools\convex-ue-codegen.bat --help
    echo   or set CONVEX_UE_CODEGEN to the executable path.
    exit /b 1
)

if "%~1"=="" (
    "%EXE%" --out "%REPO_ROOT%\Example\Source\ConvexExample\ConvexApi" --prefix ConvexApi
) else (
    "%EXE%" %*
)
exit /b %errorlevel%
