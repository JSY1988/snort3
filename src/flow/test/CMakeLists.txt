add_cpputest( ha_test )

add_cpputest( flow_stash_test
    SOURCES ../flow_stash.cc
)

add_cpputest( flow_control_test
    SOURCES ../flow_control.cc
)

add_cpputest( flow_cache_test
    SOURCES
        ../flow_cache.cc
        ../flow_control.cc
        ../flow_key.cc
        ../../hash/hash_key_operations.cc
        ../../hash/hash_lru_cache.cc
        ../../hash/primetable.cc
        ../../hash/xhash.cc
        ../../hash/zhash.cc
)

add_cpputest( session_test )

add_cpputest( flow_test
    SOURCES
        ../flow.cc
        ../flow_data.cc
)
