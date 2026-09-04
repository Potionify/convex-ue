@echo off
setlocal EnableDelayedExpansion
::=============================================================================
:: convex-ue-build.bat - build (and optionally package) the Convex UE plugin.
::
::   convex-ue-build.bat                 Build the Example editor target (dev).
::   convex-ue-build.bat dev             Same, explicitly.
::   convex-ue-build.bat package         Package a redistributable plugin into
::                                       Dist\Convex (RunUAT BuildPlugin).
::   convex-ue-build.bat test            Build, then run the automation tests.
::   convex-ue-build.bat angelscript     Build a copy against the Hazelight
::                                       UnrealEngine-Angelscript fork and run
::                                       the Example/Script unit tests there.
::   convex-ue-build.bat clean           Delete Binaries/Intermediate.
::
:: Options (any order, after the command):
::   -engine=<path>   UE root (default: %CONVEX_UE_ENGINE% or F:\UE5\UE_5.8)
::   -config=<cfg>    Build configuration (default: Development)
::   -out=<path>      Package output dir (default: <repo>\Dist\Convex)
::   -work=<path>     angelscript only: where the fork build copy lives
::                    (default: %CONVEX_UEAS_WORK% or %LOCALAPPDATA%\convex-ue-as)
::
:: The angelscript command reads the fork's root from -engine, else
:: %CONVEX_UEAS_ENGINE%. The fork patches CoreUObject and UHT, so its binaries
:: never mix with a stock build: the command copies the Example project and
:: the plugin to a short work path (UBT rejects paths over 260 characters)
:: and builds there, leaving this checkout's Binaries alone.
::
:: Close the Unreal Editor before building: a running editor locks the plugin
:: DLLs, and reflection changes (UCLASS/UFUNCTION/delegate signatures) are not
:: picked up by Live Coding anyway.
::=============================================================================

set "REPO_ROOT=%~dp0"
if "%REPO_ROOT:~-1%"=="\" set "REPO_ROOT=%REPO_ROOT:~0,-1%"

:: --- defaults ---------------------------------------------------------------
set "COMMAND=%~1"
if "%COMMAND%"=="" set "COMMAND=dev"
if not "%COMMAND%"=="" shift /1

if defined CONVEX_UE_ENGINE (set "ENGINE=%CONVEX_UE_ENGINE%") else (set "ENGINE=F:\UE5\UE_5.8")
set "CONFIG=Development"
set "OUTDIR=%REPO_ROOT%\Dist\Convex"
if defined CONVEX_UEAS_WORK (set "WORKDIR=%CONVEX_UEAS_WORK%") else (set "WORKDIR=%LOCALAPPDATA%\convex-ue-as")
set "ENGINE_EXPLICIT="

:: --- parse options ----------------------------------------------------------
:parse_args
if "%~1"=="" goto args_done
set "ARG=%~1"
if /I "!ARG:~0,8!"=="-engine=" (set "ENGINE=!ARG:~8!" & set "ENGINE_EXPLICIT=1")
if /I "!ARG:~0,8!"=="-config=" set "CONFIG=!ARG:~8!"
if /I "!ARG:~0,5!"=="-out="   set "OUTDIR=!ARG:~5!"
if /I "!ARG:~0,6!"=="-work="  set "WORKDIR=!ARG:~6!"
shift /1
goto parse_args
:args_done

:: The fork lives at its own root; only the angelscript command uses it.
if /I "%COMMAND%"=="angelscript" if not defined ENGINE_EXPLICIT (
    if not defined CONVEX_UEAS_ENGINE (
        echo [convex-ue] ERROR: set CONVEX_UEAS_ENGINE to the UnrealEngine-Angelscript root,
        echo             or pass -engine=^<path^>.
        exit /b 1
    )
    set "ENGINE=%CONVEX_UEAS_ENGINE%"
)

set "UPLUGIN=%REPO_ROOT%\Convex.uplugin"
set "UPROJECT=%REPO_ROOT%\Example\ConvexExample.uproject"
set "BUILD_BAT=%ENGINE%\Engine\Build\BatchFiles\Build.bat"
set "RUNUAT=%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat"
set "EDITOR_CMD=%ENGINE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

:: --- sanity checks ----------------------------------------------------------
if not exist "%ENGINE%\Engine" (
    echo [convex-ue] ERROR: Unreal Engine not found at "%ENGINE%".
    echo             Pass -engine=^<path^> or set CONVEX_UE_ENGINE.
    exit /b 1
)
if not exist "%UPLUGIN%" (
    echo [convex-ue] ERROR: Convex.uplugin not found next to this script.
    exit /b 1
)

:: The plugin compiles a vendored copy of convex-cpp; a stale copy silently
:: builds old code. Warn (do not fail) when the checker reports a mismatch.
if exist "%REPO_ROOT%\Tools\check-vendor-sync.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%REPO_ROOT%\Tools\check-vendor-sync.ps1" >nul 2>&1
    if errorlevel 1 (
        echo [convex-ue] WARNING: vendored convex-cpp is out of sync.
        echo             Run Tools\sync-convex-cpp.ps1 to refresh it.
    )
)

if /I "%COMMAND%"=="dev"     goto do_dev
if /I "%COMMAND%"=="test"    goto do_test
if /I "%COMMAND%"=="package" goto do_package
if /I "%COMMAND%"=="clean"   goto do_clean
if /I "%COMMAND%"=="angelscript" goto do_angelscript
echo [convex-ue] ERROR: unknown command "%COMMAND%". Use dev, test, package, angelscript or clean.
exit /b 1

::=============================================================================
:do_dev
echo [convex-ue] Building ConvexExampleEditor ^(Win64 %CONFIG%^) with "%ENGINE%"...
call "%BUILD_BAT%" ConvexExampleEditor Win64 %CONFIG% -project="%UPROJECT%" -WaitMutex
if errorlevel 1 (
    echo [convex-ue] BUILD FAILED.
    exit /b 1
)
echo [convex-ue] Build succeeded.
if /I "%COMMAND%"=="test" goto run_tests
exit /b 0

::=============================================================================
:do_test
call :do_dev
if errorlevel 1 exit /b 1
:run_tests
echo [convex-ue] Running automation tests ^(Convex.*^)...
echo             Live tests need the backend: convex-cpp\integration\backend ^> docker compose up -d
"%EDITOR_CMD%" "%UPROJECT%" -ExecCmds="Automation RunTests Convex; Quit" ^
    -unattended -nullrhi -nosplash -nosound -stdout
if errorlevel 1 (
    echo [convex-ue] TESTS FAILED ^(see Example\Saved\Logs\ConvexExample.log^).
    exit /b 1
)
echo [convex-ue] Tests finished. Check the log for Result={Success} lines.
exit /b 0

::=============================================================================
:do_package
echo [convex-ue] Packaging redistributable plugin to "%OUTDIR%"...
if exist "%OUTDIR%" rmdir /S /Q "%OUTDIR%"

:: Example\Plugins\Convex is a junction back to this repo root (so the Example
:: project can compile the plugin in place). BuildPlugin copies the plugin
:: tree recursively and would follow it forever, so drop the junction for the
:: duration of the package and restore it afterwards. `rmdir` on a junction
:: removes the link only, never the target.
set "JUNCTION=%REPO_ROOT%\Example\Plugins\Convex"
set "JUNCTION_REMOVED="
if exist "%JUNCTION%" (
    echo [convex-ue] Temporarily removing the Example plugin junction...
    rmdir "%JUNCTION%" 2>nul
    if not exist "%JUNCTION%" set "JUNCTION_REMOVED=1"
)

call "%RUNUAT%" BuildPlugin -Plugin="%UPLUGIN%" -Package="%OUTDIR%" -Rocket -TargetPlatforms=Win64
set "PACKAGE_RESULT=%ERRORLEVEL%"

if defined JUNCTION_REMOVED (
    echo [convex-ue] Restoring the Example plugin junction...
    mklink /J "%JUNCTION%" "%REPO_ROOT%" >nul
)

if not "%PACKAGE_RESULT%"=="0" (
    echo [convex-ue] PACKAGING FAILED.
    exit /b 1
)

:: BuildPlugin leaves its build artifacts behind (~140 MB). They are not part
:: of a redistributable plugin - consumers rebuild from Source - and Epic's
:: submission guidelines require removing them.
if exist "%OUTDIR%\Intermediate" (
    echo [convex-ue] Stripping Intermediate build artifacts...
    rmdir /S /Q "%OUTDIR%\Intermediate"
)

echo [convex-ue] Packaged plugin: %OUTDIR%
echo             Copy that folder into ^<YourProject^>\Plugins\Convex.
echo             ^(Binaries\Win64\*.pdb may be deleted to shrink it further.^)
exit /b 0

::=============================================================================
:do_angelscript
set "WORK_PROJECT=%WORKDIR%\Example"
echo [convex-ue] Copying the Example project and plugin to "%WORKDIR%"...
if exist "%WORKDIR%" rmdir /S /Q "%WORKDIR%"
robocopy "%REPO_ROOT%\Example" "%WORK_PROJECT%" /E /XD Binaries Intermediate Saved DerivedDataCache Plugins /NFL /NDL /NJH /NJS >nul
robocopy "%REPO_ROOT%" "%WORK_PROJECT%\Plugins\Convex" /E /XD Binaries Intermediate Dist Example Tools .git /XF *.bat /NFL /NDL /NJH /NJS >nul
if not exist "%WORK_PROJECT%\ConvexExample.uproject" (
    echo [convex-ue] ERROR: copy failed.
    exit /b 1
)

echo [convex-ue] Building ConvexExampleEditor ^(Win64 %CONFIG%^) with the fork at "%ENGINE%"...
call "%BUILD_BAT%" ConvexExampleEditor Win64 %CONFIG% -project="%WORK_PROJECT%\ConvexExample.uproject" -WaitMutex -NoHotReload
if errorlevel 1 (
    echo [convex-ue] BUILD FAILED.
    exit /b 1
)

echo [convex-ue] Running the AngelScript unit tests ^(Example\Script^)...
"%EDITOR_CMD%" "%WORK_PROJECT%\ConvexExample.uproject" -run=AngelscriptTest ^
    -unattended -nopause -nullrhi -nosplash -nosound -stdout
if errorlevel 1 (
    echo [convex-ue] SCRIPT TESTS FAILED ^(see %WORK_PROJECT%\Saved\Logs\ConvexExample.log^).
    exit /b 1
)
echo [convex-ue] Script tests passed. Build copy kept at %WORKDIR%.
exit /b 0

::=============================================================================
:do_clean
echo [convex-ue] Cleaning build artifacts...
for %%D in ("%REPO_ROOT%\Binaries" "%REPO_ROOT%\Intermediate" ^
            "%REPO_ROOT%\Example\Binaries" "%REPO_ROOT%\Example\Intermediate") do (
    if exist %%D (
        echo             removing %%D
        rmdir /S /Q %%D
    )
)
echo [convex-ue] Clean complete.
exit /b 0
