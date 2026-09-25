###############################################################################
# Unit tests: engine, serialization, scripting, scene editor, game scripts
#
# Headless: the ContestAPI (OpenGL / GLUT / SDL) is replaced by a scriptable
# stub (support/AppStub.cpp) that also captures the frames the renderer draws.
# The real engine, editor and scripts are compiled and run frame by frame.
#
#   cmake -S tests -B build/tests
#   cmake --build build/tests
#   ctest --test-dir build/tests --output-on-failure
#
# or from the root project with -DBUILD_TESTS=ON
###############################################################################

cmake_minimum_required(VERSION 3.16)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(EngineTests CXX)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED True)
	enable_testing()
endif()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# Everything except the programs' entry points (ContestAPI main / Init ...)
file(GLOB_RECURSE HEADLESS_SRC_FILES
	"${SRC_DIR}/Engine/*.cpp"
	"${SRC_DIR}/Game/Scripts/*.cpp"
)
list(APPEND HEADLESS_SRC_FILES
	"${SRC_DIR}/Game/GameScripts.cpp"
	"${SRC_DIR}/Game/SceneMenu.cpp"
	"${SRC_DIR}/Editor/SceneEditor.cpp"
	"${SRC_DIR}/Editor/SceneEditorScene.cpp"
	"${SRC_DIR}/Editor/ContextMenu.cpp"
	"${SRC_DIR}/Editor/EditorLeftPanel.cpp"
	"${SRC_DIR}/Editor/EditorInspector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/support/AppStub.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/support/TestEnvironment.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/scenes/SampleScenes.cpp"
)

add_library(HeadlessEngine STATIC ${HEADLESS_SRC_FILES})

# The stub directory comes first so "app.h" resolves to the headless stub
target_include_directories(HeadlessEngine PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/support/stubs"
	"${CMAKE_CURRENT_SOURCE_DIR}/support"
	"${CMAKE_CURRENT_SOURCE_DIR}/scenes"
	"${SRC_DIR}/Engine"
	"${SRC_DIR}/Game"
	"${SRC_DIR}/Editor"
	"${SRC_DIR}/ContestAPI"
)

target_compile_definitions(HeadlessEngine PUBLIC
	BUILD_PLATFORM_WINDOWS=0
	BUILD_PLATFORM_APPLE=0
)

if (MSVC)
	target_compile_options(HeadlessEngine PUBLIC "/FI${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h" /bigobj)
else()
	target_compile_options(HeadlessEngine PUBLIC -include "${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h")
endif()

# GCC rejects members named after their own type (e.g. `Shape Shape;` in
# RigidBody) which MSVC and Clang accept, downgrade that to a warning
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(HeadlessEngine PUBLIC -fpermissive -Wno-changes-meaning)
endif()

find_package(Threads)
if (Threads_FOUND)
	target_link_libraries(HeadlessEngine PUBLIC Threads::Threads)
endif()

# -- Test runner ------------------------------------------------------------------

file(GLOB TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(EngineTests ${TEST_SRC_FILES})
target_link_libraries(EngineTests PRIVATE HeadlessEngine)
target_compile_definitions(EngineTests PRIVATE
	REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_test(NAME EngineTests COMMAND EngineTests)
set_tests_properties(EngineTests PROPERTIES
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

# -- Scene authoring tool ---------------------------------------------------------
# Writes the sample scenes (tests/scenes/SampleScenes.cpp) to data/scenes with
# the headless scene editor:  cmake --build build/tests --target author_scenes

add_executable(AuthorScenes "${CMAKE_CURRENT_SOURCE_DIR}/tools/AuthorScenes.cpp")
target_link_libraries(AuthorScenes PRIVATE HeadlessEngine)

add_custom_target(author_scenes
	COMMAND AuthorScenes "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenes" "${CMAKE_CURRENT_SOURCE_DIR}/../data/prefabs"
	DEPENDS AuthorScenes
	COMMENT "Authoring sample scenes into data/scenes and prefabs into data/prefabs"
)
