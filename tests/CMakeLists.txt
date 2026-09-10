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

enable_testing()
add_test(NAME dsp-ports COMMAND dsp-ports 5CSEBA6U23I7)
add_test(NAME lab-clock-oracle
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run-lab-clock-oracle.py $<TARGET_FILE:lab-clock-oracle>
)
