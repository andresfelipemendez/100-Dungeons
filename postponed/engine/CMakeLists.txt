cmake_minimum_required(VERSION 3.12)

project(AnitraEngine)

# Set C99 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(include)

# Source files for the main executable
set(SRC_FILES src/main.cpp)

# Add platform-specific source files
if(WIN32)
  list(APPEND SRC_FILES src/core/loadlibrary_windows.cpp)
elseif(UNIX)
  list(APPEND SRC_FILES src/core/loadlibrary_linux.cpp)
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_SOURCE_DIR}/lib/glfw/lib/glfw3.dll"
		$<TARGET_FILE_DIR:${PROJECT_NAME}>)

find_package(OpenGL REQUIRED)

set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/glfw/include")
set(GLFW_LIB_PATH    "${CMAKE_SOURCE_DIR}/lib/glfw/lib/glfw3dll.lib")

set(TOML_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/toml")
set(TOML_SRC_FILES "${CMAKE_SOURCE_DIR}/lib/toml/toml.c")

set(GLM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/glm")

set(GLEW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/toml")
set(GLEW_SRC_FILES "${CMAKE_SOURCE_DIR}/lib/toml/toml.c")

add_library(toml STATIC ${TOML_SRC_FILES})

set(FASTGLTF_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/fastgltf/include")
set(FASTGLTF_LIB_PATH    "${CMAKE_SOURCE_DIR}/lib/fastgltf/lib/fastgltf.lib")

set(FASTGLTF_LIB_PATH    "${CMAKE_SOURCE_DIR}/lib/fastgltf/lib/fastgltf.lib")

set(STB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/stb")

file(GLOB GLAD_SRC_FILES "${CMAKE_SOURCE_DIR}/lib/glad/*.c")
set(GLAD_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lib/glad")

add_library(glad STATIC ${GLAD_SRC_FILES})
target_include_directories(glad PRIVATE ${OPENGL_INCLUDE_DIR} ${GLAD_INCLUDE_DIR})

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/lib/imgui-1.90.9")

target_include_directories(
		${PROJECT_NAME} PRIVATE 
		"${CMAKE_SOURCE_DIR}/include" 
		"${OPENGL_INCLUDE_DIR}" 
		"${GLFW_INCLUDE_DIR}"
	)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_link_options(${PROJECT_NAME} PRIVATE "/NODEFAULTLIB:MSVCRT")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES} ${GLFW_LIB_PATH})

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE gdi32 user32 shell32)
endif()

set(EXTERNALS_SRC_FILES
    src/externals/externals.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

add_library(externals SHARED ${EXTERNALS_SRC_FILES})

target_include_directories(
		externals
		PRIVATE 
		"${CMAKE_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/src/core"
		"${CMAKE_SOURCE_DIR}/src/engine"
		"${CMAKE_SOURCE_DIR}/src/externals"
		"${IMGUI_DIR}"
		"${IMGUI_DIR}/backends"
		"${OPENGL_INCLUDE_DIR}"
		"${GLFW_INCLUDE_DIR}"
		"${FLECS_INCLUDE_DIR}"
		"${TOML_INCLUDE_DIR}"
		"${GLAD_INCLUDE_DIR}"
		"${GLM_INCLUDE_DIR}"
		"${FASTGLTF_INCLUDE_DIR}"
	)

target_link_libraries(externals PRIVATE ${OPENGL_LIBRARIES} ${GLFW_LIB_PATH} toml glad)

set(CORE_SRC_FILES src/core/core.cpp)
if(WIN32)
  list(APPEND CORE_SRC_FILES src/core/loadlibrary_windows.cpp)
elseif(UNIX)
  list(APPED CORE_SRC_FILES src/core/loadlibrary_linux.cpp)
endif()

add_library(core SHARED ${CORE_SRC_FILES})

target_include_directories(
		core
		PRIVATE 
		"${CMAKE_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/src/engine"
		"${CMAKE_SOURCE_DIR}/src/core"
		"${CMAKE_SOURCE_DIR}/src/externals"
		"${CMAKE_SOURCE_DIR}/src"          
		"${IMGUI_DIR}"
		"${IMGUI_DIR}/backends"
		"${OPENGL_INCLUDE_DIR}"
		"${GLFW_INCLUDE_DIR}"
		"${TOML_INCLUDE_DIR}"
	)

target_link_libraries(core PRIVATE ${OPENGL_LIBRARIES} ${GLFW_LIB_PATH} ${RYML_LIB_PATH} toml)

file(GLOB ENGINE_SRC_FILES "src/engine/*.cpp")

set(IMGUI_SRC_FILES 
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

file(GLOB ENGINE_HEADER_FILES "src/engine/*.h")

add_library(engine SHARED ${ENGINE_SRC_FILES}  ${IMGUI_SRC_FILES})
target_compile_definitions(engine PRIVATE _ITERATOR_DEBUG_LEVEL=0)
set_target_properties(engine PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"  # Use /MD to match release settings
)
target_include_directories(
		engine PRIVATE 
		"${CMAKE_SOURCE_DIR}/src/engine"
		"${CMAKE_SOURCE_DIR}/src/core"
		"${CMAKE_SOURCE_DIR}/src/externals"
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_SOURCE_DIR}/include"
		"${IMGUI_DIR}"
		"${IMGUI_DIR}/backends"
		"${GLM_INCLUDE_DIR}"
		"${OPENGL_INCLUDE_DIR}"
		"${GLFW_INCLUDE_DIR}"
		"${FLECS_INCLUDE_DIR}"
		"${TOML_INCLUDE_DIR}"
		"${FASTGLTF_INCLUDE_DIR}"
		"${GLAD_INCLUDE_DIR}"
		"${STB_INCLUDE_DIR}"
	)

target_link_libraries(
		engine PRIVATE 
		${OPENGL_LIBRARIES} 
		${GLFW_LIB_PATH} 
		${FASTGLTF_LIB_PATH} 
		toml
		glad
	)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c23")

set(CODEGENERATOR_SRC_FILES
    src/generator/codegenerator.c
)

add_library(codegenerator STATIC ${CODEGENERATOR_SRC_FILES})
target_include_directories(codegenerator PRIVATE "${CMAKE_SOURCE_DIR}/include" "${TOML_INCLUDE_DIR}")
target_compile_features(codegenerator PUBLIC c_std_23)  # Use C23 standard

# Add a target for the test executable
set(TEST_SRC_FILES
    src/generator/test_codegenerator.c
)

add_executable(tests ${TEST_SRC_FILES})
target_compile_features(tests PUBLIC c_std_23)  # Use C23 standard
target_include_directories(tests PRIVATE "${CMAKE_SOURCE_DIR}/include" "${TOML_INCLUDE_DIR}")
target_link_libraries(tests PRIVATE codegenerator toml)

# Add warning and diagnostic flags for tests
target_compile_options(tests PRIVATE -Wall -Wextra -pedantic)

# Custom command to run the tests
add_custom_target(run_tests
    COMMAND tests
    DEPENDS tests
    COMMENT "Running code generator tests..."
)