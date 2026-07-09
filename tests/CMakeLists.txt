include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(dotenvy_test dotenvy_test.cpp)
target_link_libraries(
    dotenvy_test PRIVATE
    Dotenvy::dotenvy
    Catch2::Catch2WithMain
)

include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(dotenvy_test)
