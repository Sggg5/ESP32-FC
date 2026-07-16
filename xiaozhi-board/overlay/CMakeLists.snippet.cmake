# ATIAN_SCREEN_STREAM_BEGIN
if(CONFIG_BOARD_TYPE_ATIAN_S3)
    list(APPEND SOURCES
        "apps/screen_stream/screen_stream_app.cpp"
        "apps/screen_stream/screen_stream_receiver.cpp"
        "apps/screen_stream/screen_stream_renderer.cpp"
        "apps/screen_stream/screen_stream_ui.cpp"
    )
    list(APPEND INCLUDE_DIRS "apps/screen_stream")
endif()
# ATIAN_SCREEN_STREAM_END
