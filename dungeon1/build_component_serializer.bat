cls
call "%~dp0env_vars.bat" 

clang -std=c23 -Wall -Wextra -pedantic -g -I"%TOML_INCLUDE_DIR%" ^
     -o "%OUTPUT_PATH%\componentserializer.exe" ^
    "%GENERATOR_SRC_PATH%\componentserializer.c" ^
    "%GENERATOR_SRC_PATH%\codegenerator.c" ^
    "%GENERATOR_SRC_PATH%\arena.c" ^
    -L"%TOML_LIB_DIR%" -ltoml -fuse-ld=lld -Xlinker /subsystem:console
