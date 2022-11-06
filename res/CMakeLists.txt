execute_process(COMMAND bash -c "cd ${CMAKE_CURRENT_SOURCE_DIR}; ./minify.sh ")

set(js_in_files
${CMAKE_CURRENT_SOURCE_DIR}/res/src/main.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/app.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/misc/crypt.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/misc/utils.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/CmdButton.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/BoolInput.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/ROProp.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/Table.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/DeviceList.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/controls/IntInput.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/EventsPage.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/ConfigEditor.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/Program.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/FirmwarUpdater.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/TripViewer.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/StorageViewer.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/StatusPage.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/SystemPage.js
${CMAKE_CURRENT_SOURCE_DIR}/res/src/components/Event.js
)

find_program(YUI_EXECUTABLE yui-compressor)
if(YUI_EXECUTABLE AND (NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug"))
    message(STATUS "JS files will be minified before install.")
    foreach(jsfile ${js_in_files})
        set(jsmin "${CMAKE_CURRENT_BINARY_DIR}/${jsfile}.min")
        add_custom_command(OUTPUT ${jsmin}
            COMMAND ${YUI_EXECUTABLE}
            ARGS "${CMAKE_CURRENT_SOURCE_DIR}/${jsfile}" -o "${jsmin}"
        )
        install(FILES ${jsmin}
            DESTINATION "dist/"
            RENAME ${jsfile}
        )
        set(js_out_files ${js_out_files} ${jsmin})
    endforeach(jsfile)
else()
    message(STATUS "JS files will be installed unmodified.${YUI_EXECUTABLE}")
    foreach(jsfile ${js_in_files})
        install(FILES ${jsfile}
            message(STATUS "${jsfile} will be installed to ${CMAKE_CURRENT_SOURCE_DIR}/res/dist/.")
            DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/res/dist/"
        )
        set(js_out_files ${js_out_files} ${jsfile})
    endforeach(jsfile)
endif()

