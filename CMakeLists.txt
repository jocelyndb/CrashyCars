cmake_minimum_required(VERSION 3.1)
project(crashy_cars)

#set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures for macOS")

# Set c++11
# https://stackoverflow.com/questions/10851247/how-to-activate-c-11-in-cmake
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif ()
set (CMAKE_CXX_STANDARD 14)

# nice hierarchichal structure in MSVC
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#Find OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(IS_OS_MAC 1)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(IS_OS_LINUX 1)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(IS_OS_WINDOWS 1)
else()
  message(FATAL_ERROR "OS ${CMAKE_SYSTEM_NAME} was not recognized")
endif()

# Create executable target

# Generate the shader folder location to the header
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ext/project_path.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/ext/project_path.hpp")

# You can switch to use the file GLOB for simplicity but at your own risk
file(GLOB SOURCE_FILES src/*.cpp src/*.hpp)

# external libraries will be installed into /usr/local/include and /usr/local/lib but that folder is not automatically included in the search on MACs
if (IS_OS_MAC)
    set(ENV{MACOSX_DEPLOYMENT_TARGET} 10.6)
  include_directories(/usr/local/include)
  link_directories(/usr/local/lib)
  # 2024-09-24 - added for M-series Mac's
  include_directories(/opt/homebrew/include)
  link_directories(/opt/homebrew/lib)
endif()

file(GLOB_RECURSE
        MY_RESOURCE_FILES
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        CONFIGURE_DEPENDS

        ${CMAKE_CURRENT_SOURCE_DIR}/data/**.wav    # adapt these to your needs
        ${CMAKE_CURRENT_SOURCE_DIR}/data/**.mp3
        ${CMAKE_CURRENT_SOURCE_DIR}/data/**.obj
        ${CMAKE_CURRENT_SOURCE_DIR}/data/**.png
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/**.fs.glsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/**.vs.glsl
)
set_source_files_properties(${MY_RESOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

#FOREACH(MY_RESOURCE_FILE ${MY_RESOURCE_FILES})
#    add_custom_command(
#            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE}
#            COMMAND ${CMAKE_COMMAND} -E copy
#            ${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE}
#            ${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE}
#            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE}
#    )
#    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${MY_RESOURCE_FILE})
#ENDFOREACH()

#set(APP_NAME "Crashy Cars")
set(APP_ICON_PATH "${CMAKE_SOURCE_DIR}/data/icons/car_icon.icns")
set(APP_ICON_NAME "car_icon.icns")

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SOURCE_FILES}
        ${APP_ICON_PATH}
        src/state_system.cpp
        src/state_system.h)

FOREACH(MY_RESOURCE_FILE ${MY_RESOURCE_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${MY_RESOURCE_FILE} "$<TARGET_FILE_DIR:${PROJECT_NAME}>/../Resources/${MY_RESOURCE_FILE}"
    )
endforeach ()

set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER com.jcln.${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
)
set_source_files_properties(${APP_ICON_PATH} PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE ${APP_ICON_NAME} )
file( COPY ${ICON_PATH} DESTINATION "crashy_cars.app/Contents/Resources/" )
install(FILES ${APP_ICON_PATH}
        DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}.app/Contents/Resources/")

target_include_directories(${PROJECT_NAME} PUBLIC src/)

# Added this so policy CMP0065 doesn't scream
set_target_properties(${PROJECT_NAME} PROPERTIES ENABLE_EXPORTS 0)

# External header-only libraries in the ext/
target_include_directories(${PROJECT_NAME} PUBLIC ext/stb_image/)
target_include_directories(${PROJECT_NAME} PUBLIC ext/gl3w)

# Find OpenGL
find_package(OpenGL REQUIRED)

if (OPENGL_FOUND)
   target_include_directories(${PROJECT_NAME} PUBLIC ${OPENGL_INCLUDE_DIR})
   target_link_libraries(${PROJECT_NAME} PUBLIC ${OPENGL_gl_LIBRARY})
endif()

set(glm_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/glm/cmake/glm) # if necessary
find_package(glm REQUIRED)

# GLFW, SDL2 could be precompiled (on windows) or installed by a package manager (on OSX and Linux)
if (IS_OS_LINUX OR IS_OS_MAC)
    # Try to find packages rather than to use the precompiled ones
    # Since we're on OSX or Linux, we can just use pkgconfig.
    find_package(PkgConfig REQUIRED)

    pkg_search_module(GLFW REQUIRED glfw3)

    pkg_search_module(SDL2 REQUIRED sdl2)
    pkg_search_module(SDL2MIXER REQUIRED SDL2_mixer)

    # Link Frameworks on OSX
    if (IS_OS_MAC)
        find_library(COCOA_LIBRARY Cocoa)
        find_library(CF_LIBRARY CoreFoundation)
        target_link_libraries(${PROJECT_NAME} PUBLIC ${COCOA_LIBRARY} ${CF_LIBRARY})
    endif()

    # Increase warning level
    target_compile_options(${PROJECT_NAME} PUBLIC "-Wall")
elseif (IS_OS_WINDOWS)
# https://stackoverflow.com/questions/17126860/cmake-link-precompiled-library-depending-on-os-and-architecture
    set(GLFW_FOUND TRUE)
    set(SDL2_FOUND TRUE)

    set(GLFW_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/include")
    set(SDL2_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/include/SDL")

    if (${CMAKE_SIZEOF_VOID_P} MATCHES "8")
        set(GLFW_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/lib/glfw3dll-x64.lib")
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2-x64.lib")
	set(SDL2MIXER_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2_mixer-x64.lib")

	set(GLFW_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/lib/glfw3-x64.dll")
	set(SDL2_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2-x64.dll")
	set(SDL2MIXER_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2_mixer-x64.dll")
    else()
        set(GLFW_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/lib/glfw3dll-x86.lib")
        set(SDL2_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2-x86.lib")
	set(SDL2MIXER_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2_mixer-x86.lib")

	set(GLFW_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/lib/glfw3-x86.dll")
	set(SDL2_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2-x86.dll")
	set(SDL2MIXER_DLL "${CMAKE_CURRENT_SOURCE_DIR}/ext/sdl/lib/SDL2_mixer-x86.dll")
    endif()

    # Copy and rename dlls
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLFW_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/glfw3.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2MIXER_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2_mixer.dll")

    target_compile_options(${PROJECT_NAME} PUBLIC
        # increase warning level
        "/W4"

        # Turn warning "not all control paths return a value" into an error
        "/we4715"

        # use sane exception handling, rather than trying to catch segfaults and allowing resource
        # leaks and UB. Yup... See "Default exception handling behavior" at
        # https://docs.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=vs-2019
        "/EHsc"

        # turn warning C4239 (non-standard extension that allows temporaries to be bound to
        # non-const references, yay microsoft) into an error
        "/we4239"
    )
endif()

# Can't find the include and lib. Quit.
if (NOT GLFW_FOUND OR NOT SDL2_FOUND)
   if (NOT GLFW_FOUND)
      message(FATAL_ERROR "Can't find GLFW." )
   else ()
      message(FATAL_ERROR "Can't find SDL2." )
   endif()
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${GLFW_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${SDL2_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC ${GLFW_LIBRARIES} ${SDL2_LIBRARIES} ${SDL2MIXER_LIBRARIES} glm::glm)

# Needed to add this
if(IS_OS_LINUX)
  target_link_libraries(${PROJECT_NAME} PUBLIC glfw ${CMAKE_DL_LIBS})
endif()
