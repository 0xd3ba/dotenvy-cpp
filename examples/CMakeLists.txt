add_executable(dotenvy_example dotenvy_example.cpp)
target_link_libraries(dotenvy_example PRIVATE Dotenvy::dotenvy)

# Keep the demo's example.env next to the built binary so
# `./dotenvy_demo` works from the build directory without an extra setup
add_custom_command(
    TARGET dotenvy_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/examples/example.env
    $<TARGET_FILE_DIR:dotenvy_example>/example.env
)
