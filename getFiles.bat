@echo off
FOR /R "./src" %%f IN (*.cpp) DO (
  echo %%f
)
