# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(Tarefa4_Aplicacaoo_Temporizadores C CXX ASM)

pico_sdk_init()

add_executable(Tarefa4_Aplicacaoo_Temporizadores 
    Tarefa4_Aplicacaoo_Temporizadores.c
    inc/ssd1306_i2c.c  # Adicionando a implementação correta do display
)

pico_set_program_name(Tarefa4_Aplicacaoo_Temporizadores "Tarefa4_Aplicacaoo_Temporizadores")
pico_set_program_version(Tarefa4_Aplicacaoo_Temporizadores "0.1")

pico_enable_stdio_uart(Tarefa4_Aplicacaoo_Temporizadores 0)
pico_enable_stdio_usb(Tarefa4_Aplicacaoo_Temporizadores 1)

target_link_libraries(Tarefa4_Aplicacaoo_Temporizadores 
    pico_stdlib 
    hardware_timer 
    hardware_i2c
)

target_include_directories(Tarefa4_Aplicacaoo_Temporizadores PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/inc
)

pico_add_extra_outputs(Tarefa4_Aplicacaoo_Temporizadores)
