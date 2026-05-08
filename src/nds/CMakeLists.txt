add_executable(SRB2DSI)

target_link_libraries(SRB2DSI mm9 dswifi9)

set (SRB2_NDS_SOURCES
    i_main.c
    i_video.c
    i_system.c
    i_cdmus.c
    i_sound.c
    i_net.c
)

set(SRB2_HWRENDER_SOURCES
	${PROJECT_SOURCE_DIR}/src/hardware/hw_bsp.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_cache.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_draw.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_light.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_main.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_md2.c
	${PROJECT_SOURCE_DIR}/src/hardware/hw_trick.c
)

list(TRANSFORM SRB2_CORE_SOURCES PREPEND "${CMAKE_SOURCE_DIR}/src/")
list(TRANSFORM SRB2_CORE_RENDER_SOURCES PREPEND "${CMAKE_SOURCE_DIR}/src/")
list(TRANSFORM SRB2_CORE_GAME_SOURCES PREPEND "${CMAKE_SOURCE_DIR}/src/")
target_sources(SRB2DSI PRIVATE ${SRB2_CORE_SOURCES} ${SRB2_CORE_RENDER_SOURCES} ${SRB2_CORE_GAME_SOURCES} ${SRB2_NDS_SOURCES} ${SRB2_HWRENDER_SOURCES})

target_compile_definitions(SRB2DSI PRIVATE -D_NDS -DNOIPX -DZDEBUG -DNOMD5)

nds_create_rom(SRB2DSI
    NAME "Sonic Robo Blast 2"
    SUBTITLE "Retro"
    AUTHOR "Sonic Team Jr."
    ICON "icon.gif"
)
