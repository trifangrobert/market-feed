# All test executables live here.

# Helper: link to static core if it exists, else header-only interface
function(link_core tgt)
  if (TARGET marketfeed_core_static)
    target_link_libraries(${tgt} PRIVATE marketfeed_core_static)
  else()
    target_link_libraries(${tgt} PRIVATE marketfeed_core)
  endif()
endfunction()

# ---- Tests ----
add_executable(wire_roundtrip wire_roundtrip.cpp)
link_core(wire_roundtrip)
add_test(NAME wire_roundtrip COMMAND wire_roundtrip)

add_executable(ob_add_resting ob_add_resting.cpp)
link_core(ob_add_resting)
add_test(NAME ob_add_resting COMMAND ob_add_resting)

add_executable(ob_cancel_order ob_cancel_order.cpp)
link_core(ob_cancel_order)
add_test(NAME ob_cancel_order COMMAND ob_cancel_order)

add_executable(ob_best_quote ob_best_quote.cpp)
link_core(ob_best_quote)
add_test(NAME ob_best_quote COMMAND ob_best_quote)

add_executable(ob_match_taker ob_match_taker.cpp)
link_core(ob_match_taker)
add_test(NAME ob_match_taker COMMAND ob_match_taker)

add_executable(engine_new engine_new.cpp)
link_core(engine_new)
add_test(NAME engine_new COMMAND engine_new)

add_executable(engine_cancel engine_cancel.cpp)
link_core(engine_cancel)
add_test(NAME engine_cancel COMMAND engine_cancel)

add_executable(roundtrip roundtrip.cpp)
link_core(roundtrip)
add_test(NAME roundtrip COMMAND roundtrip)

# --- Integration tests ---
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(NAME server_client_roundtrip
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/integration/server_client_roundtrip.py
                 --server $<TARGET_FILE:server>
                 --client $<TARGET_FILE:client>)
set_tests_properties(server_client_roundtrip PROPERTIES TIMEOUT 20 RUN_SERIAL TRUE)


# --- HOW TO ADD A NEW TEST ---
# 1) Drop my_new_test.cpp into this folder.
# 2) Copy/paste these 3 lines and adjust the name:
# add_executable(my_new_test my_new_test.cpp)
# link_core(my_new_test)
# add_test(NAME my_new_test COMMAND my_new_test)