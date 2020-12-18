add_executable(tests
    common/bit_field.cpp
    common/bit_utils.cpp
    common/fibers.cpp
    common/multi_level_queue.cpp
    common/param_package.cpp
    common/ring_buffer.cpp
    core/arm/arm_test_common.cpp
    core/arm/arm_test_common.h
    core/core_timing.cpp
    tests.cpp
)

create_target_directory_groups(tests)

target_link_libraries(tests PRIVATE common core)
target_link_libraries(tests PRIVATE ${PLATFORM_LIBRARIES} catch-single-include Threads::Threads)

if (APPLE)
  find_library(COREAUDIO CoreAudio)
  find_library(COREMEDIA CoreMedia)
  find_library(COREVIDEO CoreVideo)
  find_library(AUDIOTOOLBOX AudioToolbox)
  find_library(VIDEOTOOLBOX VideoToolbox)
  find_library(SWRESAMPLE swresample)
  target_link_libraries(tests PRIVATE ${COREAUDIO} ${COREMEDIA} ${COREVIDEO} ${AUDIOTOOLBOX} ${VIDEOTOOLBOX} ${SWRESAMPLE} "-liconv")
endif()

add_test(NAME tests COMMAND tests)
