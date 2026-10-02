cmake_minimum_required(VERSION 3.20)

project(dev_challenge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FRAMEWORK_SOURCES
        src/antworld.cpp
        src/useable_functions.cpp
)

set(APPLICANT_SOURCES
        src/applicant_solution.cpp
        src/analysis.cpp
        src/utilities.cpp
        src/signalSearch.cpp
        src/simpleSearch.cpp
)

add_executable(dev_challenge
        src/main.cpp
        ${APPLICANT_SOURCES}
        ${FRAMEWORK_SOURCES}
)

target_include_directories(dev_challenge PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(dev_challenge PRIVATE /W4)
else ()
    target_compile_options(dev_challenge PRIVATE -Wall -Wextra -Wpedantic)
endif ()

include(CTest)

if (BUILD_TESTING)
    add_executable(antworld_tests
            test/antworld_tests.cpp
            ${APPLICANT_SOURCES}
            ${FRAMEWORK_SOURCES}
    )

    target_include_directories(antworld_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if (MSVC)
        target_compile_options(antworld_tests PRIVATE /W4)
    else ()
        target_compile_options(antworld_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif ()

    add_test(
            NAME antworld_framework_tests
            COMMAND antworld_tests
    )

    # --- analysis tests ---
    add_executable(analysis_tests
            test/analysis_tests.cpp
            ${APPLICANT_SOURCES}
            ${FRAMEWORK_SOURCES}
    )
    target_include_directories(analysis_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if (MSVC)
        target_compile_options(analysis_tests PRIVATE /W4)
    else ()
        target_compile_options(analysis_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME analysis_tests COMMAND analysis_tests)

    # --- simple search tests ---
    add_executable(simple_search_tests
            test/simple_search_tests.cpp
            ${APPLICANT_SOURCES}
            ${FRAMEWORK_SOURCES}
    )
    target_include_directories(simple_search_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if (MSVC)
        target_compile_options(simple_search_tests PRIVATE /W4)
    else ()
        target_compile_options(simple_search_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME simple_search_tests COMMAND simple_search_tests)

    # --- territory tests ---
    add_executable(territory_tests
            test/territory_tests.cpp
            ${APPLICANT_SOURCES}
            ${FRAMEWORK_SOURCES}
    )
    target_include_directories(territory_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if (MSVC)
        target_compile_options(territory_tests PRIVATE /W4)
    else ()
        target_compile_options(territory_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
    add_test(NAME territory_tests COMMAND territory_tests)

    # --- compare searches ---
    add_executable(compare_searches
            test/compare_searches.cpp
            ${APPLICANT_SOURCES}
            ${FRAMEWORK_SOURCES}
    )
    target_include_directories(compare_searches PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    if (MSVC)
        target_compile_options(compare_searches PRIVATE /W4)
    else ()
        target_compile_options(compare_searches PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endif ()
