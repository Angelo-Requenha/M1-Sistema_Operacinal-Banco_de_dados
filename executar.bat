@echo off
rem ===========================================================================
rem  executar.bat - Trabalho M1 de Sistemas Operacionais
rem
rem  Menu para rodar o sistema. Os binarios estao em bin\ e sao executaveis
rem  do Windows: nao precisa de WSL, Linux nem nada instalado.
rem ===========================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"
title Executar - Trabalho M1 SO

if not exist "bin\servidor.exe" goto sem_binario
if not exist "bin\cliente.exe"  goto sem_binario

:menu
cls
echo ===========================================================
echo   Trabalho M1 - Sistemas Operacionais
echo   Memoria compartilhada + semaforos + pool de threads + mutex
echo ===========================================================
echo.
echo   SERVIDOR
echo     1  Subir o servidor            (abre em outra janela)
echo     2  Encerrar o servidor
echo.
echo   CLIENTE
echo     3  Requisicao avulsa           (INSERT/SELECT/UPDATE/DELETE)
echo     4  Modo interativo
echo.
echo   DADOS
echo     5  Ver o log do servidor       (servidor.log)
echo     6  Ver a tabela                (banco.txt)
echo.
echo     0  Sair
echo.
set "op="
set /p op=  opcao:

if "%op%"=="1" goto subir
if "%op%"=="2" goto encerrar
if "%op%"=="3" goto avulsa
if "%op%"=="4" goto interativo
if "%op%"=="5" goto log
if "%op%"=="6" goto tabela
if "%op%"=="0" exit /b 0
goto menu

:subir
echo.
set "t="
set /p t=  quantas threads no pool [4]:
if "!t!"=="" set "t=4"
set "c="
set /p c=  custo artificial por requisicao em ms [500]:
if "!c!"=="" set "c=500"
echo.
echo   abrindo o servidor em outra janela...
echo   dica: abra 2-3 janelas de cliente ao mesmo tempo (opcao 3) para ver
echo   o pool atendendo em paralelo no servidor.log
start "Servidor SO-M1 (pool=!t!)" /D "%~dp0" cmd /k bin\servidor.exe --threads !t! --custo !c!
timeout /t 2 >nul
goto menu

:encerrar
echo.
bin\cliente.exe ENCERRAR
echo.
pause
goto menu

:avulsa
echo.
echo   exemplos:  INSERT 7 Joao  ^|  SELECT 7  ^|  UPDATE 7 Maria  ^|  DELETE 7  ^|  LISTAR
echo.
set "req="
set /p req=  requisicao:
if "!req!"=="" goto menu
echo.
bin\cliente.exe !req!
echo.
pause
goto menu

:interativo
echo.
bin\cliente.exe -i
echo.
pause
goto menu

:log
echo.
if not exist "servidor.log" (
    echo   ainda nao existe servidor.log - suba o servidor primeiro.
    echo.
    pause
    goto menu
)
powershell -NoProfile -Command "Get-Content servidor.log -Tail 60"
echo.
echo   (mostrando as ultimas 60 linhas de servidor.log)
echo.
pause
goto menu

:tabela
echo.
type banco.txt
echo.
pause
goto menu

:sem_binario
echo.
echo   ERRO: bin\servidor.exe ou bin\cliente.exe nao encontrado.
echo   Rode  compilar.bat  primeiro.
echo.
pause
exit /b 1
