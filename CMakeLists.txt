cmake_minimum_required(VERSION 3.1)
project(endorsement_scores)

set (CMAKE_CXX_STANDARD 14)

# set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
# set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
# @TODO set up more general system for debug/release builds
SET(CMAKE_CXX_FLAGS "-g -fno-omit-frame-pointer -fsanitize=address,undefined ${CMAKE_CXX_FLAGS}")
# SET(CMAKE_CXX_FLAGS "-fsanitize=address,undefined ${CMAKE_CXX_FLAGS}")

find_package(cpr QUIET)
if (NOT cpr_FOUND)
	message (STATUS "cpr not found, building from source")
	add_subdirectory(libraries/cpr)
endif()

find_package(nlohmann_json 3.10.5 QUIET)
if (NOT nlohmann_json_FOUND)
	message ("nlohmann json not found, building from source")
	add_subdirectory(libraries/json)
endif()

add_subdirectory(libraries/liboauthcpp/build)

# @TODO mongoose.c is symlinked from libraries/mongoose; this is janky, find a better way
add_executable(endorsement_score main.cpp util.cpp endorsement_score.cpp oauth.cpp server.cpp mongoose.c)

target_include_directories(endorsement_score PRIVATE libraries/liboauthcpp/include)
target_link_libraries(endorsement_score PRIVATE cpr::cpr PRIVATE nlohmann_json::nlohmann_json PRIVATE oauthcpp PRIVATE)
