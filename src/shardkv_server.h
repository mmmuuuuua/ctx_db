#pragma once

#include "shardmaster_client.h"
#include "ShardkvInnerRpcClient.h"
//#include "shardmaster_config.h"

#include <grpcpp/grpcpp.h>
#include "grpc/shardkv_messages.grpc.pb.h"
#include "grpc/shardkv_messages.pb.h"

#include "grpc/shardkvinner_messages.grpc.pb.h"
#include "grpc/shardkvinner_messages.pb.h"



static constexpr uint64_t PollInterval = 400; 
static constexpr uint64_t PullInterval = 100; 
static constexpr uint64_t CleanInterval = 100;



struct ShardkvServer
{
	ShardkvServer(const std::string& nodeAddr_,const std::string& kvRpcAddr_,const std::string& innerRpcAddr_,int id,
				std::shared_ptr<ShardmasterClient> innerShardmasterClient_);
	~ShardkvServer();

	//基本
	mutable std::mutex shardkvServerMu;
	std::mutex monitor_mut;
	int me;
	struct RaftNode* node; 
	int gid;
	Config config;   //指针还是值？
	bool tobe_paused;
	//rpc
	//ShardmasterClient* mck;
	//考虑换成智能指针
	std::shared_ptr<ShardmasterClient> mck;
	//接受用户rpc请求的rpc服务端
	struct ShardkvRpcServer * shardkvRpcServer = nullptr;

	//与其他shardkvServer通信的rpc客户端和服务端
	struct ShardkvInnerRpcServer * shardkvInnerRpcServer = nullptr;
	//struct ShardkvInnerRpcClient * shardkvInnerRpcClient = nullptr;
	std::unordered_map<std::string,std::shared_ptr<ShardkvInnerRpcClient>> shardkvInnerRpcClients;
	
	//集群相关
	std::set<int> ownShards;
	std::unordered_map<int,std::unordered_map<int,MigrationData>> migratingShards;
	std::unordered_map<int,int> waitingShards;
	std::unordered_map<int,std::set<int>> cleaningShards;
	std::vector<Config> historyConfigs;//指针还是值？
	
	//存储
	std::unordered_map<std::string,std::string> data;
	std::unordered_map<int,std::string> cache;
	
	//rpc回调函数
	int onGet(shardkv_messages::GetResponse* response_ptr,const shardkv_messages::GetRequest& request);
	int onPutAppend(shardkv_messages::PutAppendResponse* response_ptr,const shardkv_messages::PutAppendRequest& request);
	int onShardCleanup(shardkvinner_messages::ShardCleanupResponse* response_ptr,const shardkvinner_messages::ShardCleanupRequest& request);
	int onShardMigration(shardkvinner_messages::ShardMigrationResponse* response_ptr,const shardkvinner_messages::ShardMigrationRequest& request);

	//apply回调函数
	NuftResult onGetApply(NUFT_CB_TYPE type,NuftCallbackArg * arg);
	NuftResult onPutAppendApply(NUFT_CB_TYPE type,NuftCallbackArg * arg);
	NuftResult onNewConfigApply(NUFT_CB_TYPE type,NuftCallbackArg * arg);  //同poll相关
	NuftResult onShardMigrationReplyApply(NUFT_CB_TYPE type,NuftCallbackArg * arg); //注意这个回调，同其他回调不同，该回调和onShardMigration没有关系
	NuftResult onShardCleanupApply(NUFT_CB_TYPE type,NuftCallbackArg * arg);
	//其他函数
	void ApplyNewConf(Config config);
	void poll();
	void pull();
	void doPull(int shard,Config oldConfig);
	void clean();
	void doClean(int shard,Config config);
	
	//用于多线程之间通信的全局变量和条件变量
	bool flag;
	//std::string getValue;
	std::queue<std::string> getValue;
	std::unordered_map<int,std::pair<std::mutex,std::condition_variable>> map_;
	
	//poll  pull clean 线程
	bool tobe_destructed;
	std::thread pollThread;
	std::thread pullThread;
	std::thread cleanThread;
	
	//ip:port
	std::string nodeAddr;
	std::string kvRpcAddr;
	std::string innerRpcAddr;
	
	//kvRpcAddr->innerRpcAddr
	std::unordered_map<std::string,std::string> kvToInnerAddr;

	//添加shardkvInnerRpcClients，mck等客户端通信服务
	void AddShardkvInnerRpcClients(std::string Addr)
	{
		shardkvInnerRpcClients[Addr]=std::make_shared<ShardkvInnerRpcClient>(Addr);
	}
};










