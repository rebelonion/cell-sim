file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})