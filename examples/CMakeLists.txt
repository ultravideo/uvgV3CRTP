project(examples)

add_executable(simple_sender_example)

# Sources
target_sources(simple_sender_example PRIVATE simple_sender_example.cpp)

target_link_libraries(simple_sender_example PRIVATE v3crtplib)