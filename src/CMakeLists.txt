cmake_minimum_required(VERSION 2.8)

# uncomment to get more insight into the build process
#set(CMAKE_VERBOSE_MAKEFILE ON)


set(PLUGIN_NAME     "gimp-exr-plugin")
set(GIMP_PLUGIN_DIR $ENV{HOME}/.gimp-2.8/plug-ins)

# use the gimp tool to figure out some compiler flags (done before building)
exec_program(gimptool-2.0
             ARGS --cflags
             OUTPUT_VARIABLE GIMP_CXX_FLAGS)
exec_program(gimptool-2.0
             ARGS --libs
             OUTPUT_VARIABLE GIMP_LD_FLAGS)

add_definitions(${GIMP_CXX_FLAGS})

include_directories(/usr/include/OpenEXR)

set(SOURCES 
    conversion.cpp
    exr_file.cpp
    plugin.cpp)

add_executable(${PLUGIN_NAME} ${SOURCES})

target_link_libraries(${PLUGIN_NAME} ${GIMP_LD_FLAGS} IlmImf)

install(TARGETS ${PLUGIN_NAME}
        DESTINATION ${GIMP_PLUGIN_DIR})
