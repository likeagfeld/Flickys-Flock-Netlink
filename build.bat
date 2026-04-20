@ECHO Off
SET COMPILER_DIR=D:\joengine-master\Compiler
SET JO_ENGINE_SRC_DIR=D:/joengine-master/jo_engine
SET PATH=%COMPILER_DIR%\WINDOWS\Other Utilities;%COMPILER_DIR%\WINDOWS\bin;%PATH%

ECHO.
ECHO === Building Flicky's Flock ===
ECHO.

REM Step 1: Compile and link (ignore mkisofs failure from jo_engine_makefile)
make re JO_ENGINE_SRC_DIR=%JO_ENGINE_SRC_DIR% COMPILER_DIR=D:/joengine-master/Compiler OS=Windows_NT 2>NUL

REM Step 2: Verify compilation succeeded
IF NOT EXIST game.elf (
    ECHO === ERROR: Compilation failed! No game.elf produced. ===
    PAUSE
    EXIT /B 1
)
IF NOT EXIST cd\0.bin (
    ECHO === ERROR: Compilation failed! No cd\0.bin produced. ===
    PAUSE
    EXIT /B 1
)

ECHO === Compile + Link OK ===

REM Step 3: Always create ISO with correct mkisofs invocation
ECHO === Creating ISO... ===
IF EXIST game.iso DEL /Q game.iso
mkisofs -quiet -sysid "SEGA SATURN" -volid "SaturnApp" -volset "SaturnApp" -sectype 2352 -publisher "SEGA ENTERPRISES, LTD." -preparer "SEGA ENTERPRISES, LTD." -appid "SaturnApp" -abstract "ABS.TXT" -copyright "CPY.TXT" -biblio "BIB.TXT" -generic-boot %COMPILER_DIR%\COMMON\IP.BIN -full-iso9660-filenames -o game.iso cd
IF NOT EXIST game.iso (
    ECHO === ERROR: ISO creation failed! ===
    PAUSE
    EXIT /B 1
)
ECHO === ISO created successfully ===

REM Step 4: Generate CUE file with audio tracks
JoEngineCueMaker.exe
REM Rename to START GAME.CUE
IF EXIST game.cue (
    IF EXIST "START GAME.CUE" DEL /Q "START GAME.CUE"
    RENAME game.cue "START GAME.CUE"
)
ECHO === CUE file generated ===

REM Step 5: Package everything into build\ folder
ECHO.
ECHO === Packaging build output ===
IF NOT EXIST build mkdir build

copy /Y game.iso build\game.iso >NUL
copy /Y "START GAME.CUE" "build\START GAME.CUE" >NUL
copy /Y TRACK1.WAV build\TRACK1.WAV >NUL 2>NUL
copy /Y TRACK2.WAV build\TRACK2.WAV >NUL 2>NUL
copy /Y TRACK3.WAV build\TRACK3.WAV >NUL 2>NUL
copy /Y TRACK4.WAV build\TRACK4.WAV >NUL 2>NUL

REM Copy server file to build for reference
copy /Y ..\tools\flock_server\fserver.py build\fserver.py >NUL 2>NUL

ECHO.
ECHO === Build complete! Files in build\ folder: ===
DIR /B build\
ECHO.
PAUSE
