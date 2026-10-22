set(COMPRESSION_LEVEL 1 CACHE STRING "Internal data compression level (0=no compression, 1=good, 6=smaller but slow)")

list(APPEND mistral_SOURCES
    cv-bmux-data.cc
    cv-bmux.cc
    cv-files.cc
    cv-inv.cc
    cv-opt.cc
    cv-pkg.cc
    cv-rmux.cc
    cv-rpats.cc
    cyclonev.cc

    bdz-ph.cc
    mistral-analogsim.cc
)

set(TOOLS ${CMAKE_CURRENT_SOURCE_DIR}/../tools)
set(DATA ${CMAKE_CURRENT_SOURCE_DIR}/../data)

find_package(PythonInterp 3.5 REQUIRED)

# Block muxes data

add_custom_command(
   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cv-bmux-data.cc
   COMMAND ${PYTHON_EXECUTABLE} ${TOOLS}/mux_to_source.py mux ${DATA} > ${CMAKE_CURRENT_BINARY_DIR}/cv-bmux-data.cc
   DEPENDS ${TOOLS}/mux_to_source.py ${DATA}/cbuf-mux.txt ${DATA}/cmuxc-mux.txt ${DATA}/cmuxh-mux.txt ${DATA}/cmuxv-mux.txt ${DATA}/dll-mux.txt ${DATA}/dqs16-mux.txt ${DATA}/dsp-mux.txt ${DATA}/fpll-mux.txt ${DATA}/gpio-mux.txt ${DATA}/hip-mux.txt ${DATA}/hmc-mux.txt ${DATA}/hps_clocks-mux.txt ${DATA}/hssi-mux.txt ${DATA}/lab-mux.txt ${DATA}/lvl-mux.txt ${DATA}/m10k-mux.txt ${DATA}/mlab-mux.txt ${DATA}/opt-mux.txt ${DATA}/pma3-c-mux.txt ${DATA}/pma3-mux.txt ${DATA}/serpar-mux.txt ${DATA}/term-mux.txt
)
list(APPEND mistral_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/cv-bmux-data.cc)

# Models generation

add_custom_command(
   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cv-models.cc
   COMMAND ${PYTHON_EXECUTABLE} ${TOOLS}/models_to_source.py ${DATA} > ${CMAKE_CURRENT_BINARY_DIR}/cv-models.cc
   DEPENDS ${TOOLS}/models_to_source.py ${DATA}/models.txt
)
list(APPEND mistral_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/cv-models.cc)


# C++ source generation/inclusion
foreach(die e50f gx25f gt75f gt150f gt300f sx50f sx120f)

    # per-die hand-made information

    list(APPEND mistral_SOURCES cvd-${die}.cc)


    # packages information

    add_custom_command(
       OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cvd-${die}-pkg.cc
       COMMAND ${PYTHON_EXECUTABLE} ${TOOLS}/pkg_to_source.py ${DATA} ${die} > ${CMAKE_CURRENT_BINARY_DIR}/cvd-${die}-pkg.cc
       DEPENDS ${TOOLS}/pkg_to_source.py ${DATA}/${die}-pkg.txt
    )
    list(APPEND mistral_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/cvd-${die}-pkg.cc)

endforeach()


# Binary routing/timing information generation

# Embed a generated binary as _binary_<sym>_start/_end through an .incbin in a
# generated C++ source, which works with any assembler the compiler drives
# (ELF and Mach-O alike); GNU "ld -r -b binary" does not exist on macOS.
macro(embed_bin bin sym source)
    set(EMBED_BIN ${CMAKE_CURRENT_BINARY_DIR}/${bin})
    set(EMBED_SYM ${sym})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/embed-bin.cc.in ${CMAKE_CURRENT_BINARY_DIR}/${source} @ONLY)
    # These translation units only define symbols in top-level assembly. Keep
    # them as native objects so the static archive exposes those symbols to
    # the linker when the rest of the library is built with IPO/LTO.
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${source} PROPERTIES
        OBJECT_DEPENDS ${EMBED_BIN}
        COMPILE_OPTIONS "-fno-lto"
    )
    list(APPEND mistral_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${source})
endmacro()

foreach(die e50f gx25f gt75f gt150f gt300f sx50f sx120f)
    add_custom_command(
       OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${die}-r.bin
       COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../generator/generator ${DATA} ${die} ${CMAKE_CURRENT_BINARY_DIR} ${COMPRESSION_LEVEL}
       DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../generator/generator ${DATA}/${die}-r.txt.xz ${DATA}/${die}-l.txt.xz ${DATA}/${die}-d.txt.xz ${DATA}/${die}-p2r.txt ${DATA}/${die}-p2p.txt ${DATA}/${die}-inv.txt
    )

    embed_bin(${die}-r.bin ${die}_r_bin cvd-${die}-r.cc)

endforeach()


# Binary global information generation

add_custom_command(
   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/global.bin
   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/../generator/generator ${DATA} - ${CMAKE_CURRENT_BINARY_DIR} ${COMPRESSION_LEVEL}
   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/../generator/generator ${DATA}/driver-models.txt.xz
)

embed_bin(global.bin global_bin cv-global.cc)


add_library(mistral ${mistral_SOURCES})
find_package(LibLZMA REQUIRED)
target_include_directories(mistral PRIVATE ${LIBLZMA_INCLUDE_DIRS})
target_link_libraries(mistral PUBLIC ${LIBLZMA_LIBRARIES})
add_dependencies(mistral tools)
add_dependencies(mistral generator)
target_include_directories(mistral PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/../tools ${CMAKE_CURRENT_SOURCE_DIR} /toto)


if(NOT MISTRAL_DONT_INSTALL)

install(TARGETS mistral DESTINATION lib)

install(FILES cyclonev.h DESTINATION include)
install(FILES mistral-analogsim.h DESTINATION include)
install(FILES cv-blocktypes.ipp DESTINATION include)
install(FILES cv-rnodetypes.ipp DESTINATION include)

endif()
