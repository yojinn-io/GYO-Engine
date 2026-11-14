# Base is the lowest engine layer (failure handling: Assert, Result, Error);
# Math sits directly above it. Every module may link either.
add_subdirectory(base)
add_subdirectory(math)
# The general mechanism (Registry, System contract, Scheduler, Plan) is a leaf
# directly above Base.
add_subdirectory(kernel)
# The OS layer sits directly above Base and below every module that needs the
# operating system (docs/architecture/os.md).
add_subdirectory(os)
# The engine time base and role threads sit directly above Base.
add_subdirectory(time)
add_subdirectory(threads)
# File systems, paths and streams above Base, Math and the OS layer
# (docs/architecture/io.md).
add_subdirectory(io)
# The audio mixer sits above the time base (docs/architecture/audio.md).
add_subdirectory(audio)
