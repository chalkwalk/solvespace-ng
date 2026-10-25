include(GNUInstallDirs)

# configuration

set(HAVE_SPACEWARE ${SPACEWARE_FOUND})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# solvespace dependencies
add_library(slvs_deps INTERFACE)
target_include_directories(slvs_deps SYSTEM INTERFACE
    ${OPENGL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${PNG_PNG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${MIMALLOC_INCLUDE_DIR}
    ${EIGEN3_INCLUDE_DIRS})
target_link_libraries(slvs_deps INTERFACE
    dxfrw
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
    ${FREETYPE_LIBRARY}
    mimalloc-static)

target_include_directories(slvs_deps SYSTEM INTERFACE ${CAIRO_INCLUDE_DIRS})
target_link_libraries(slvs_deps INTERFACE ${CAIRO_LIBRARIES})

if(Backtrace_FOUND)
    target_include_directories(slvs_deps SYSTEM INTERFACE
        ${Backtrace_INCLUDE_DIRS})
    target_link_libraries(slvs_deps INTERFACE
        ${Backtrace_LIBRARY})
endif()

if(SPACEWARE_FOUND)
    target_include_directories(slvs_deps SYSTEM INTERFACE
        ${SPACEWARE_INCLUDE_DIR})
    target_link_libraries(slvs_deps INTERFACE
        ${SPACEWARE_LIBRARIES})
endif()

if(ENABLE_OPENMP)
    target_link_libraries(slvs_deps INTERFACE slvs_openmp)
endif()

target_compile_options(slvs_deps
    INTERFACE ${COVERAGE_FLAGS})

# platform utilities
if(APPLE)
    target_link_libraries(slvs_deps INTERFACE
        ${APPKIT_LIBRARY})
endif()

# Solver
add_library(slvs-solver INTERFACE)
target_sources(slvs-solver INTERFACE
    dsc.h
    expr.h
    handle.h
    param.h
    platform/platform.h
    solvespace.h

    constrainteq.cpp
    entity.cpp
    expr.cpp
    platform/platformbase.cpp
    system.cpp
    util.cpp
)
target_include_directories(slvs-solver SYSTEM INTERFACE
    ${MIMALLOC_INCLUDE_DIR}
    ${EIGEN3_INCLUDE_DIRS})
target_include_directories(slvs-solver INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR})

# Needed for CI
file(WRITE ${CMAKE_BINARY_DIR}/version.env "\
VERSION_MAJOR=${PROJECT_VERSION_MAJOR}\n\
VERSION_MINOR=${PROJECT_VERSION_MINOR}\n\
VERSION_PATCH=0\n\
VERSION_GIT_HASH=${solvespace_GIT_HASH}\n\
")

if(ENABLE_GUI OR ENABLE_CLI)
    set(every_platform_SOURCES
        platform/guisdl.cpp
        platform/guihtml.cpp)

    # solvespace library

    set(solvespace_core_gl_SOURCES
        solvespace.cpp)

    add_library(solvespace-core STATIC
        polygon.h
        sketch.h
        ui.h
        render/render.h
        srf/surface.h

        bsp.cpp
        clipboard.cpp
        commandnames.cpp
        confscreen.cpp
        constraint.cpp
        describescreen.cpp
        draw.cpp
        drawconstraint.cpp
        drawentity.cpp
        export.cpp
        export3mf.cpp
        joint.cpp
        fit.cpp
        split.cpp
        exportstep.cpp
        exportvector.cpp
        file.cpp
        generate.cpp
        regen.cpp
        graphicswin.cpp
        group.cpp
        groupmesh.cpp
        kernel/solvespacekernel.cpp
        kernel/worker.cpp
        importdxf.cpp
        importidf.cpp
        importmesh.cpp
        mesh.cpp
        modify.cpp
        mouse.cpp
        viewpresets.cpp
        visibility.cpp
        groupprops.cpp
        constraintedit.cpp
        preferences.cpp
        styleedit.cpp
        sketchfocus.cpp
        constraintprobe.cpp
        grab.cpp
        typedinput.cpp
        polyline.cpp
        polygon.cpp
        resource.cpp
        request.cpp
        style.cpp
        textscreens.cpp
        textwin.cpp
        toolbar.cpp
        ttf.cpp
        undoredo.cpp
        view.cpp
        platform/platform.cpp
        platform/gui.cpp
        render/render.cpp
        render/render2d.cpp
        srf/boolean.cpp
        srf/curve.cpp
        srf/merge.cpp
        srf/ratpoly.cpp
        srf/raycast.cpp
        srf/shell.cpp
        srf/surface.cpp
        srf/surfinter.cpp
        srf/triangulate.cpp)

    target_include_directories(solvespace-core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(solvespace-core PRIVATE slvs-solver)
    target_link_libraries(solvespace-core PUBLIC slvs_deps)
    if(ENABLE_OCCT)
        target_link_libraries(solvespace-core PUBLIC occt)
        target_sources(solvespace-core PRIVATE kernel/occtkernel.cpp kernel/occtquadmesh.cpp)
        target_compile_definitions(solvespace-core PUBLIC HAVE_OCCT)
        if(ENABLE_SANITIZERS AND NOT MSVC)
            # OCCT's pool allocators hand out 4-byte aligned nodes, which its own header
            # templates then trip UBSan's alignment check on.
            set_source_files_properties(kernel/occtkernel.cpp kernel/occtquadmesh.cpp
                exportstep.cpp PROPERTIES COMPILE_OPTIONS -fno-sanitize=alignment)
        endif()
    endif()

    # solvespace translations

    if(HAVE_GETTEXT)
        get_target_property(solvespace_core_SOURCES solvespace-core SOURCES)
        get_target_property(slvs_solver_SOURCES slvs-solver INTERFACE_SOURCES)
        set(inputs
            ${slvs_solver_SOURCES}
            ${solvespace_core_SOURCES}
            ${every_platform_SOURCES}
            ${solvespace_core_gl_SOURCES})

        set(templ_po   ${CMAKE_CURRENT_BINARY_DIR}/../res/messages.po)

        set(output_pot ${CMAKE_CURRENT_SOURCE_DIR}/../res/messages.pot)
        set(output_po  ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/en_US.po)
        file(GLOB locale_pos ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/*.po)

        string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
            gen_output_pot ${output_pot}.gen)
        string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
            gen_output_po ${output_po}.gen)
        foreach(locale_po ${locale_pos})
            string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
                gen_locale_po ${locale_po}.gen)
            list(APPEND gen_locale_pos ${gen_locale_po})
        endforeach()

        add_custom_command(
            OUTPUT  ${gen_output_pot}
            COMMAND ${XGETTEXT}
                    --language=C++
                    --keyword --keyword=_ --keyword=N_ --keyword=C_:2,1c --keyword=CN_:2,1c
                    --force-po --width=100 --sort-by-file
                    --package-name=SolveSpace
                    --package-version=${PROJECT_VERSION}
                    "--copyright-holder=the PACKAGE authors"
                    --msgid-bugs-address=phkahler@gmail.com
                    --from-code=utf-8 --output=${gen_output_pot} ${inputs}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_output_pot} ${output_pot}
            DEPENDS ${inputs}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Extracting translations"
            VERBATIM)

        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../res/locales)

        # en_US is a bit special; we pre-fill the msgstrs from msgids, instead of (as would normally
        # happen) leaving them empty.
        add_custom_command(
            OUTPUT  ${gen_output_po}
            COMMAND ${MSGINIT}
                    --locale=en_US --no-translator
                    --output=${templ_po} --input=${gen_output_pot}
            COMMAND ${MSGMERGE}
                    --force-po --no-fuzzy-matching
                    --output=${gen_output_po} ${output_po} ${templ_po}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_output_po} ${output_po}
            DEPENDS ${gen_output_pot}
            COMMENT "Updating en_US translations"
            VERBATIM)

        foreach(locale_po ${locale_pos})
            string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
                gen_locale_po ${locale_po}.gen)

            get_filename_component(locale_name ${locale_po} NAME_WE)
            if(locale_name STREQUAL "en_US")
                continue()
            endif()

            add_custom_command(
                OUTPUT  ${gen_locale_po}
                COMMAND ${MSGMERGE}
                        --no-fuzzy-matching
                        --output=${gen_locale_po} ${locale_po} ${gen_output_pot}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_locale_po} ${locale_po}
                DEPENDS ${gen_output_pot}
                COMMENT "Updating ${locale_name} translations"
                VERBATIM)
        endforeach()

        add_custom_target(translate_solvespace
            DEPENDS ${gen_output_pot} ${gen_output_po} ${gen_locale_pos})
    endif()
endif()

# Dear ImGui with SDL3 + OpenGL3 backends, for solvespace-ng

if(ENABLE_GUI AND ENABLE_NG_GUI)
    set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extlib/imgui)
    add_library(imgui-sdl3 STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp)
    if(APPLE)
        # Legacy GL 2.1 context on macOS: no vertex array objects for the GL3 backend.
        target_sources(imgui-sdl3 PRIVATE ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp)
    else()
        target_sources(imgui-sdl3 PRIVATE ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
    endif()
    target_include_directories(imgui-sdl3 PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends)
    # The test-engine hooks (platform/sdl/itemregistry.cpp) let UI tests find items by label;
    # they cost nothing unless the test driver turns them on.
    target_compile_definitions(imgui-sdl3 PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS
                                                 IMGUI_ENABLE_TEST_ENGINE)
    target_link_libraries(imgui-sdl3 PUBLIC ${NG_SDL_TARGET})
    if(EMSCRIPTEN)
        target_compile_definitions(imgui-sdl3 PUBLIC IMGUI_IMPL_OPENGL_ES3)
    elseif(WIN32)
        # ANGLE provides GLES 3 on Windows; its gl3.h declares functions only with
        # GL_GLEXT_PROTOTYPES.
        target_compile_definitions(imgui-sdl3 PUBLIC IMGUI_IMPL_OPENGL_ES3 GL_GLEXT_PROTOTYPES)
        target_include_directories(imgui-sdl3 PUBLIC ${CMAKE_SOURCE_DIR}/extlib/angle/include)
        target_link_libraries(imgui-sdl3 PUBLIC GLESv2)
    endif()

    add_library(solvespace-ng-support STATIC
        platform/sdl/settingsfile.cpp
        platform/sdl/menumodel.cpp
        platform/sdl/keychord.cpp
        platform/sdl/timerregistry.cpp
        platform/sdl/dialoghelpers.cpp
        platform/sdl/debugscript.cpp
        platform/sdl/touchgestures.cpp
        ui/keypresets.cpp
        ui/keyrouter.cpp
        ui/uiactions.cpp
        ui/typedfields.cpp
        ui/coreactions.cpp
        ui/statusmodel.cpp
        ui/iconraster.cpp
        ui/shelfmodel.cpp
        ui/viewcubemodel.cpp
        ui/menulayout.cpp
        ui/grabtool.cpp
        ui/drawtool.cpp
        ui/outlinermodel.cpp
        ui/selectionmodel.cpp
        ui/problemsmodel.cpp)
    target_include_directories(solvespace-ng-support PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(solvespace-ng-support PUBLIC solvespace-core keymap palette SDL3::Headers)
    target_link_libraries(solvespace-ng-support PRIVATE lunasvg::lunasvg)

    # Everything but main(): also linked into the UI tests (test/ngui), which run the real app.
    add_library(solvespace-ng-app OBJECT
        solvespace.cpp
        platform/guisdl.cpp
        platform/sdl/itemregistry.cpp
        ui/palettewidget.cpp
        ui/panels.cpp
        ui/outliner.cpp
        ui/properties.cpp
        ui/problems.cpp
        ui/prefspanel.cpp
        ui/stylespanel.cpp
        ui/toolpanels.cpp
        ui/shelf.cpp
        ui/viewcube.cpp
        ui/shortcuts.cpp
        ui/icons.cpp
        ui/fields.cpp
        ui/ngui.cpp
        ui/piemenu.cpp
        ui/theme.cpp
        render/gl3shader.cpp
        render/rendergl3.cpp)
    add_dependencies(solvespace-ng-app resources)
    target_link_libraries(solvespace-ng-app PUBLIC
        solvespace-core solvespace-ng-support pie imgui-sdl3 ${NG_SDL_TARGET} ${OPENGL_LIBRARIES})

    add_executable(solvespace-ng WIN32 MACOSX_BUNDLE
        platform/entrygui.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
    add_dependencies(solvespace-ng resources)
    target_link_libraries(solvespace-ng PRIVATE solvespace-ng-app)

    if(EMSCRIPTEN)
        # The web build of solvespace-ng: WebGL 2, and ASYNCIFY for the app's own event loops.
        # IDBFS keeps /data (files and settings) in the browser between visits.
        set(ng_web_flags
            "-sALLOW_MEMORY_GROWTH=1" "-sASYNCIFY=1" "-sASYNCIFY_STACK_SIZE=1048576"
            "-sSTACK_SIZE=33554432" "-sINITIAL_MEMORY=134217728"
            "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2" "-lidbfs.js"
            --shell-file "\"${CMAKE_CURRENT_SOURCE_DIR}/platform/html/ngshell.html\"")
        get_target_property(ng_resources resources SOURCES)
        foreach(resource ${ng_resources})
            file(RELATIVE_PATH target_path "${CMAKE_SOURCE_DIR}" "${resource}")
            list(APPEND ng_web_flags "--preload-file \"${resource}\"@\"${target_path}\"")
        endforeach()
        set(ng_resp_file "${CMAKE_CURRENT_BINARY_DIR}/emscripten_ng_flags.rsp")
        file(GENERATE OUTPUT "${ng_resp_file}" CONTENT "$<JOIN:${ng_web_flags},\n>")
        set_target_properties(solvespace-ng PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "@${ng_resp_file}"
            LINK_DEPENDS
                "${CMAKE_CURRENT_SOURCE_DIR}/platform/html/ngshell.html;${ng_resp_file};${ng_resources}")
    endif()
endif()

# solvespace graphical executable: the web build (guihtml). The desktop is solvespace-ng.

if(ENABLE_GUI AND EMSCRIPTEN)
    add_executable(solvespace
        ${solvespace_core_gl_SOURCES}
        platform/entrygui.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

    add_dependencies(solvespace
        resources)

    target_link_libraries(solvespace
        PRIVATE
        solvespace-core
        ${OPENGL_LIBRARIES})

    # OpenGL version
    if(OPENGL STREQUAL 3)
        target_sources(solvespace PRIVATE
            render/gl3shader.cpp
            render/rendergl3.cpp)
    elseif(OPENGL STREQUAL 1)
        target_sources(solvespace PRIVATE
            render/rendergl1.cpp)
    else()
        message(FATAL_ERROR "Unsupported OpenGL version ${OPENGL}")
    endif()

    # Due to the response file hack below, we need a version of Emscripten that
    # passes arguments via a response file to the `file_packager` utility, but
    # this was not the case prior to version 4.0.8 of Emscripten.
    # See https://github.com/emscripten-core/emscripten/pull/24107
    if(NOT EMSCRIPTEN_VERSION OR EMSCRIPTEN_VERSION VERSION_LESS "4.0.8")
        message(FATAL_ERROR "The Emscripten build requires at least version 4.0.8")
    endif()

    target_compile_features(solvespace PRIVATE cxx_std_17)

    set(SHELL "${CMAKE_CURRENT_SOURCE_DIR}/platform/html/emshell.html")
    # Add internal quotes to paths used in the response file to ensure that paths
    # that contain spaces are handled correctly
    set(RESP_LINK_FLAGS
        --bind --shell-file "\"${SHELL}\"" --no-heap-copy
        "-s ALLOW_MEMORY_GROWTH=1" "-s WASM=1"
        "-s ASYNCIFY=1" "-s DYNCALLS=1" "-s ASSERTIONS=1"
        "-s TOTAL_STACK=33554432" "-s TOTAL_MEMORY=134217728")

    get_target_property(resource_list resources SOURCES)
    foreach(resource ${resource_list})
        # Strip everything before `res/...`, and map the source path to the stripped
        # path in the virtual Emscripten fileystem
        file(RELATIVE_PATH target_path "${CMAKE_SOURCE_DIR}" "${resource}")
        list(APPEND RESP_LINK_FLAGS "--preload-file \"${resource}\"@\"${target_path}\"")
    endforeach()

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        list(APPEND RESP_LINK_FLAGS
            --emrun --emit-symbol-map
            "-s DEMANGLE_SUPPORT=1"
            "-s SAFE_HEAP=1")
    endif()

    target_sources(solvespace PRIVATE
        platform/guihtml.cpp)

    # Pass arguments through a response file in order to avoid exceeding the maximum
    # command line length (the Emscripten tools on Windows are all wrapped by batch
    # scripts, and as such are subject to the CMD length limit of 8KiB).
    set(resp_file_path "${CMAKE_CURRENT_BINARY_DIR}/emscripten_flags.rsp")
    file(GENERATE OUTPUT "${resp_file_path}" CONTENT "$<JOIN:${RESP_LINK_FLAGS},\n>")

    # NOTE: it would be ideal if we could portably specify that linking depends on the
    # response file (and the files that are specified in it), but unfortunately there
    # is no way to reliably trigger a relink like that without using LINK_DEPENDS,
    # which only works with the Ninja and Makefile generators.
    # This means that using e.g. the Visual Studio of Xcode generators will not work
    # correctly when any of the link time dependencies changes, so the output artefact
    # needs to be explicitly deleted in order to trigger a relink.
    set_target_properties(solvespace PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "@${resp_file_path}"
        LINK_DEPENDS "${SHELL};${resp_file_path};${resource_list}")

    add_custom_command(
        TARGET solvespace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/solvespaceui.css
                ${EXECUTABLE_OUTPUT_PATH}/solvespaceui.css
        COMMENT "Copying UI stylesheet"
        VERBATIM)
    add_custom_command(
        TARGET solvespace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/solvespaceui.js
                ${EXECUTABLE_OUTPUT_PATH}/solvespaceui.js
        COMMENT "Copying UI script solvespaceui.js"
        VERBATIM)
    add_custom_command(
        TARGET solvespace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/filemanagerui.js
                ${EXECUTABLE_OUTPUT_PATH}/filemanagerui.js
        COMMENT "Copying UI script filemanagerui.sj"
        VERBATIM)
endif()

if(ENABLE_CLI)
# solvespace headless library

add_library(solvespace-headless STATIC EXCLUDE_FROM_ALL
    ${solvespace_core_gl_SOURCES}
    platform/guinone.cpp
    render/rendercairo.cpp)

target_compile_definitions(solvespace-headless
    PRIVATE HEADLESS)

target_include_directories(solvespace-headless
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${EIGEN3_INCLUDE_DIRS})

target_link_libraries(solvespace-headless
    PRIVATE
    solvespace-core)

# solvespace command-line executable

    add_executable(solvespace-cli
        platform/entrycli.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

    target_link_libraries(solvespace-cli
        solvespace-core
        solvespace-headless)

    add_dependencies(solvespace-cli
        resources)

    if(MSVC)
        set_target_properties(solvespace-cli PROPERTIES
            LINK_FLAGS "/INCREMENTAL:NO /OPT:REF")
    endif()
endif()

# solvespace unix package

if(NOT (WIN32 OR APPLE OR EMSCRIPTEN))
    if(TARGET solvespace-ng)
        install(TARGETS solvespace-ng
            COMPONENT   solvespace
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    if(ENABLE_CLI)
        install(TARGETS solvespace-cli
            COMPONENT   solvespace
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

# An unpacked Homebrew bottle of libomp (macOS CI) names itself
# @@HOMEBREW_PREFIX@@/opt/libomp/lib/libomp.dylib, which dyld can't load: OpenMP calls then
# jump to null. Name it by where it is (re-signed, as the edit breaks its signature).
if(APPLE AND ENABLE_OPENMP AND NOT TARGET solvespace)
    list(GET OpenMP_CXX_LIBRARIES 0 libomp_path)
    execute_process(COMMAND otool -D ${libomp_path} OUTPUT_VARIABLE libomp_id)
    if(libomp_id MATCHES "@@HOMEBREW_PREFIX@@")
        execute_process(COMMAND install_name_tool -id ${libomp_path} ${libomp_path})
        execute_process(COMMAND codesign --force --sign - ${libomp_path})
    endif()
endif()

# solvespace macOS package (the retired Cocoa app's bundle; solvespace-ng's is still to do)

if(APPLE AND TARGET solvespace)
    set(LIBOMP_LIB_PATH ${OpenMP_CXX_INCLUDE_DIRS}/../lib/libomp.dylib)
    set(LIBOMP_LINK_PATH "@executable_path/../Resources/libomp.dylib")
    set(LIBOMP_LINK_PATH_UTILS "@executable_path/SolveSpace.app/Contents/Resources/libomp.dylib")
    if(ENABLE_GUI)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND cp -r ${CMAKE_BINARY_DIR}/Resources $<TARGET_BUNDLE_CONTENT_DIR:solvespace>
        )
        if(ENABLE_OPENMP)
            execute_process(COMMAND install_name_tool -id ${LIBOMP_LINK_PATH} ${LIBOMP_LIB_PATH})
            message("FROM " ${${LIBOMP_LIB_PATH}} "TO" $<TARGET_BUNDLE_CONTENT_DIR:solvespace>/Resources/libomp.dylib)
            add_custom_command(TARGET solvespace POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${LIBOMP_LIB_PATH} $<TARGET_BUNDLE_CONTENT_DIR:solvespace>/Resources/libomp.dylib
                COMMAND install_name_tool -change ${LIBOMP_LINK_PATH} ${LIBOMP_LINK_PATH_UTILS} $<TARGET_FILE:solvespace-debugtool>
            )
        endif()
    endif()
    if(ENABLE_TESTS AND ENABLE_OPENMP)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND install_name_tool -change ${LIBOMP_LINK_PATH} ${LIBOMP_LINK_PATH_UTILS} $<TARGET_FILE:solvespace-testsuite>)
    endif()
    if(ENABLE_CLI)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:solvespace-cli> $<TARGET_FILE_DIR:solvespace>
            COMMENT "Bundling executable solvespace-cli"
            VERBATIM)
    endif()
endif()

add_subdirectory(slvs)

# With ENABLE_NG_GUI, ANGLE is built as DLLs (see the top-level CMakeLists), which affects both
# Windows GUI executables.

if(WIN32 AND ENABLE_GUI AND ENABLE_NG_GUI)
    foreach(target solvespace-ng solvespace)
        if(TARGET ${target})
            target_compile_definitions(${target} PRIVATE SOLVESPACE_ANGLE_SHARED)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        $<TARGET_FILE:EGL> $<TARGET_FILE:GLESv2> $<TARGET_FILE_DIR:${target}>)
        endif()
    endforeach()
endif()
