set(srcs "Events.c"
         "Buttons.c"
         "FrameBuffer.c"
         "sr_driver.c"
         "Display_task.c"
         "bluetooth_audio/i2s_task.c"
         "FFT/FFT.c"
         "FFT/FFT_task.c"
         "Font.c"
         "drivers/MAX17048.c"
         "rgb_manager.c"
         "flash/flash_manager.c"
         "daftpunk_speaker.c")


set(audio_srcs "bluetooth_audio/bt_app_av.c"
               "bluetooth_audio/bt_app_core.c"
               "bluetooth_audio/bt_audio.c")
set(wifi_srcs "TCP_Shell/tcp_shell.c")

if (CONFIG_AUDIO_ENABLED)
        message(STATUS CONFIG_AUDIO_ENABLED: ${CONFIG_AUDIO_ENABLED})
        list(APPEND srcs ${audio_srcs})
endif()
if (CONFIG_WIFI_ENABLED)
        message(STATUS CONFIG_WIFI_ENABLED: ${CONFIG_WIFI_ENABLED})
        list(APPEND srcs ${wifi_srcs})
endif()


message(SRCS:"${srcs}")
message(AUDIO_SRCS:"${audio_srcs}")
message(WIFI_SRCS:"${wifi_srcs}")


idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                                 "bluetooth_audio"
                                 "FFT"
                                 "drivers"
                                 "TCP_Shell"
                                 "flash")
