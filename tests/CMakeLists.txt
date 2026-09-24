###############################################################################
# Serialization unit tests
#
# Can be built on its own (does not need OpenGL / GLUT / SDL):
#   cmake -S tests -B build/tests
#   cmake --build build/tests
#   ctest --test-dir build/tests --output-on-failure
#
# or from the root project with -DBUILD_SERIALIZATION_TESTS=ON
###############################################################################

cmake_minimum_required(VERSION 3.16)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(SerializationTests CXX)
	set(CMAKE_CXX_STANDARD 17)
	set(CMAKE_CXX_STANDARD_REQUIRED True)
	enable_testing()
endif()

set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/Game")
set(CONTEST_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/ContestAPI")

# Serialization system and headless scene editor core under test
file(GLOB SERIALIZATION_SRC_FILES "${GAME_DIR}/Serialization/*.cpp")
set(EDITOR_SRC_FILES
	"${GAME_DIR}/Editor/SceneEditor.cpp"
	"${GAME_DIR}/Prefabs.cpp"
)

# Minimal set of engine sources the serialized types depend on
set(ENGINE_SRC_FILES
	"${GAME_DIR}/Vec2.cpp"
	"${GAME_DIR}/Vec3.cpp"
	"${GAME_DIR}/Vec4.cpp"
	"${GAME_DIR}/Quat.cpp"
	"${GAME_DIR}/Mat2.cpp"
	"${GAME_DIR}/Mat3.cpp"
	"${GAME_DIR}/Mat4.cpp"
	"${GAME_DIR}/Transform.cpp"
	"${GAME_DIR}/RigidBody.cpp"
	"${GAME_DIR}/Utils.cpp"
	"${GAME_DIR}/Map.cpp"
	"${GAME_DIR}/Camera.cpp"
	"${GAME_DIR}/Material.cpp"
	"${GAME_DIR}/GameUtils.cpp"
)

# Headless engine + save system + editor core, shared by the test runner and
# the AuthorScenes tool
add_library(HeadlessGameCore STATIC
	${SERIALIZATION_SRC_FILES}
	${EDITOR_SRC_FILES}
	${ENGINE_SRC_FILES}
	"${CMAKE_CURRENT_SOURCE_DIR}/scenes/SampleScenes.cpp"
)

# The stub directory comes first so "app.h" resolves to the headless stub
target_include_directories(HeadlessGameCore PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/support/stubs"
	"${CMAKE_CURRENT_SOURCE_DIR}/support"
	"${CMAKE_CURRENT_SOURCE_DIR}/scenes"
	"${GAME_DIR}"
	"${CONTEST_API_DIR}"
)

target_compile_definitions(HeadlessGameCore PUBLIC
	BUILD_PLATFORM_WINDOWS=0
	BUILD_PLATFORM_APPLE=0
)

if (MSVC)
	target_compile_options(HeadlessGameCore PUBLIC "/FI${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h" /bigobj)
else()
	target_compile_options(HeadlessGameCore PUBLIC -include "${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h")
endif()

# GCC rejects members named after their own type (e.g. `Shape Shape;` in
# RigidBody) which MSVC and Clang accept, downgrade that to a warning
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(HeadlessGameCore PUBLIC -fpermissive -Wno-changes-meaning)
endif()

find_package(Threads)
if (Threads_FOUND)
	target_link_libraries(HeadlessGameCore PUBLIC Threads::Threads)
endif()

# -- Test runner ----------------------------------------------------------------

file(GLOB TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# The editor GUI runs headless against the stubbed App API (support/AppStub.cpp)
set(GUI_SRC_FILES
	"${GAME_DIR}/Editor/SceneEditorScene.cpp"
	"${GAME_DIR}/Widget.cpp"
	"${GAME_DIR}/UIStateManager.cpp"
	"${GAME_DIR}/Lights.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/support/AppStub.cpp"
)
add_executable(SerializationTests ${TEST_SRC_FILES} ${GUI_SRC_FILES})
target_link_libraries(SerializationTests PRIVATE HeadlessGameCore)
# Committed scene files verified by SceneFileTests.cpp
target_compile_definitions(SerializationTests PRIVATE
	SCENES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/scenes"
)

add_test(NAME SerializationTests COMMAND SerializationTests)
set_tests_properties(SerializationTests PROPERTIES
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

# -- Scene authoring tool ---------------------------------------------------------
# Regenerates the sample scenes in data/scenes with the headless scene editor:
#   cmake --build build/tests --target author_scenes

add_executable(AuthorScenes "${CMAKE_CURRENT_SOURCE_DIR}/tools/AuthorScenes.cpp")
target_link_libraries(AuthorScenes PRIVATE HeadlessGameCore)

add_custom_target(author_scenes
	COMMAND AuthorScenes "${CMAKE_CURRENT_SOURCE_DIR}/../data/scenes"
	DEPENDS AuthorScenes
	COMMENT "Authoring sample scenes into data/scenes"
)
