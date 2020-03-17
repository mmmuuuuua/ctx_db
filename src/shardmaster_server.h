#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <unordered_map>
#include "shardmaster_config.h"

#include <grpcpp/grpcpp.h>
#include "grpc/shardmaster_messages.grpc.pb.h"
#include "grpc/shardmaster_messages.pb.h"

struct ShardmasterServer
{
    ShardmasterServer(const std::string& nodeAddr_,const std::string& rpcAddr_);
    ~ShardmasterServer();
	
    int onJoin(shardmaster_messages::JoinResponse* response_ptr,const shardmaster_messages::JoinRequest& request) ;
	int onLeave(shardmaster_messages::LeaveResponse* response_ptr,const shardmaster_messages::LeaveRequest& request) ;
	int onMove(shardmaster_messages::MoveResponse* response_ptr,const shardmaster_messages::MoveRequest& request) ;
	int onQuery(shardmaster_messages::QueryResponse* response_ptr,const shardmaster_messages::QueryRequest& request) ;
	
	int onJoinApply(int type,NuftCallbackArg * arg);
	int onLeaveApply(int type,NuftCallbackArg * arg);
	int onMoveApply(int type,NuftCallbackArg * arg);
	int onQueryApply(int type,NuftCallbackArg * arg);
						
	std::mutex monitor_mut;
	
    Config get_config(int num);
	void appendNewConfig(Config newConfig);
	
    struct ShardmasterRpcServer * shardmasterRpcServer_ = nullptr;

    struct RaftNode* node;
    mutable std::mutex shardmasterServerMu; ///注意区分这个锁和RaftNode里面的锁
    
    std::vector<Config> configs;
	std::unordered_map<long long,long long> cache;
	
	//用于多线程之间通信的全局变量和条件变量
	std::map<int,std::pair<std::mutex,std::condition_variable>> map_;
	Config queryConfig;
	
	//ip:port
	std::string nodeAddr;
	std::string rpcAddr;

	//全局变量
	int flag = true;
};













