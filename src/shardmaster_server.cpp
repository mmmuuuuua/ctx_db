#pragma once

#include <string>
#include <mutex>

#include "node.h"
#include "shardmaster_rpc_server.h"
 
ShardmasterServer::ShardmasterServer(const std::string& nodeAddr_,const std::string& rpcAddr_):nodeAddr(nodeAddr_),rpcAddr(rpcAddr_)
{
	Config initConfig;
	initConfig.num = 0;
	initConfig.shards = std::vector<int>(10,0);
	configs.push_back(initConfig);
	
	node = new RaftNode(nodeAddr,this,NUFT_SHARDMASTER);
	shardmasterRpcServer_ = new ShardmasterRpcServer(this,rpcAddr);

	node->set_callback(NUFT_CB_ON_JOIN_APPLY, std::bind(&ShardmasterServer::onJoinApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_LEAVE_APPLY, std::bind(&ShardmasterServer::onLeaveApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_MOVE_APPLY, std::bind(&ShardmasterServer::onMoveApply,this,std::placeholders::_1,std::placeholders::_2));
	node->set_callback(NUFT_CB_ON_QUERY_APPLY, std::bind(&ShardmasterServer::onQueryApply,this,std::placeholders::_1,std::placeholders::_2));
}

ShardmasterServer::~ShardmasterServer()
{
    delete node;
    node = nullptr;
    delete shardmasterRpcServer_;
    shardmasterRpcServer_ = nullptr;
}

int ShardmasterServer::onJoin(shardmaster_messages::JoinResponse* response_ptr,const shardmaster_messages::JoinRequest& request)
{
	shardmaster_messages::JoinResponse & response = *response_ptr; 
    if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardmasterServerMu.lock(); ///考虑将此锁放到do_log之后
    

    IndexID index = node->do_log(std::string("join"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardmasterServerMu.unlock();
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
			response.set_flag(SUCCESS);
        return 0;
    }
}

int ShardmasterServer::onLeave(shardmaster_messages::LeaveResponse* response_ptr,const shardmaster_messages::LeaveRequest& request) 
{
	shardmaster_messages::LeaveResponse & response = *response_ptr; 
    if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardmasterServerMu.lock(); ///考虑将此锁放到do_log之后
	

    IndexID index = node->do_log(std::string("leave"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardmasterServerMu.unlock();
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
			response.set_flag(SUCCESS);
        return 0;
    }
}
int ShardmasterServer::onMove(shardmaster_messages::MoveResponse* response_ptr,const shardmaster_messages::MoveRequest& request) 
{
	shardmaster_messages::MoveResponse & response = *response_ptr; 
    if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardmasterServerMu.lock(); ///考虑将此锁放到do_log之后
	

    IndexID index = node->do_log(std::string("move"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardmasterServerMu.unlock();
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
			response.set_flag(SUCCESS);
        return 0;
    }
}
int ShardmasterServer::onQuery(shardmaster_messages::QueryResponse* response_ptr,const shardmaster_messages::QueryRequest& request) 
{
	shardmaster_messages::QueryResponse & response = *response_ptr;
    if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    shardmasterServerMu.lock(); ///考虑将此锁放到do_log之后
	 
	if(request.num()>0&&request.num()<configs.size())
	{
		Config config = get_config(request.num());

		shardmaster_messages::Config* temp_config = response.mutable_config();
		for(int i=0;i<config.shards.size();i++)
		{
			temp_config->add_shards(config.shards[i]);
		}
		for(auto iter=config.groups.begin();iter!=config.groups.end();iter++)
		{
			std::vector<std::string>& temp_vec = iter->second;
			std::string temp_str;
			for(int j=0;j<temp_vec.size();j++)
			{
				if(!temp_str.empty())
					temp_str=temp_str+"-"; //以-分隔
				temp_str=temp_str+temp_vec[j];
			}
			shardmaster_messages::Group* me;
			me = temp_config->add_groups();
			me->set_gid(iter->first);
			me->set_servers(temp_str);
		}
		temp_config->set_num(request.num());


		response.set_flag(SUCCESS);
		shardmasterServerMu.unlock();
		return 0;
	}

    IndexID index = node->do_log(std::string("query"),request);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

    std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                        ///应该选择更好的初始化机制？
    shardmasterServerMu.unlock();

    if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
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
		{
			Config config = queryConfig;

			::shardmaster_messages::Config* temp_config = response.mutable_config();
			for(int i=0;i<config.shards.size();i++)
			{
				temp_config->add_shards(config.shards[i]);
			}
			for(auto iter=config.groups.begin();iter!=config.groups.end();iter++)
			{
				std::vector<std::string>& temp_vec = iter->second;
				std::string temp_str;
				for(int j=0;j<temp_vec.size();j++)
				{
					if(!temp_str.empty())
						temp_str=temp_str+"-"; //以-分隔
					temp_str=temp_str+temp_vec[j];
				}
				shardmaster_messages::Group* me;
				me = temp_config->add_groups();
				me->set_gid(iter->first);
				me->set_servers(temp_str);
			}
			temp_config->set_num(request.num());

			response.set_flag(SUCCESS);
		}
        return 0;
    }
}

Config ShardmasterServer::get_config(int num)
{
    if(num<0||num>=configs.size())
    {
        return configs[configs.size()-1];
    }
    else
    {
        return configs[num];
    }
}

void ShardmasterServer::appendNewConfig(Config newConfig)
{
	newConfig.num = configs.size();
	configs.push_back(newConfig);
}


int ShardmasterServer::onJoinApply(int type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	JoinApplyMessage * applymsg = (JoinApplyMessage *)(arg->p1);
	
	if(cache[applymsg->clientId]<applymsg->requestSeq)
	{
		flag = true;
		Config newConfig = get_config(-1);
		std::vector<int> newGids;
		std::unordered_map<int,int> shardsByGID;
		
		for(auto iter=(applymsg->servers).begin();iter!=(applymsg->servers).end();iter++)
		{
			if(newConfig.groups.find(iter->first)!=newConfig.groups.end())
			{
				for(auto iter1=(iter->second).begin();iter1!=(iter->second).end();iter1++)
				{
					newConfig.groups[iter->first].push_back(*iter1);
				}
			}
			else
			{
				newConfig.groups[iter->first]=iter->second;
				newGids.push_back(iter->first);
			}
		}
		
		if(newConfig.groups.empty())
		{
			newConfig.shards = std::vector<int>(NShards,0);
		}
		else if(newConfig.groups.size()<=NShards)
		{
			int minShardsPerGID, maxShardsPerGID, maxShardsPerGIDCount;
			minShardsPerGID = NShards/(newConfig.groups.size());
			maxShardsPerGIDCount = NShards%(newConfig.groups.size());
			if(maxShardsPerGIDCount!=0)
				maxShardsPerGID = minShardsPerGID+1;
			else
				maxShardsPerGID = minShardsPerGID;
			for(int i=0,j=0;i<NShards;i++)
			{
				int gid = newConfig.shards[i];
				if(gid==0||
				(minShardsPerGID == maxShardsPerGID && shardsByGID[gid] == minShardsPerGID) ||
				(minShardsPerGID < maxShardsPerGID && shardsByGID[gid] == minShardsPerGID && maxShardsPerGIDCount <= 0))
				{
					int newGid = newGids[j];
					newConfig.shards[i] = newGid;
					shardsByGID[newGid]++;
					j = (j+1)%newGids.size();
				}
				else
				{
					shardsByGID[gid]++;
					if(shardsByGID[gid]==minShardsPerGID)
						maxShardsPerGIDCount--;
				}
			}
		}
		
		cache[applymsg->clientId]=applymsg->requestSeq;
		appendNewConfig(newConfig);
	}
	else
	{
		flag = false;
	}

	//f(arg, applymsg, guard);
	return NUFT_OK;
}

int ShardmasterServer::onLeaveApply(int type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	LeaveApplyMessage * applymsg = (LeaveApplyMessage *)(arg->p1);
	
	if(cache[applymsg->clientId]<applymsg->requestSeq)
	{
		flag = true;
		Config newConfig = get_config(-1);
		std::set<int> leaveGids;
		for(int i=0;i<(applymsg->gids).size();i++)
		{
			newConfig.groups.erase(applymsg->gids[i]);
			leaveGids.insert(applymsg->gids[i]);	
		}
		if(newConfig.groups.empty())
		{
			newConfig.shards = std::vector<int>(NShards,0);
		}
		else
		{
			std::vector<int> remainingGIDs;
			for(auto iter=newConfig.groups.begin();iter!=newConfig.groups.end();iter++)
			{
				remainingGIDs.push_back(iter->first);
			}
			int shardsPerGID = NShards/newConfig.groups.size();
			if(shardsPerGID<1)
				shardsPerGID=1;
			std::unordered_map<int,int> shardsByGID;
	loop:
			for(int i=0,j=0;i<NShards;i++)
			{
				int gid = newConfig.shards[i];
				if(leaveGids.find(gid)!=leaveGids.end()||shardsByGID[gid]==shardsPerGID)
				{
					for(int k=0;k<remainingGIDs.size();k++)
					{
						if(shardsByGID[remainingGIDs[k]]<shardsPerGID)
						{
							newConfig.shards[i]=remainingGIDs[k];
							shardsByGID[remainingGIDs[k]]++;
							goto loop;
						}
					}
					int id = remainingGIDs[j];
					j = (j+1)%remainingGIDs.size();
					newConfig.shards[i]=id;
					shardsByGID[id]++;
				}	
				else
				{
					shardsByGID[gid]++;
				}
			}
		}
		cache[applymsg->clientId]=applymsg->requestSeq;
		appendNewConfig(newConfig);	
	}
	else
	{
		flag = false;
	}
	//f(arg, applymsg, guard);
	return NUFT_OK;
}

int ShardmasterServer::onMoveApply(int type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	MoveApplyMessage * applymsg = (MoveApplyMessage *)(arg->p1);
	
	if(cache[applymsg->clientId]<applymsg->requestSeq)
	{
		flag = true;
		Config newConfig = get_config(-1);
		newConfig.shards[applymsg->shard]=applymsg->gid;
		
		cache[applymsg->clientId] = applymsg->requestSeq;
		appendNewConfig(newConfig);
	}
	else
	{
		flag = false;
	}
	return NUFT_OK;
}

int ShardmasterServer::onQueryApply(int type,NuftCallbackArg * arg)
{
	std::lock_guard<std::mutex> guard((monitor_mut));
	QueryApplyMessage * applymsg = (QueryApplyMessage *)(arg->p1);
	queryConfig = get_config(applymsg->num);
	flag = true;
	return NUFT_OK;
}