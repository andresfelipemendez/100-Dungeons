cls
call "%~dp0env_vars.bat" 
clang -std=c23 -Wall -Wextra -pedantic -g -I"%TOML_INCLUDE_DIR%" ^
    -o tests.exe ^
    src/generator/test_codegenerator.c ^
    src/generator/codegenerator.c ^
    src/generator/arena.c ^
    -L"%TOML_LIB_DIR%" -ltoml -fuse-ld=lld

tests.exe