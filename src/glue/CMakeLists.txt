add_library(mg-glue STATIC )
target_sources(mg-glue PRIVATE auth.cpp auth_checker.cpp auth_handler.cpp communication.cpp SessionHL.cpp ServerT.cpp MonitoringServerT.cpp)
target_link_libraries(mg-glue mg-query mg-auth mg-audit)
target_precompile_headers(mg-glue INTERFACE auth_checker.hpp auth_handler.hpp)
