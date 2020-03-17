#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <set>
#include <thread>
#include <memory>
#include <algorithm>
#include <functional>

#include "grpc_utils.h"
#include "settings.h"
#include "utils.h"

static constexpr int NShards = 10; 
typedef std::string NodeName;
typedef int64_t IndexID;
typedef uint64_t TermID;
static constexpr IndexID default_index_cursor = -1; // When no log, index is -1
static constexpr TermID default_term_cursor = 0; // When starting, term is 0

static constexpr uint64_t default_timeout_interval_lowerbound = 150 + 300; // 1000;
static constexpr uint64_t default_timeout_interval_upperbound = 300 + 300; // 1800;
static constexpr uint64_t default_heartbeat_interval = 30;
// the Raft paper: Each candidate
// restarts its randomized election timeout at the start of an
// election, and it waits for that timeout to elapse before
// starting the next election; 
// This implementation does not conform to the standard.
static constexpr uint64_t default_election_fail_timeout_interval = 550; // 3000; 
#define vote_for_none ""

#define GUARD std::lock_guard<std::mutex> guard((mut));

#if defined(USE_GRPC_ASYNC)
// Use Async gRPC model
typedef RaftMessagesClientAsync RaftMessagesClient;
#else
// Use Sync gRPC model
#if defined(USE_GRPC_STREAM)
typedef RaftMessagesStreamClientSync RaftMessagesClient;
#else
typedef RaftMessagesClientSync RaftMessagesClient;
#endif
#define GRPC_SYNC_CONCUR_LEVEL 8 
#endif

#define SEQ_START 1
#define USE_MORE_REMOVE


struct Config{
    int num;
	std::vector<int> shards;
    std::unordered_map<int,std::vector<std::string>> groups;
};

enum FLAG{
	SUCCESS=0,
	FAIL=1,
	AGAIN=2,
};

int key2shard(std::string key);

struct MigrationData{
	std::unordered_map<std::string,std::string> data;
	std::unordered_map<int,std::string> cache;
};


struct Persister{
    struct RaftNode * node = nullptr;
    void Dump(std::lock_guard<std::mutex> &, bool backup_conf = false);
    void Load(std::lock_guard<std::mutex> &);
};


struct NodePeer {
    NodeName name;
    bool voted_for_me = false;
    // Index of next log to copy
    IndexID next_index = default_index_cursor + 1;
    // Index of logs already copied
    IndexID match_index = default_index_cursor;
    // Use `RaftMessagesClient` to send RPC message to peer.
    std::shared_ptr<RaftMessagesClient> raft_message_client;
    // Debug usage
    bool receive_enabled = true;
    bool send_enabled = true;
    // According to the Raft Paper Chapter 6 Issue 1,
    // Newly added nodes is in staging mode, and thet have no voting rights,
    // Until they are sync-ed.
    bool voting_rights = true;
    // seq nr for next rpc call
    uint64_t seq = SEQ_START;
};

enum NUFT_RESULT{
    NUFT_OK = 0,
    NUFT_FAIL = 1,
    NUFT_RETRY = 2,
    NUFT_NOT_LEADER = 3,
    NUFT_RESULT_SIZE,
};
	
enum NUFT_CB_TYPE {
    NUFT_CB_ELECTION_START,
    NUFT_CB_ELECTION_END,
    NUFT_CB_STATE_CHANGE,
    NUFT_CB_ON_APPLY,

    NUFT_CB_ON_JOIN_APPLY,
    NUFT_CB_ON_LEAVE_APPLY,
    NUFT_CB_ON_MOVE_APPLY,
    NUFT_CB_ON_QUERY_APPLY,

	NUFT_CB_ON_GET_APPLY,
	NUFT_CB_ON_PUTAPPEND_APPLY,
	NUFT_CB_ON_NEWCONFIG_APPLY,
	NUFT_CB_ON_SHARDMIGRATIONREPLY_APPLY,
	NUFT_CB_ON_SHARDCLEANUP_APPLY,
	
    NUFT_CB_CONF_START,
    NUFT_CB_CONF_END,
    NUFT_CB_ON_REPLICATE,
    NUFT_CB_ON_NEW_ENTRY,
    NUFT_CB_SIZE,
};
enum NUFT_CMD_TYPE {
    NUFT_CMD_NORMAL = 0,
    NUFT_CMD_TRANS = 1,
    NUFT_CMD_SNAPSHOT = 2,
    NUFT_CMD_TRANS_NEW = 3,
    NUFT_CMD_SIZE,
};

enum NUFT_SERVER_TYPE{
    NUFT_KV=0,
    NUFT_SHARDMASTER = 1,
    NUFT_SHARDKV,
};
struct NuftCallbackArg{
    struct RaftNode * node;
    std::lock_guard<std::mutex> * lk = nullptr;
    int a1 = 0;
    int a2 = 0;
    void * p1 = nullptr;
};
typedef int NuftResult;
// typedef NuftResult NuftCallbackFunc(NUFT_CB_TYPE, NuftCallbackArg *);
// typedef std::function<NuftResult(NUFT_CB_TYPE, NuftCallbackArg *)> NuftCallbackFunc;
typedef std::function<NuftResult(NUFT_CB_TYPE, NuftCallbackArg *)> NuftCallbackFunc;

struct ApplyMessage{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    std::string data;
};

//ctx
struct JoinApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    //std::string data;

	long long clientId;
	long long requestSeq;
    std::unordered_map<int,std::vector<std::string>> servers;
};

struct LeaveApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
	
	long long clientId;
	long long requestSeq;
    std::vector<int> gids;
};

struct MoveApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
	
	long long clientId;
	long long requestSeq;
    int shard;
    int gid;
};

struct QueryApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    int num;
};

struct GetApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
	
	int configNum;
    std::string key;
};

struct PutAppendApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
	
	int requestId;
	int expireRequestId;
	int configNum;
    std::string key;
	std::string value;
	std::string op;
};

struct NewConfigApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    
	Config newConfig;  
};

struct ShardMigrationReplyApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    
	int shard;
	int configNum;
	MigrationData migrationData;
};

struct ShardCleanupApplyMessage//:public ApplyMessage
{
    IndexID index = default_index_cursor;
    TermID term = default_term_cursor;
    std::string name;
    bool from_snapshot = false;
    
	int shard;
	int configNum;
};