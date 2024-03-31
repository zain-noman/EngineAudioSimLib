idf_component_register(
    SRCS "i2s_std_example_main.c" "engineLib.c"
    INCLUDE_DIRS "."
    EMBED_FILES "../../../alto_25Hz.wav" "../../../alto_40Hz.wav" "../../../alto_65Hz.wav"
    )
