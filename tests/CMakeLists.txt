include(GoogleTest)

add_executable(htb_tests
    ConfigTest.cpp
    MetricTest.cpp
    ProcessProviderTest.cpp
    UtfTest.cpp
    VendorTest.cpp
)
target_link_libraries(htb_tests PRIVATE htb_core htb_hardware gtest_main)

gtest_discover_tests(htb_tests)
