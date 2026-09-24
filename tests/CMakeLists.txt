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

# Serialization system under test
file(GLOB SERIALIZATION_SRC_FILES "${GAME_DIR}/Serialization/*.cpp")

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
)

file(GLOB TEST_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(SerializationTests
	${TEST_SRC_FILES}
	${SERIALIZATION_SRC_FILES}
	${ENGINE_SRC_FILES}
)

# The stub directory comes first so "app.h" resolves to the headless stub
target_include_directories(SerializationTests PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/support/stubs"
	"${CMAKE_CURRENT_SOURCE_DIR}/support"
	"${GAME_DIR}"
	"${CONTEST_API_DIR}"
)

target_compile_definitions(SerializationTests PRIVATE
	BUILD_PLATFORM_WINDOWS=0
	BUILD_PLATFORM_APPLE=0
)

if (MSVC)
	target_compile_options(SerializationTests PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h" /bigobj)
else()
	target_compile_options(SerializationTests PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/support/TestCompat.h")
endif()

# GCC rejects members named after their own type (e.g. `Shape Shape;` in
# RigidBody) which MSVC and Clang accept, downgrade that to a warning
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(SerializationTests PRIVATE -fpermissive -Wno-changes-meaning)
endif()

find_package(Threads)
if (Threads_FOUND)
	target_link_libraries(SerializationTests PRIVATE Threads::Threads)
endif()

add_test(NAME SerializationTests COMMAND SerializationTests)
set_tests_properties(SerializationTests PROPERTIES
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
