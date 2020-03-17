#pragma once

#define USE_GRPC_ASYNC

// #define USE_GRPC_SYNC
// #define USE_GRPC_STREAM

#if defined(USE_GRPC_ASYNC)
#undef USE_GRPC_SYNC
#define USE_GRPC_ASYNC
#else
// Use Sync gRPC model
#undef USE_GRPC_ASYNC
#define USE_GRPC_SYNC
#endif

#if defined(_HIDE_RAFT_DEBUG)
#define debug_node(...)
#else
#define debug_node(...) NUKE_LOG(LOGLEVEL_DEBUG, RaftNodeLogger{this}, ##__VA_ARGS__)
#endif

#define LOGLEVEL_DEBUG 5

#if defined(_HIDE_DEBUG)
#define debug(...)
#else
#define debug printf
#endif