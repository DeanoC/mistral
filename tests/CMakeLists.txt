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

enable_testing()
add_test(NAME dsp-ports COMMAND dsp-ports 5CSEBA6U23I7)
