find_package(PythonInterp 3.5 REQUIRED)

set(MISTRAL_TEST_INCLUDES
  "${PROJECT_BINARY_DIR}"
  "${PROJECT_SOURCE_DIR}/libmistral"
  "${CMAKE_BINARY_DIR}/libmistral"
  "${CMAKE_BINARY_DIR}/tools"
)

add_executable(dsp-ports dsp-ports.cc)
target_link_libraries(dsp-ports PUBLIC mistral lzma)
target_include_directories(dsp-ports PUBLIC ${MISTRAL_TEST_INCLUDES})

add_executable(lab-clock-oracle lab-clock-oracle.cc)
target_link_libraries(lab-clock-oracle PUBLIC mistral lzma)
target_include_directories(lab-clock-oracle PUBLIC ${MISTRAL_TEST_INCLUDES})

add_executable(routing-mux-cram routing-mux-cram.cc)
target_link_libraries(routing-mux-cram PUBLIC mistral lzma)
target_include_directories(routing-mux-cram PUBLIC ${MISTRAL_TEST_INCLUDES})

add_executable(routing-inverter-cram routing-inverter-cram.cc)
target_link_libraries(routing-inverter-cram PUBLIC mistral lzma)
target_include_directories(routing-inverter-cram PUBLIC ${MISTRAL_TEST_INCLUDES})

add_executable(bmux-cram-bits bmux-cram-bits.cc)
target_link_libraries(bmux-cram-bits PUBLIC mistral lzma)
target_include_directories(bmux-cram-bits PUBLIC ${MISTRAL_TEST_INCLUDES})

add_test(NAME dsp-ports COMMAND dsp-ports 5CSEBA6U23I7)
add_test(NAME lab-clock-oracle
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run-lab-clock-oracle.py $<TARGET_FILE:lab-clock-oracle>
)
add_test(NAME routing-mux-cram COMMAND routing-mux-cram)
add_test(NAME routing-inverter-cram COMMAND routing-inverter-cram)
add_test(NAME bmux-cram-bits COMMAND bmux-cram-bits)
