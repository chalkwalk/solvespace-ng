# Open CASCADE (extlib/occt, LGPL-2.1 with the OCCT exception) for the OCCT kernel.
#
# OCCT is built once per build directory as an external project, always Release (it is never
# stepped through, and a Debug build is several times larger), static, with only the toolkits
# in OCCT_TOOLKITS and what they depend on. Set OCCT_PREFIX to an existing install of the same
# toolkits (for example another build directory's occt/install) to skip that build.
#
# Defines the interface target `occt`, which links the toolkits in dependency order.

include(ExternalProject)
find_package(Threads REQUIRED)

set(OCCT_SOURCE_DIR ${CMAKE_SOURCE_DIR}/extlib/occt)
set(OCCT_TOOLKITS TKBool TKFillet TKOffset TKFeat TKMesh TKDESTEP)
set(OCCT_PREFIX "" CACHE PATH
    "An existing OCCT install to use instead of building extlib/occt")

if(NOT EXISTS ${OCCT_SOURCE_DIR}/CMakeLists.txt AND NOT OCCT_PREFIX)
    message(FATAL_ERROR "extlib/occt is missing: run `git submodule update --init extlib/occt`, "
                        "or configure with -DENABLE_OCCT=OFF")
endif()

# The toolkits OCCT_TOOLKITS need, each with the toolkits it links (from OCCT's EXTERNLIB files).
set(occt_pending ${OCCT_TOOLKITS})
set(occt_all)
while(occt_pending)
    list(POP_FRONT occt_pending tk)
    if(tk IN_LIST occt_all)
        continue()
    endif()
    list(APPEND occt_all ${tk})
    file(STRINGS ${OCCT_SOURCE_DIR}/src/${tk}/EXTERNLIB occt_lines)
    set(occt_deps_${tk})
    foreach(line ${occt_lines})
        if(line MATCHES "^TK")
            list(APPEND occt_deps_${tk} ${line})
            list(APPEND occt_pending ${line})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES occt_deps_${tk})
endwhile()

if(OCCT_PREFIX)
    set(OCCT_INSTALL ${OCCT_PREFIX})
    message(STATUS "Using OCCT from ${OCCT_INSTALL}")
else()
    set(OCCT_INSTALL ${CMAKE_BINARY_DIR}/occt/install)
    message(STATUS "Using in-tree OCCT (built Release on first build, about 10 minutes)")
    set(occt_byproducts)
    foreach(tk ${occt_all})
        list(APPEND occt_byproducts
            ${OCCT_INSTALL}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${tk}${CMAKE_STATIC_LIBRARY_SUFFIX})
    endforeach()
    string(REPLACE ";" " " occt_toolkit_string "${OCCT_TOOLKITS}")
    set(occt_modules)
    foreach(module FoundationClasses ModelingData ModelingAlgorithms Visualization
                   ApplicationFramework DataExchange Draw DETools)
        list(APPEND occt_modules -DBUILD_MODULE_${module}=OFF)
    endforeach()
    # Sanitizer builds keep OCCT's frame pointers, so leak reports and their suppressions
    # (test/harness.cpp) see its frames.
    set(occt_cxx_flags)
    if(ENABLE_SANITIZERS AND NOT MSVC)
        set(occt_cxx_flags -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer)
    endif()
    ExternalProject_Add(occt-external
        SOURCE_DIR       ${OCCT_SOURCE_DIR}
        PREFIX           ${CMAKE_BINARY_DIR}/occt
        BINARY_DIR       ${CMAKE_BINARY_DIR}/occt/build
        INSTALL_DIR      ${OCCT_INSTALL}
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            ${occt_cxx_flags}
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
            -DINSTALL_DIR=<INSTALL_DIR>
            -DINSTALL_DIR_LIB=lib
            -DINSTALL_DIR_INCLUDE=include/opencascade
            -DBUILD_LIBRARY_TYPE=Static
            ${occt_modules}
            -DBUILD_ADDITIONAL_TOOLKITS=${occt_toolkit_string}
            -DBUILD_DOC_Overview=OFF
            -DBUILD_RESOURCES=OFF
            -DUSE_TK=OFF
            -DUSE_FREETYPE=OFF
            -DUSE_OPENGL=OFF
            -DUSE_GLES2=OFF
            -DUSE_XLIB=OFF
            -DUSE_D3D=OFF
            -DUSE_FREEIMAGE=OFF
            -DUSE_OPENVR=OFF
            -DUSE_TBB=OFF
            -DUSE_VTK=OFF
            -DUSE_RAPIDJSON=OFF
            -DUSE_DRACO=OFF
        BUILD_BYPRODUCTS ${occt_byproducts}
        USES_TERMINAL_BUILD ON
        EXCLUDE_FROM_ALL ON)
endif()

file(MAKE_DIRECTORY ${OCCT_INSTALL}/include/opencascade)
foreach(tk ${occt_all})
    add_library(occt::${tk} STATIC IMPORTED)
    set_target_properties(occt::${tk} PROPERTIES
        IMPORTED_LOCATION
            ${OCCT_INSTALL}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${tk}${CMAKE_STATIC_LIBRARY_SUFFIX}
        INTERFACE_INCLUDE_DIRECTORIES ${OCCT_INSTALL}/include/opencascade)
endforeach()
foreach(tk ${occt_all})
    set(occt_links)
    foreach(dep ${occt_deps_${tk}})
        list(APPEND occt_links occt::${dep})
    endforeach()
    if(tk STREQUAL "TKernel")
        list(APPEND occt_links Threads::Threads ${CMAKE_DL_LIBS})
        if(WIN32)
            list(APPEND occt_links advapi32 gdi32 user32 wsock32 psapi)
        endif()
    endif()
    set_property(TARGET occt::${tk} PROPERTY INTERFACE_LINK_LIBRARIES ${occt_links})
endforeach()

add_library(occt INTERFACE)
foreach(tk ${OCCT_TOOLKITS})
    target_link_libraries(occt INTERFACE occt::${tk})
endforeach()
if(TARGET occt-external)
    add_dependencies(occt occt-external)
endif()
