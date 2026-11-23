# gyo_expect_static_assert_failure(<check> <variant> <marker> <output>): the configure stops unless <output>, the
# output of a try_compile that failed, has a static assertion's failure that names <marker> on the same line
# (clang, gcc: "static assertion failed"; MSVC: "static_assert failed"). The one judgement of the compile-failure
# samples: a test directory whose variants must fail with their own static_assert includes this file and calls it
# after try_compile reported the failure:
#   include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectStaticAssert.cmake")
#   gyo_expect_static_assert_failure(kernel.compile_fail_sample CAPTURE GYO_KERNEL_REJECT_CAPTURE "${output}")
# The marker alone is not enough: a compiler echoes the source line of each error, so an unrelated hard error (a
# misspelt type) on the line that carries the marker would put the marker in the output too, and the sample would
# pass without the expected failure (asset declaration section 15 item 12; v8-arch-gyo2-compile-fail-judgement).
# Assumes the compilers' English wording and a marker of [A-Z_] only (no regular-expression character): a localized
# compiler, or MSVC before 17.x ("error C2338: <message>"), stops a correct sample at configure, never lets a wrong
# one pass (merge review S3, N1).
# Each directory includes it itself, as with ExpectLinks.cmake.
include_guard(GLOBAL)

function(gyo_expect_static_assert_failure check variant marker output)
    if(NOT output MATCHES "static[_ ]assert(ion)? failed[^\n]*${marker}")
        message(FATAL_ERROR "${check}: variant ${variant} failed without ${marker} on a static assertion's "
            "failure line, so the failure is not the expected one:\n${output}")
    endif()
endfunction()
