#pragma once

#include <unistd.h>
#include <sys/time.h>
#include <random>  //https://blog.csdn.net/dongshixian/article/details/46496787
#include "shardmaster_config.h"

struct ShardkvClient
{
	ShardkvClient();
	~ShardkvClient();
	
	//API
	void Get(std::string& key,std::string& value);
	void Put(std::string& key,std::string& value)
	{
		PutAppend(key,value,std::string("put"));
	}
	void Append(std::string& key,std::string& value)
	{
		PutAppend(key,value,std::string("append"));
	}
	
	void PutAppend(std::string& key,std::string& value,std::string& op);

	//辅助函数
    long long GetCurrentTime()      //获得unix时间戳，精确到毫秒
    {    
       struct timeval tv;    
       gettimeofday(&tv,NULL);    //该函数在sys/time.h头文件中
       return tv.tv_sec * 1000 + tv.tv_usec / 1000;    
    } 

	unsigned long long nrand()
	{
		std::independent_bits_engine<std::default_random_engine,32,unsigned long long> engine;
		return engine();
	}

	//rpc客户端
	void AddShardkvNode(std::string Addr)
	{
		shardkvRpcClients[Addr]=std::make_shared<ShardkvRpcClient>(Addr);
		if(currentAddr.empty())
			currentAddr = Addr;
	}
	void SetCurrentAddr(const std::string& addr)
	{
		currentAddr = addr;
	}
	ShardmasterClient* mck;
	std::vector<std::string> addrs;
    std::unordered_map<std::string,std::shared_ptr<ShardkvRpcClient>> shardkvRpcClients;
    int currentIndex;////当前尝试发送的node
    std::string currentAddr;
	
	//私有变量
	Config config;
	long long lastRequestId;
	unsigned long long clientId;

};

ShardkvClient::ShardkvClient()
{
	mck = new ShardmasterClient();
	clientId = nrand();
	lastRequestId = 0;
	config = mck.Query(-1);
}

ShardkvClient::~ShardkvClient()
{
	delete mck;
}

void ShardkvClient::Get(std::string& key,std::string& value)
{
	while(1)
	{
		int shard = key2shard(key);
		int gid = config.shards[shard];
		shardkv_messages::GetRequest request;
        shardkv_messages::GetResponse response;
		request.set_confignum(config.num);
		request.set_key(key);
		if(config.groups.find(gid)!=config.groups.end())
		{
			for(int i=0;i<config.groups[gid].size();i++)
			{
				ShardkvRpcClients[config.groups[gid][i]]->Get(request,response);
				int flag = response.flag();
				if(flag==AGAIN)
				{
					currentAddr = response.leader_name();
					continue;
				}
				else if(flag==FAIL)
				{
					break;
				}
				else if(flag==SUCCESS)
				{
					value = response.value();
					return;
				}
			}
		}
		usleep(100000);
		config = mck.Query(config.num+1);
	}
}

void ShardkvClient::PutAppend(std::string& key,std::string& value,std::string& op)
{
	shardkv_messages::PutAppendRequest request;
    shardkv_messages::PutAppendResponse response;

	long long requestId = GetCurrentTime() - clientId;
	request.set_expirerequestid(lastRequestId);
	request.set_requestid(requestId);
	request.set_confignum(config.num);
	request.set_key(key);
	request.set_value(value);
	request.set_op(op);
	lastRequestId = requestId;

	while(1)
	{
		int shard = key2shard(key);
		int gid = config.shards[shard];
		
		request.set_confignum(config.num);
		if(config.groups.find(gid)!=config.groups.end())
		{
			for(int i=0;i<config.groups[gid].size();i++)
			{
				ShardkvRpcClients[config.groups[gid][i]]->Get(request,response);
				int flag = response.flag();
				if(flag==AGAIN)
				{
					currentAddr = response.leader_name();
					continue;
				}
				else if(flag==FAIL)
				{
					break;
				}
				else if(flag==SUCCESS)
				{
					return;
				}
			}
		}
		usleep(100000);
		config = mck.Query(config.num+1);
	}
}



