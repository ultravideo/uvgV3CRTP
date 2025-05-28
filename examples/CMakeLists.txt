project(examples)

add_executable(simple_example)

# Sources
target_sources(simple_example PRIVATE simple_example.cpp)

target_link_libraries(simple_example PRIVATE v3crtplib)