if(NOT EDITOR OR NOT WORK OR NOT FIXTURES)
    message(FATAL_ERROR "EDITOR, WORK and FIXTURES are required")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/assets" "${WORK}/working")
file(WRITE "${WORK}/assets/asset_catalog.json" "{\n  \"version\": 1,\n  \"assets\": []\n}\n")
set(source "${FIXTURES}/reference/stereo_22050_pcm24.wav")
function(check expected label)
    execute_process(COMMAND "${EDITOR}" ${ARGN}
        WORKING_DIRECTORY "${WORK}/working" RESULT_VARIABLE actual
        OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
    if(NOT actual STREQUAL "${expected}")
        message(FATAL_ERROR "${label}: expected ${expected}, got ${actual}\n${output}\n${error}")
    endif()
endfunction()
check(0 help --help)
check(2 no_command)
check(2 unknown_command play)
check(0 info info "${source}")
check(3 info_missing info "${WORK}/missing.wav")
check(2 convert_missing_option convert "${source}" --asset-root "${WORK}/assets" --id game.audio.a --path a.wav)
check(0 convert convert "${source}" --asset-root "${WORK}/assets"
    --asset-catalog "${WORK}/assets/asset_catalog.json" --id game.audio.a --path audio/a.wav)
if(NOT EXISTS "${WORK}/assets/audio/a.wav")
    message(FATAL_ERROR "convert: audio/a.wav was not written")
endif()
file(READ "${WORK}/assets/asset_catalog.json" catalog)
if(NOT catalog MATCHES "\"id\": \"game.audio.a\", \"type\": \"sound\", \"path\": \"audio/a.wav\"")
    message(FATAL_ERROR "convert: the catalog entry is missing\n${catalog}")
endif()
check(4 convert_taken_path convert "${source}" --asset-root "${WORK}/assets"
    --asset-catalog "${WORK}/assets/asset_catalog.json" --id game.audio.b --path audio/a.wav)
file(WRITE "${WORK}/bad.wav" "not a wav")
check(4 convert_invalid_wav convert "${WORK}/bad.wav" --asset-root "${WORK}/assets"
    --asset-catalog "${WORK}/assets/asset_catalog.json" --id game.audio.c --path audio/c.wav)

# export: every region of a project, written by hand (the format is the editor's own).
file(WRITE "${WORK}/working/reload.audio.json" "{
  \"format\": \"gyo.audio_editor.project\",
  \"version\": 2,
  \"source\": \"${source}\",
  \"asset_root\": \"${WORK}/assets\",
  \"asset_catalog\": \"${WORK}/assets/asset_catalog.json\",
  \"regions\": [
    {\"id\": \"game.audio.out\", \"path\": \"audio/out.wav\", \"trim_start\": 0, \"trim_end\": 120, \"gain_db\": 0, \"fade_in\": 0, \"fade_out\": 20},
    {\"id\": \"game.audio.in\", \"path\": \"audio/in.wav\", \"trim_start\": 100, \"trim_end\": null, \"gain_db\": -2, \"fade_in\": 20, \"fade_out\": 0}
  ]
}
")
check(2 export_no_project export)
check(2 export_option export --id)
check(0 export export "${WORK}/working/reload.audio.json")
foreach(clip out in)
    if(NOT EXISTS "${WORK}/assets/audio/${clip}.wav")
        message(FATAL_ERROR "export: audio/${clip}.wav was not written")
    endif()
endforeach()
file(READ "${WORK}/assets/asset_catalog.json" catalog)
if(NOT catalog MATCHES "game.audio.out" OR NOT catalog MATCHES "game.audio.in")
    message(FATAL_ERROR "export: an entry is missing\n${catalog}")
endif()
check(3 export_missing export "${WORK}/missing.audio.json")
file(WRITE "${WORK}/working/broken.audio.json" "{\"format\": \"gyo.audio_editor.project\", \"version\": 9}")
check(4 export_invalid export "${WORK}/working/broken.audio.json")
