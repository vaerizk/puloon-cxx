find_package(Boost 1.35.0 REQUIRED)

set(PULOON_PRIVATE_HEADERS
	lcdm_operations.h
)
set(PULOON_PUBLIC_HEADERS
	${PROJECT_SOURCE_DIR}/include/${PULOON_TARGET_NAME}/lcdm.h
)
set(PULOON_SOURCES
	lcdm.cpp
	lcdm_operations.cpp
)

add_library(${PULOON_TARGET_NAME} STATIC
	${PULOON_PRIVATE_HEADERS}
	${PULOON_PUBLIC_HEADERS}
	${PULOON_SOURCES}
)

target_include_directories(${PULOON_TARGET_NAME}
	PRIVATE
		${Boost_INCLUDE_DIRS}
		${PROJECT_SOURCE_DIR}/include/${PULOON_TARGET_NAME}
	INTERFACE
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(${PULOON_TARGET_NAME}
	${Boost_LIBRARIES}
)

install(TARGETS ${PULOON_TARGET_NAME}
	EXPORT ${PULOON_EXPORT_NAME}
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

# group source files for IDE source explorers (e.g. Visual Studio)
source_group("Private Header Files" FILES ${PULOON_PRIVATE_HEADERS})
source_group("Public Header Files" FILES ${PULOON_PUBLIC_HEADERS})
source_group("Source Files" FILES ${PULOON_SOURCES})
