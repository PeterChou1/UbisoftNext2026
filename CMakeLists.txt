cmake_minimum_required(VERSION 4.0)

# Set the C++ version to be C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(UbiTorContestAPI)

set(VENDOR_SRC_DIR "${PROJECT_SOURCE_DIR}/src/ContestAPI/vendor")

###############################################################################
# Common Project for Setup
# This project exists to provide common definitions and config for all platforms
# Most notably, this includes platform definitions
###############################################################################

add_library(Common INTERFACE)

set(IS_PLATFORM_WINDOWS 0)
set(IS_PLATFORM_APPLE 0)

if (CMAKE_SYSTEM_NAME MATCHES Windows)
	set(IS_PLATFORM_WINDOWS 1)
endif()

if (CMAKE_SYSTEM_NAME MATCHES Apple)
	set(IS_PLATFORM_APPLE 1)
endif()

target_compile_definitions(Common INTERFACE 
    BUILD_PLATFORM_WINDOWS=${IS_PLATFORM_WINDOWS}
	BUILD_PLATFORM_APPLE=${IS_PLATFORM_APPLE}
	GL_SILENCE_DEPRECATION
)

#homebrew installs to /usr/local on intel macs and /opt/homebrew on apple chip macs
#This sets it up to find SDL3 and link it
if (CMAKE_SYSTEM_NAME MATCHES Apple)
	target_include_directories(Common INTERFACE
		"/usr/local/include"
		"/opt/homebrew/include"
	)
	target_link_directories(Common INTERFACE
		"/usr/local/lib"
		"/opt/homebrew/lib"
	)
endif()

message(STATUS "System Name is ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler Name is ${CMAKE_CXX_COMPILER_ID}")


###############################################################################
#FreeGLUT lib/dll
#On windows, we need to link the freeglut dll 
###############################################################################

set(FREEGLUT_INC_DIR "")

if (CMAKE_SYSTEM_NAME MATCHES Windows)
	set(FREEGLUT_ROOT_DIR "${VENDOR_SRC_DIR}/glut/")

	#Here we declare that this is an imported lib, and configure the .lib and .dll files
	#This allows us to copy the DLL to the build directory
	add_library(FreeGLUT SHARED IMPORTED)
	set_target_properties(FreeGLUT PROPERTIES
		IMPORTED_LOCATION "${FREEGLUT_ROOT_DIR}/bin/x64/freeglut.dll"
		IMPORTED_IMPLIB "${FREEGLUT_ROOT_DIR}/lib/x64/freeglut.lib"
	)
	set(FREEGLUT_LIB_NAME "FreeGLUT")
	set(FREE_GLUT_INC_DIR "${FREEGLUT_ROOT_DIR}/include")
endif()

###############################################################################
#SDL3 lib - need on macos for controller support
###############################################################################

if (CMAKE_SYSTEM_NAME MATCHES Apple)
	find_package(SDL3 REQUIRED)
endif()

###############################################################################
#Contest API library
#This configures the contest api as it's own project
###############################################################################

message(STATUS "Project Source Dir is ${PROJECT_SOURCE_DIR}")
message(STATUS "Vendor Source Dir is ${VENDOR_SRC_DIR}")

file(GLOB CONTEST_API_INC_FILES ${PROJECT_SOURCE_DIR}/src/ContestAPI/*.h)
file(GLOB CONTEST_API_SRC_FILES ${PROJECT_SOURCE_DIR}/src/ContestAPI/*.cpp)
file(GLOB MINIAUDIO_SRC_FILES ${VENDOR_SRC_DIR}/miniaudio/*.cpp)
file(GLOB STB_IMAGE_SRC_FILES ${VENDOR_SRC_DIR}/stb_image/*.cpp)

add_library(ContestAPI 
	${CONTEST_API_INC_FILES} 
	${CONTEST_API_SRC_FILES} 
	${MINIAUDIO_SRC_FILES} 
	${STB_IMAGE_SRC_FILES} 
)

target_include_directories(ContestAPI PUBLIC 
	"${PROJECT_SOURCE_DIR}/src/ContestAPI"
)

target_include_directories(ContestAPI PRIVATE 
	"${FREE_GLUT_INC_DIR}"
	"${VENDOR_SRC_DIR}/miniaudio"
	"${VENDOR_SRC_DIR}/stb_image"
)

target_link_libraries(ContestAPI PUBLIC Common)

# For Apple, we need to specify that we link with GLUT, OpenGL and libC++
# For the different compilers, specifying the c++ lib has different arguments
if (CMAKE_SYSTEM_NAME MATCHES Apple)
	
	target_link_libraries(ContestAPI PUBLIC SDL3)

	target_link_libraries(ContestAPI PUBLIC "-framework GLUT -framework OpenGL")

	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++")
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	 	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++")
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=stdlibc++")
	else()
		message(FATAL_ERROR "Unhandled Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
	endif()

endif()

# For windows, we can link with the custom freeglut target we defined
if (CMAKE_SYSTEM_NAME MATCHES Windows)
	target_link_directories(ContestAPI PUBLIC "${VENDOR_SRC_DIR}/glut/lib/x64")
	target_link_libraries(ContestAPI PUBLIC FreeGLUT)
endif()

###############################################################################
# Engine library
# ECS, math, physics, 3D renderer, UI widgets, serialization, scene objects and
# the C++ scripting system. Shared by the SceneEditor, the Game and the tests
###############################################################################

file(GLOB_RECURSE ENGINE_FILES
	${PROJECT_SOURCE_DIR}/src/Engine/*.h
	${PROJECT_SOURCE_DIR}/src/Engine/*.cpp
)

add_library(Engine STATIC ${ENGINE_FILES})

target_include_directories(Engine PUBLIC
	"${PROJECT_SOURCE_DIR}/src/Engine"
	"${FREE_GLUT_INC_DIR}"
)

target_link_libraries(Engine PUBLIC Common ContestAPI)

###############################################################################
# GameScripts library
# The project's C++ scripts (object behaviours + scene scripts). Linked by the
# Game (to run scenes) and by the SceneEditor (to attach / play them)
###############################################################################

file(GLOB_RECURSE GAME_SCRIPT_FILES
	${PROJECT_SOURCE_DIR}/src/Game/Scripts/*.h
	${PROJECT_SOURCE_DIR}/src/Game/Scripts/*.cpp
)

add_library(GameScripts STATIC
	${GAME_SCRIPT_FILES}
	"${PROJECT_SOURCE_DIR}/src/Game/GameScripts.h"
	"${PROJECT_SOURCE_DIR}/src/Game/GameScripts.cpp"
)

target_include_directories(GameScripts PUBLIC "${PROJECT_SOURCE_DIR}/src/Game")
target_link_libraries(GameScripts PUBLIC Engine)

###############################################################################
# Game executable: plays the scenes in data/scenes with the scripts
###############################################################################

add_executable(Game WIN32
	"${PROJECT_SOURCE_DIR}/src/Game/GameMain.cpp"
	"${PROJECT_SOURCE_DIR}/src/Game/SceneMenu.h"
	"${PROJECT_SOURCE_DIR}/src/Game/SceneMenu.cpp"
)

target_link_libraries(Game PRIVATE GameScripts)

###############################################################################
# SceneEditor executable: authors scenes (2D shapes / models, scripts)
###############################################################################

file(GLOB EDITOR_FILES
	${PROJECT_SOURCE_DIR}/src/Editor/*.h
	${PROJECT_SOURCE_DIR}/src/Editor/*.cpp
)

add_executable(SceneEditor WIN32 ${EDITOR_FILES})

target_include_directories(SceneEditor PRIVATE "${PROJECT_SOURCE_DIR}/src/Editor")
target_link_libraries(SceneEditor PRIVATE GameScripts)

# Both programs run from the repository root so they find data/
set(APP_TARGETS Game SceneEditor)

# 'make run' / 'make run_editor' on MacOS
if (CMAKE_SYSTEM_NAME MATCHES Apple)
	add_custom_target(run
		DEPENDS Game
		COMMAND "$<TARGET_FILE:Game>"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	)
	add_custom_target(run_editor
		DEPENDS SceneEditor
		COMMAND "$<TARGET_FILE:SceneEditor>"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	)
endif()

if (CMAKE_SYSTEM_NAME MATCHES Windows)
	foreach(APP ${APP_TARGETS})
		#Sets the program to run in the root working directory
		set_target_properties(${APP} PROPERTIES
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		)
		# Copy runtime DLLS (freeglut) next to the executable
		if (CMAKE_IMPORT_LIBRARY_SUFFIX)
			add_custom_command(
				TARGET ${APP} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_RUNTIME_DLLS:${APP}>" "$<TARGET_FILE_DIR:${APP}>"
				COMMAND_EXPAND_LISTS
			)
		endif()
	endforeach()

	#Sets the VS startup project. If this is not called then the startup project is ALL_BUILD which results in an error in VS
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Game)
endif()

###############################################################################
# Unit tests (optional)
# Enable with: cmake -B build/tests-root -DBUILD_TESTS=ON
# The tests are headless (no OpenGL/GLUT/SDL) and can also be built on their
# own from the tests/ directory, see tests/CMakeLists.txt
###############################################################################

option(BUILD_TESTS "Build the engine / editor / script unit tests" OFF)

if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
