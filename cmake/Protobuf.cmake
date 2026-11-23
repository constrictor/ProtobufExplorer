# Google's protobuf, for the .proto parser only.
#
# Pinned and built from source because distributions lag: Ubuntu 26.04 ships 3.21, which
# rejects every `edition = "..."` file.

include(FetchContent)

# Plain variables, not cache entries: both projects honour them (CMP0077), and they cannot
# leak into a reconfigure as user settings.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(ABSL_PROPAGATE_CXX_STD ON)
set(protobuf_INSTALL OFF)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_PROTOC_BINARIES OFF)
set(protobuf_BUILD_LIBUPB OFF)
set(protobuf_WITH_ZLIB OFF)

FetchContent_Declare(absl
    URL https://github.com/abseil/abseil-cpp/releases/download/20250512.1/abseil-cpp-20250512.1.tar.gz
    URL_HASH SHA256=9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db
    EXCLUDE_FROM_ALL SYSTEM)

FetchContent_Declare(protobuf
    URL https://github.com/protocolbuffers/protobuf/releases/download/v36.2/protobuf-36.2.tar.gz
    URL_HASH SHA256=3d9642a662d10e68ebae5e53f14dcce5105684212d5078f8e0d47d1ab3ae6b64
    EXCLUDE_FROM_ALL SYSTEM)

# Abseil first: protobuf looks for an installed copy unless absl::strings already exists.
FetchContent_MakeAvailable(absl protobuf)
