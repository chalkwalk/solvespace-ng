include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

foreach(pkg_config_lib CAIRO)
    include_directories(${${pkg_config_lib}_INCLUDE_DIRS})
    link_directories(${${pkg_config_lib}_LIBRARY_DIRS})
endforeach()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    add_definitions(-DTEST_BUILD_ON_WINDOWS)
endif()

# test suite

set(testsuite_SOURCES
    harness.cpp
    analysis/contour_area/test.cpp
    core/expr/test.cpp
    core/commandnames/test.cpp
    core/commands/test.cpp
    core/viewpresets/test.cpp
    core/visibility/test.cpp
    core/sketchfocus/test.cpp
    core/smartdimension/test.cpp
    core/constraintprobe/test.cpp
    core/grab/test.cpp
    core/typedinput/test.cpp
    libs/keymap/test.cpp
    libs/palette/test.cpp
    libs/pie/test.cpp
    core/locale/test.cpp
    core/paste/test.cpp
    core/path/test.cpp
    core/prune/test.cpp
    core/solver/test.cpp
    kernel/facenamer/test.cpp
    kernel/solvespace/test.cpp
    kernel/revolved/test.cpp
    constraint/points_coincident/test.cpp
    constraint/pt_pt_distance/test.cpp
    constraint/pt_plane_distance/test.cpp
    constraint/pt_line_distance/test.cpp
    constraint/pt_face_distance/test.cpp
    constraint/proj_pt_distance/test.cpp
    constraint/pt_in_plane/test.cpp
    constraint/pt_on_line/test.cpp
    constraint/pt_on_face/test.cpp
    constraint/equal_length_lines/test.cpp
    constraint/length_ratio/test.cpp
    constraint/eq_len_pt_line_d/test.cpp
    constraint/eq_pt_ln_distances/test.cpp
    constraint/equal_angle/test.cpp
    constraint/equal_line_arc_len/test.cpp
    constraint/length_difference/test.cpp
    constraint/symmetric/test.cpp
    constraint/symmetric_horiz/test.cpp
    constraint/symmetric_vert/test.cpp
    constraint/symmetric_line/test.cpp
    constraint/at_midpoint/test.cpp
    constraint/horizontal/test.cpp
    constraint/vertical/test.cpp
    constraint/diameter/test.cpp
    constraint/pt_on_circle/test.cpp
    constraint/same_orientation/test.cpp
    constraint/angle/test.cpp
    constraint/parallel/test.cpp
    constraint/perpendicular/test.cpp
    constraint/arc_line_tangent/test.cpp
    constraint/cubic_line_tangent/test.cpp
    constraint/curve_curve_tangent/test.cpp
    constraint/equal_radius/test.cpp
    constraint/where_dragged/test.cpp
    constraint/comment/test.cpp
    constraint/large_dimensions/test.cpp
    request/arc_of_circle/test.cpp
    request/circle/test.cpp
    request/cubic/test.cpp
    request/cubic_periodic/test.cpp
    request/datum_point/test.cpp
    request/image/test.cpp
    request/line_segment/test.cpp
    request/ttf_text/test.cpp
    request/workplane/test.cpp
    group/boolean_coplanar_union/test.cpp
    group/boolean_knife_edge/test.cpp
    group/boolean_tangent_crossing/test.cpp
    group/boolean_tangent_edge/test.cpp
    group/boolean_tangent_fillet/test.cpp
    group/boolean_tangent_spline/test.cpp
    group/link/test.cpp
    group/translate_asy/test.cpp
    group/translate_nd/test.cpp
)

add_executable(solvespace-testsuite
    ${testsuite_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-testsuite
    solvespace-headless
    ${COVERAGE_LIBRARY})
target_link_libraries(solvespace-testsuite keymap palette pie)
if(ENABLE_NG_GUI)
    target_sources(solvespace-testsuite PRIVATE
        ng/settingsfile/test.cpp
        ng/menumodel/test.cpp
        ng/keys/test.cpp
        ng/backend/test.cpp
        ng/keyrouter/test.cpp
        ng/debugscript/test.cpp
        ng/uiactions/test.cpp
        ng/typedfields/test.cpp
        ng/coreactions/test.cpp
        ng/statusmodel/test.cpp
        ng/grabtool/test.cpp
        ng/drawtool/test.cpp
        ng/groupedit/test.cpp
        ng/outliner/test.cpp
        ng/groupprops/test.cpp
        ng/selection/test.cpp
        ng/problems/test.cpp
        ng/preferences/test.cpp
        ng/styles/test.cpp
        ng/viewedit/test.cpp
        ng/icons/test.cpp
        ng/shelf/test.cpp
        ng/viewcube/test.cpp
        ng/glyphs/test.cpp
        ng/menulayout/test.cpp)
    target_link_libraries(solvespace-testsuite solvespace-ng-support)
endif()

target_include_directories(solvespace-testsuite
    PRIVATE
    ${EIGEN3_INCLUDE_DIRS})
add_dependencies(solvespace-testsuite
    resources)

# UI tests: the real solvespace-ng app, driven with real SDL events (platform/sdl/uidriver.h)
# on SDL's offscreen video driver, so no display is needed.
set(ngui_SOURCES
    ngui/uitest.cpp
    ngui/smoke/test.cpp
    ngui/menus/test.cpp
    ngui/browser/test.cpp
    ngui/drawing/test.cpp
    ngui/grab/test.cpp
    ngui/constrain/test.cpp
    ngui/views/test.cpp
    ngui/groups/test.cpp
    ngui/editing/test.cpp
    ngui/app/test.cpp
    ngui/outliner/test.cpp
    ngui/properties/test.cpp
    ngui/problems/test.cpp
    ngui/preferences/test.cpp
    ngui/styles/test.cpp
    ngui/toolpanels/test.cpp
    ngui/parity/test.cpp
    ngui/sessions/test.cpp
    ngui/shelf/test.cpp
    ngui/capture/test.cpp)

if(ENABLE_GUI AND ENABLE_NG_GUI)
    add_executable(solvespace-ng-uitest
        harness.cpp
        ${ngui_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/render/rendercairo.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
    target_compile_definitions(solvespace-ng-uitest PRIVATE NG_UITEST)
    target_include_directories(solvespace-ng-uitest PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src ${EIGEN3_INCLUDE_DIRS})
    target_link_libraries(solvespace-ng-uitest solvespace-ng-app ${COVERAGE_LIBRARY})
    add_dependencies(solvespace-ng-uitest resources)
    set(ngui_COMMAND COMMAND $<TARGET_FILE:solvespace-ng-uitest>)
endif()

# Black-box tests of the real binary on a virtual X server (test/x11/run.py), when the tools
# are installed: make test_ng_x11, and part of test_solvespace.
if(ENABLE_GUI AND ENABLE_NG_GUI AND NOT WIN32 AND NOT APPLE)
    find_program(XVFB_EXECUTABLE Xvfb)
    find_program(XDOTOOL_EXECUTABLE xdotool)
    find_package(Python3 COMPONENTS Interpreter)
    if(XVFB_EXECUTABLE AND XDOTOOL_EXECUTABLE AND Python3_Interpreter_FOUND)
        set(ngx11_COMMAND COMMAND ${Python3_EXECUTABLE} -I
            ${CMAKE_CURRENT_SOURCE_DIR}/x11/run.py $<TARGET_FILE:solvespace-ng>)
        add_custom_target(test_ng_x11 ${ngx11_COMMAND}
            DEPENDS solvespace-ng
            COMMENT "Testing SolveSpace NG through X11"
            VERBATIM)
    else()
        message(STATUS "NG X11 tests disabled: needs Xvfb, xdotool and python3")
    endif()
endif()

add_custom_target(test_solvespace
    COMMAND $<TARGET_FILE:solvespace-testsuite>
    ${ngui_COMMAND}
    ${ngx11_COMMAND}
    COMMENT "Testing SolveSpace"
    VERBATIM)
if(ngx11_COMMAND)
    add_dependencies(test_solvespace solvespace-ng)
endif()

# coverage reports

if(ENABLE_COVERAGE)
    set(LCOV_FLAGS   -q --gcov-tool ${GCOV})
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc lcov_branch_coverage=1)
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_line=(ssassert|switch)")
    set(LCOV_FLAGS   ${LCOV_FLAGS} --rc "lcov_excl_br_line=BRANCH_ALWAYS_TAKEN")
    set(LCOV_COLLECT -c -b ${CMAKE_SOURCE_DIR}/src -d ${CMAKE_BINARY_DIR}/src --no-external)

    add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_base.info -i
        DEPENDS solvespace-testsuite
        COMMENT "Importing baseline coverage data"
        VERBATIM)

    add_custom_target(coverage_solvespace ALL
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS}
            -o ${CMAKE_BINARY_DIR}/coverage_full.info
            -a ${CMAKE_BINARY_DIR}/coverage_base.info
            -a ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS} --summary
            ${CMAKE_BINARY_DIR}/coverage_full.info
        COMMAND ${GENHTML} -q --branch-coverage --demangle-cpp --legend
            ${CMAKE_BINARY_DIR}/coverage_full.info
            -o ${CMAKE_BINARY_DIR}/coverage/
            -t "SolveSpace testbench"
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        DEPENDS test_solvespace
        COMMENT "Generating coverage report"
        VERBATIM)
endif()

# debug runner

set(debugtool_SOURCES
    debugtool.cpp
)

add_executable(solvespace-debugtool
    ${debugtool_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-debugtool
    solvespace-core
    solvespace-headless)

add_dependencies(solvespace-debugtool
    resources)
