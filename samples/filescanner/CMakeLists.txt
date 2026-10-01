add_executable(ucb_filescanner)

target_sources(ucb_filescanner
    PRIVATE
        src/main.c
)

target_link_libraries(ucb_filescanner PRIVATE ucb)
