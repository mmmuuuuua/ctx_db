

#include "node.h"
#include "shardkv_rpc_server.h"
#include "ShardkvInnerRpcServer.h"


ShardkvServer::ShardkvServer(const std::string& nodeAddr_,const std::string& kvRpcAddr_,const std::string& innerRpcAddr_,int id,
							std::shared_ptr<ShardmasterClient> innerShardmasterClient_)
							:nodeAddr(nodeAddr_),kvRpcAddr(kvRpcAddr_),innerRpcAddr(innerRpcAddr_),gid(id)
{
	printf("start constructing a new ShardkvServer,the gid is %d\n",gid);
	tobe_destructed = false;
	
	////新增
	tobe_paused = false;
	
	
	config.num = 0;
	config.shards = std::vector<int>(10,0);
	
	
	//mck = new ShardmasterClient();
	//mck = std::make_shared<ShardmasterClient>();
	mck = innerShardmasterClient_;
	node = new RaftNode(nodeAddr,this,NUFT_SHARDKV);
	
	shardkvRpcServer = new ShardkvRpcServer(this,kvRpcAddr);
	shardkvInnerRpcServer = new ShardkvInnerRpcServer(this,innerRpcAddr);

    using namespace std::chrono_literals;
	//poll
	pollThread = std::thread([&]() {
	while (1) {
		if(tobe_destructed){
			return;
		}
		if(!tobe_paused)
			this->poll();
		std::this_thread::sleep_for(std::chrono::duration<int, std::milli> {PollInterval});
	}
    });
	
	//pull
	pullThread = std::thread([&]() {
	while (1) {
		if(tobe_destructed){
			return;
		}
		if(!tobe_paused)
			this->pull();
		std::this_thread::sleep_for(std::chrono::duration<int, std::milli> {PullInterval});
	}
    });

	//clean
	cleanThread = std::thread([&]() {
	while (1) {
		if(!tobe_destructed){
			return;
		}
		if(tobe_paused)
			this->clean();
		std::this_thread::sleep_for(std::chrono::duration<int, std::milli> {CleanInterval});
	}
    });

	node->set_callback(NUFT_CB_ON_GET_APPLY, std::bind(&ShardkvServer::onGetApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_PUTAPPEND_APPLY, std::bind(&ShardkvServer::onPutAppendApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_NEWCONFIG_APPLY, std::bind(&ShardkvServer::onNewConfigApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_SHARDMIGRATIONREPLY_APPLY, std::bind(&ShardkvServer::onShardMigrationReplyApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_SHARDCLEANUP_APPLY, std::bind(&ShardkvServer::onShardCleanupApply,this,std::placeholders::_1,std::placeholders::_2));

	printf("construct end,the gid is %d\n",gid);
}

ShardkvServer::~ShardkvServer()
{
	{
		shardkvServerMu.lock();
		tobe_destructed = true;
		tobe_paused = true;
		
		shardkvServerMu.unlock();
	}
	{	
		printf("start wait pollThread join,the gid is %d\n",gid);
		pollThread.join();
		printf("start wait pullThread join,the gid is %d\n",gid);
		pullThread.join();
		printf("start wait cleanThread join,the gid is %d\n",gid);
		cleanThread.join();
	}
	{
		//delete mck;
		//mck = nullptr;
		printf("start delete raft node,the gid is %d\n",gid);
		delete node;
		node = nullptr;
		printf("start delete shardkvRpcServer,the gid is %d\n",gid);
		delete shardkvRpcServer;
		shardkvRpcServer = nullptr;
		printf("start delete shardkvInnerRpcServer,the gid is %d\n",gid);
		delete shardkvInnerRpcServer;
		shardkvInnerRpcServer = nullptr;
		printf("~ShardkvServer end\n");
	}
}

void ShardkvServer::ApplyNewConf(Config newConfig)
{
	if(newConfig.num<=config.num)
		return;
	Config oldConfig = config;
	std::set<int> oldShards = ownShards;
	historyConfigs.push_back(oldConfig);
	ownShards.clear();
	config = newConfig;
	for(int i=0;i<newConfig.shards.size();i++)
	{
		if(newConfig.shards[i]==gid)
		{
			if(oldShards.find(i)!=oldShards.end()||oldConfig.num==0)
			{
				ownShards.insert(i);
				oldShards.erase(i);
			}
			else
			{
				waitingShards[i] = oldConfig.num;
			}
		}
	}
	
	if(!oldShards.empty())
	{
		std::unordered_map<int,MigrationData> v;
		for(auto iter=oldShards.begin();iter!=oldShards.end();iter++)
		{
			MigrationData temp_data;
			for(auto iter2=data.begin();iter2!=data.end();iter2++)
			{
				if(key2shard(iter2->first)==(*iter))
				{
					temp_data.data[iter2->first] = iter2->second;
					data.erase(iter2->first);
				}	
			}
			for(auto iter2=cache.begin();iter2!=cache.end();iter2++)
			{
				if(key2shard(iter2->second)==(*iter))
				{
					temp_data.cache[iter2->first] = iter2->second;
					cache.erase(iter2->first);
				}	
			}
			v[*iter] = temp_data;
		}
		migratingShards[oldConfig.num] = v;
	}
}

void ShardkvServer::poll()
{
	shardkvServerMu.lock();
	printf("try query,try get new config,the gid is %d\n",gid);
	if(node->state != RaftNode::NodeState::Leader)
	{
		shardkvServerMu.unlock();
		return;
	}
	printf("start query,try get new config,the gid is %d\n",gid);
	int nextConfigNum = config.num + 1;
	shardkvServerMu.unlock();
	Config newConfig = mck->Query(nextConfigNum);
	if(newConfig.num==nextConfigNum){
		IndexID index = node->do_log(std::string("newconfig"),newConfig); 
		shardkvServerMu.lock();
		std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
		shardkvServerMu.unlock();
		//std::unique_lock<std::mutex> lk(raft_node->mut_);

		if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
		//if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
		{
			lk.unlock();
			//response.set_flag(FAIL);
			return;
		}
		else{
			lk.unlock();
			//if(flag==false)
			//	response.set_flag(FAIL);
			//else
			//{
			//	response.set_value(getValue.front());
			//	getValue.pop();
			//	response.set_flag(SUCCESS);
			//}
			return;
		}
	}
}

void ShardkvServer::pull()
{
	shardkvServerMu.lock();
	if(node->state != RaftNode::NodeState::Leader)
	{
		shardkvServerMu.unlock();
		return;
	}
	printf("start waitingShards,number of waitingShards shards is %d,the gid is %d\n",waitingShards.size(),gid);
	std::vector<std::thread> pullThreads(waitingShards.size());
	int cnt=0;
	for(auto iter=waitingShards.begin();iter!=waitingShards.end();iter++)
	{
		pullThreads[cnt++] = std::thread(std::bind(&ShardkvServer::doPull,this,std::placeholders::_1,std::placeholders::_2),iter->first,historyConfigs[iter->second]);
	}
	shardkvServerMu.unlock();
	for(int i=0;i<cnt;i++)
	{
		pullThreads[i].join();
	}
}

void ShardkvServer::doPull(int shard,Config oldConfig)
{
	int configNum = oldConfig.num;
	int gid = oldConfig.shards[shard];
	std::vector<std::string> servers = oldConfig.groups[gid];

	shardkvinner_messages::ShardMigrationRequest request;
	shardkvinner_messages::ShardMigrationResponse response;
	request.set_shard(shard);
	request.set_confignum(configNum);
	
	for(int i=0;i<servers.size();i++)
	{
		shardkvInnerRpcClients[kvToInnerAddr[servers[i]]]->ShardMigration(request,response);
		//还需要更严格的差错处理机制
		if(response.flag()==SUCCESS)  //SUCCESS待定
		{
			
			IndexID index = node->do_log(std::string("shardmigration"),response);  
			shardkvServerMu.lock();
			std::unique_lock<std::mutex> lk(map_[index].first); 
			shardkvServerMu.unlock();

			if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
			{
				lk.unlock();
				//response.set_flag(FAIL);
				return;
			}
			else{
				lk.unlock();
				//if(flag==false)
				//	response.set_flag(FAIL);
				//else
				//	response.set_flag(SUCCESS);
				return;
			}
			return;
		}
	}
}


void ShardkvServer::clean()
{
	shardkvServerMu.lock();
	if(cleaningShards.empty())
	{
		shardkvServerMu.unlock();
		return;
	}
	printf("start cleaning,number of cleaning shards is %d\n",cleaningShards.size());
	std::vector<std::thread> cleanThreads(cleaningShards.size());
	int cnt=0;
	for(auto iter=cleaningShards.begin();iter!=cleaningShards.end();iter++)
	{
		for(auto iter1=(iter->second).begin();iter1!=(iter->second).end();iter1++)
		{
			//以下对比可以发现std::thread和类非静态成员函数结合的正确使用方法
			//cleanThreads[cnt++] = std::thread(ShardkvServer::doClean,*iter1,historyConfigs[iter->first]);
			cleanThreads[cnt++] = std::thread(std::bind(&ShardkvServer::doClean,this,std::placeholders::_1,std::placeholders::_2),*iter1,historyConfigs[iter->first]);
		}
		
	}
	shardkvServerMu.unlock();
	for(int i=0;i<cnt;i++)
	{
		cleanThreads[i].join();
	}
}

void ShardkvServer::doClean(int shard,Config config)
{
	int configNum = config.num;
	int gid = config.shards[shard];
	std::vector<std::string> servers = config.groups[gid];

	shardkvinner_messages::ShardCleanupRequest request;
	shardkvinner_messages::ShardCleanupResponse response;
	request.set_shard(shard);
	request.set_confignum(configNum);
	
	
	for(int i=0;i<servers.size();i++)
	{
		shardkvInnerRpcClients[kvToInnerAddr[servers[i]]]->ShardCleanup(request,response);

		if(response.flag()==SUCCESS)  //SUCCESS待定
		{
			shardkvServerMu.lock();
			cleaningShards[configNum].erase(shard);
			if(cleaningShards[configNum].empty())
			{
				cleaningShards.erase(configNum);
			}
			shardkvServerMu.unlock();
			return;
		}
	}
}


int ShardkvServer::onGet(shardkv_messages::GetResponse* response_ptr,const shardkv_messages::GetRequest& request)
{
	shardkv_messages::GetResponse & response = *response_ptr; 
	if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardkvServerMu.lock(); ///考虑将此锁放到do_log之后

    IndexID index = node->do_log(std::string("get"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardkvServerMu.unlock();
    //std::unique_lock<std::mutex> lk(raft_node->mut_);

    if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
    //if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
    {
        lk.unlock();
        response.set_flag(FAIL);
        return 0;
    }
    else{
        lk.unlock();
		if(flag==false)
			response.set_flag(FAIL);
		else
		{
			response.set_value(getValue.front());
			getValue.pop();
			response.set_flag(SUCCESS);
		}
        return 0;
    }
}
int ShardkvServer::onPutAppend(shardkv_messages::PutAppendResponse* response_ptr,const shardkv_messages::PutAppendRequest& request)
{
	shardkv_messages::PutAppendResponse & response = *response_ptr; 
	if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardkvServerMu.lock(); ///考虑将此锁放到do_log之后
    

    IndexID index = node->do_log(std::string("putappend"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardkvServerMu.unlock();
    //std::unique_lock<std::mutex> lk(raft_node->mut_);

    if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
    //if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
    {
        lk.unlock();
        response.set_flag(FAIL);
        return 0;
    }
    else
	{
        lk.unlock();
		if(flag==false)
			response.set_flag(FAIL);
		else
			response.set_flag(SUCCESS);
        return 0;
    }
}

int ShardkvServer::onShardCleanup(shardkvinner_messages::ShardCleanupResponse* response_ptr,const shardkvinner_messages::ShardCleanupRequest& request)
{
	shardkvinner_messages::ShardCleanupResponse & response = *response_ptr; 
	if(node->state!=RaftNode::NodeState::Leader)
	{
		response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
	}
	shardkvServerMu.lock(); 
    

	if(migratingShards.find(request.confignum())!=migratingShards.end())
	{
		if(migratingShards[request.confignum()].find(request.shard())!=migratingShards[request.confignum()].end())
		{
			IndexID index = node->do_log(std::string("shardcleanup"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

			std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
																///应该选择更好的初始化机制？
			shardkvServerMu.unlock();
			//std::unique_lock<std::mutex> lk(raft_node->mut_);

			if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
			//if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
			{
				lk.unlock();
				response.set_flag(FAIL);
				return;
			}
			else
			{
				lk.unlock();
				if(flag==false)
					response.set_flag(FAIL);
				else
					response.set_flag(SUCCESS);
				return;
			}
		}
	}
	else
	{
		shardkvServerMu.unlock(); 
		response.set_flag(FAIL);
		return;
	}
}

int ShardkvServer::onShardMigration(shardkvinner_messages::ShardMigrationResponse* response_ptr,const shardkvinner_messages::ShardMigrationRequest& request)
{
	shardkvServerMu.lock();

	shardkvinner_messages::ShardMigrationResponse & response = *response_ptr; 
	int configNum = request.confignum();
	int shard = request.shard();
	
	response.set_flag(SUCCESS);
	response.set_shard(shard);
	response.set_confignum(configNum);
	
	if(request.confignum()>=config.num)
	{
		response.set_flag(FAIL);
		return;
	}
	if(migratingShards.find(configNum)!=migratingShards.end())
	{
		const std::unordered_map<int,MigrationData>& v= migratingShards[configNum];
		if(v.find(shard)!=v.end())
		{
			MigrationData& migrationData = v[shard];
			for(auto iter=migrationData.data.begin();iter!=migrationData.data.end();iter++)
			{
				shardkvinner_messages::Data* cur;
				cur = response.add_data();
				cur->set_key(iter->first);
				cur->set_value(iter->second);
			}
			for(auto iter=migrationData.cache.begin();iter!=migrationData.cache.end();iter++)
			{
				shardkvinner_messages::Cache* cur;
				cur = response.add_cache();
				cur->set_key(iter->first);
				cur->set_value(iter->second);
			}
		}
	}
	shardkvServerMu.unlock();
}


NuftResult ShardkvServer::onGetApply(NUFT_CB_TYPE type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	GetApplyMessage * applymsg = (GetApplyMessage *)(arg->p1);
	int shard = key2shard(applymsg->key);
	if(applymsg->configNum!=config.num)
	{
		flag = false;
	}
	else if(ownShards.find(shard)==ownShards.end())
	{
		flag = false;
	}
	else
	{
		getValue.push(data[applymsg->key]);
		flag = true;
	}	
	return NUFT_OK;
}

NuftResult ShardkvServer::onPutAppendApply(NUFT_CB_TYPE type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	PutAppendApplyMessage * applymsg = (PutAppendApplyMessage *)(arg->p1);
	
	int shard = key2shard(applymsg->key);
	if(applymsg->configNum!=config.num)
	{
		flag = false;
	}
	else if(ownShards.find(shard)==ownShards.end())
	{
		flag = false;
	}
	else if(cache.find(applymsg->requestId)==cache.end())
	{
		flag = true;
		if(applymsg->op=="put")
		{
			data[applymsg->key]=applymsg->value;
		}
		else
		{
			data[applymsg->key]+=applymsg->value;
		}
	}

	cache.erase(applymsg->expireRequestId);
	cache[applymsg->requestId] = applymsg->key;
	return NUFT_OK;
}



NuftResult ShardkvServer::onShardMigrationReplyApply(NUFT_CB_TYPE type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	ShardMigrationReplyApplyMessage * applymsg = (ShardMigrationReplyApplyMessage *)(arg->p1);
	if(applymsg->configNum == config.num-1)   //这句话的意义？
	{
		printf("shardmigration success,waitingShards %d has been shardmigrated,then erase\n",applymsg->shard);
		waitingShards.erase(applymsg->shard);
		if(ownShards.find(applymsg->shard)==ownShards.end())
		{
			cleaningShards[applymsg->configNum].insert(applymsg->shard);
			ownShards.insert(applymsg->shard);
			for(auto iter=(applymsg->migrationData).data.begin();iter!=(applymsg->migrationData).data.end();iter++)
			{
				data[iter->first] = iter->second;
			}
			for(auto iter=(applymsg->migrationData).cache.begin();iter!=(applymsg->migrationData).cache.end();iter++)
			{
				cache[iter->first] = iter->second;
			}
		}
	}
	flag = true; 
	return NUFT_OK;
}

NuftResult ShardkvServer::onShardCleanupApply(NUFT_CB_TYPE type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	ShardCleanupApplyMessage * applymsg = (ShardCleanupApplyMessage *)(arg->p1);
	
	if(migratingShards.find(applymsg->configNum)!=migratingShards.end())
	{
		migratingShards[applymsg->configNum].erase(applymsg->shard);
		if(migratingShards[applymsg->configNum].empty())
			migratingShards.erase(applymsg->configNum);
	}
	flag = true;
	return NUFT_OK;
}

NuftResult ShardkvServer::onNewConfigApply(NUFT_CB_TYPE type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	NewConfigApplyMessage * applymsg = (NewConfigApplyMessage *)(arg->p1);
	ApplyNewConf(applymsg->newConfig);
	printf("newConfig is applied,the shards to gids is as follows:\n");
	for(int i=0;i<(applymsg->newConfig).shards.size();i++)
	{
		printf("%d ",(applymsg->newConfig).shards[i]);
	}
	printf("\n");
	flag = true;
	return NUFT_OK;
}