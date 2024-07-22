@echo off
FOR %%f IN ("./src/*.cpp") DO (
  echo ./src/%%f
)
FOR %%f IN ("./src/lexer/*.cpp") DO (
  echo ./src/lexer/%%f
)
FOR %%f IN ("./src/parser/*.cpp") DO (
  echo ./src/parser/%%f
)
FOR %%f IN ("./src/parser/nodes/*.cpp") DO (
  echo ./src/parser/nodes/%%f
)