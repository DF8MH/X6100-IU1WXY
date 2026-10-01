set(MAIN_DB_FN ${CMAKE_BINARY_DIR}/params.db)

add_custom_target(params_sqlite ALL DEPENDS ${MAIN_DB_FN} SOURCES params.sql)

# The .csv files are imported by params.sql: rebuild when they change too
file(GLOB CSV_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.csv)

add_custom_command(
    OUTPUT ${MAIN_DB_FN}
    MAIN_DEPENDENCY params.sql
    DEPENDS ${CSV_FILES}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${MAIN_DB_FN}
    COMMAND sqlite3 ${MAIN_DB_FN} < params.sql
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

install(FILES ${MAIN_DB_FN} DESTINATION share/x6100 RENAME params.default.db)
