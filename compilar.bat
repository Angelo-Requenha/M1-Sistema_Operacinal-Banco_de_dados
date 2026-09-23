@echo off
rem ===========================================================================
rem  compilar.bat - Trabalho M1 de Sistemas Operacionais
rem
rem  Gera bin\servidor.exe e bin\cliente.exe com o g++ do MinGW-w64.
rem  Windows puro: nao precisa de WSL nem de Linux.
rem ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"
title Compilar - Trabalho M1 SO

echo ===========================================================
echo   Trabalho M1 - Sistemas Operacionais
echo   Compilando com g++ (MinGW-w64) - Windows nativo
echo ===========================================================
echo.

rem --- acha o compilador ----------------------------------------------------
rem  Precisamos de um g++ com "Thread model: posix" (tem pthread.h). Pode
rem  existir um MinGW antigo (ex.: C:\MinGW, thread model win32, SEM
rem  pthread.h) na frente no PATH - foi exatamente esse o problema visto
rem  neste trabalho. Por isso testamos varios candidatos, nesta ordem, e
rem  usamos o primeiro que for realmente "posix":
rem    1) tools\mingw64\bin\g++.exe   (pasta ao lado deste script, se voce
rem                                    copiar o mingw64\ do WinLibs pra
rem                                    dentro do projeto - assim funciona
rem                                    em qualquer PC, inclusive o do
rem                                    professor, sem precisar instalar nada)
rem    2) %USERPROFILE%\Desktop\mingw64\bin\g++.exe
rem    3) o g++ que estiver no PATH
set "GPP="
for %%C in (
    "tools\mingw64\bin\g++.exe"
    "%USERPROFILE%\Desktop\mingw64\bin\g++.exe"
) do (
    if not defined GPP if exist %%C (
        %%~C -v 2>&1 | findstr /C:"Thread model: posix" >nul
        if not errorlevel 1 set "GPP=%%~C"
    )
)

if not defined GPP (
    where g++ >nul 2>&1
    if not errorlevel 1 (
        g++ -v 2>&1 | findstr /C:"Thread model: posix" >nul
        if not errorlevel 1 set "GPP=g++"
    )
)

if not defined GPP goto sem_compilador_posix

echo compilador: !GPP!
echo.

if not exist bin mkdir bin

set "OPC=-std=c++17 -Wall -Wextra -O2 -static -static-libgcc -static-libstdc++ -Isrc\cabecalhos"

echo [1/2] servidor.exe ...
"!GPP!" %OPC% src\servidor\comunicacao.cpp src\servidor\banco.cpp src\servidor\terminal.cpp -o bin\servidor.exe -lpthread
if errorlevel 1 goto falhou

echo [2/2] cliente.exe ...
"!GPP!" %OPC% src\cliente\comunicacao.cpp src\cliente\requisicao.cpp src\cliente\terminal.cpp -o bin\cliente.exe -lpthread
if errorlevel 1 goto falhou

echo.
echo ===========================================================
echo   OK - binarios gerados em bin\
echo ===========================================================
dir /b bin
echo.
echo   Agora rode  executar.bat
echo.
pause
exit /b 0

:sem_compilador_posix
echo.
echo   ERRO: nao encontrei um g++ com suporte a Pthreads (thread model
echo   "posix"). Isso costuma acontecer quando so ha um MinGW antigo
echo   instalado (ex.: C:\MinGW, thread model "win32", sem pthread.h).
echo.
echo   Baixe o WinLibs (variante POSIX, UCRT) em:
echo     https://github.com/brechtsanders/winlibs_mingw/releases
echo   Procure o arquivo com "posix" e "ucrt" no nome, ex.:
echo     winlibs-x86_64-posix-seh-gcc-*-mingw-w64ucrt-*.zip
echo.
echo   Extraia o zip: dentro dele tem uma pasta "mingw64". Copie essa
echo   pasta para "tools\mingw64" AQUI DENTRO DO PROJETO (ao lado deste
echo   compilar.bat) - assim o script acha ela sozinho, em qualquer PC.
echo.
pause
exit /b 1

:falhou
echo.
echo   ERRO de compilacao. Veja as mensagens do g++ acima.
echo.
pause
exit /b 1
