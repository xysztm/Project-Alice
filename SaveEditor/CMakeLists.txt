if(WIN32)
add_executable(save_editor "${PROJECT_SOURCE_DIR}/SaveEditor/save_editor_main.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_state.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_data_loading.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_borders.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map.cpp"
	"${PROJECT_SOURCE_DIR}/src/graphics/xac.cpp"
	"${PROJECT_SOURCE_DIR}/src/alice.rc")
else()
add_executable(save_editor "${PROJECT_SOURCE_DIR}/SaveEditor/save_editor_main.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_state.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_data_loading.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map_borders.cpp"
	"${PROJECT_SOURCE_DIR}/src/map/map.cpp"
	"${PROJECT_SOURCE_DIR}/src/graphics/xac.cpp")
endif()

target_link_libraries(save_editor PRIVATE AliceCommon)

add_dependencies(save_editor GENERATE_PARSERS)
add_dependencies(save_editor GENERATE_CONTAINER ParserGenerator)

target_precompile_headers(save_editor REUSE_FROM Alice)
