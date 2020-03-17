#include "shardmaster_client.h"

ShardmasterClient::ShardmasterClient():requestSeq(0),currentIndex(0),clientId(nrand())
{
	
}

ShardmasterClient::~ShardmasterClient()
{}

void ShardmasterClient::Join(const std::unordered_map<int,std::vector<std::string>>& servers)
{
	shardmaster_messages::JoinRequest request;
	shardmaster_messages::JoinResponse response;
	request.set_clientid(clientId);
	request.set_requestseq(GetNextRequestSeq());
	shardmaster_messages::MapEntry* server;
	for(auto iter=servers.begin();iter!=servers.end();iter++)
	{
		server = request.add_servers();
		server->set_key(iter->first);
		std::string sum_str;
		for(std::string str:iter->second)
		{
			if(!sum_str.empty())
				sum_str=sum_str+"-"; //以-分隔
			sum_str=sum_str+str;
		}  
		server->set_value(sum_str);
	}
	
    while(1)
    {
        shardmasterRpcClients[currentAddr]->Join(request,response);
        int flag = response.flag();
		if(flag==AGAIN)
		{
			currentAddr = response.leader_name();
			continue;
		}
		else if(flag==FAIL)
		{
			currentIndex = (currentIndex+1)%addrs.size();
			currentAddr = addrs[currentIndex];
			break;
		}
		else if(flag==SUCCESS)
		{
			return;
		}
    }
}
void ShardmasterClient::Leave(const std::vector<int>& gids)
{
	shardmaster_messages::LeaveRequest request;
	shardmaster_messages::LeaveResponse response;
	request.set_clientid(clientId);//待定
	request.set_requestseq(GetNextRequestSeq());//待定
	std::string sum_str;
	for(int gid:gids)
	{
		if(!sum_str.empty())
			sum_str=sum_str+"-"; //以-分隔
		sum_str=sum_str+std::to_string(gid);
	}
	request.set_gids(sum_str);
	
    while(1)
    {
        shardmasterRpcClients[currentAddr]->Leave(request,response);
        int flag = response.flag();
		if(flag==AGAIN)
		{
			currentAddr = response.leader_name();
			continue;
		}
		else if(flag==FAIL)
		{
			currentIndex = (currentIndex+1)%addrs.size();
			currentAddr = addrs[currentIndex];
			break;
		}
		else if(flag==SUCCESS)
		{
			return;
		}
    }
}
void ShardmasterClient::Move(const int& gid,const int& shard)
{
	shardmaster_messages::MoveRequest request;
	shardmaster_messages::MoveResponse response;
	request.set_clientid(clientId);//待定
	request.set_requestseq(GetNextRequestSeq());//待定
	request.set_shard(shard);
	request.set_gid(gid);
    while(1)
    {
        shardmasterRpcClients[currentAddr]->Move(request,response);
        int flag = response.flag();
		if(flag==AGAIN)
		{
			currentAddr = response.leader_name();
			continue;
		}
		else if(flag==FAIL)
		{
			currentIndex = (currentIndex+1)%addrs.size();
			currentAddr = addrs[currentIndex];
			break;
		}
		else if(flag==SUCCESS)
		{
			return;
		}
    }
}
Config ShardmasterClient::Query(const int& num)
{
	shardmaster_messages::QueryRequest request;
	shardmaster_messages::QueryResponse response;
	//request.set_ClientID();//待定
	//request.set_requestSeq();//待定
	request.set_num(num);
    while(1)
    {
        shardmasterRpcClients[currentAddr]->Query(request,response);
        int flag = response.flag();
		if(flag==AGAIN)
		{
			currentAddr = response.leader_name();
			continue;
		}
		else if(flag==FAIL)
		{
			currentIndex = (currentIndex+1)%addrs.size();
			currentAddr = addrs[currentIndex];
			break;
		}
		else if(flag==SUCCESS)
		{
			Config config;
			std::vector<int> temp_shards;
			int len = response.config().shards_size();
			for(int j=0;j<len;j++)
			{
				temp_shards.push_back(response.config().shards(j));
			}
			config.shards = temp_shards;
			len = response.config().groups_size();
			
			std::unordered_map<int,std::vector<std::string>> temp_groups;
			for(int j=0;j<len;j++)
			{
				temp_groups[response.config().groups(j).gid()] = Nuke::split(response.config().groups(j).servers(), "-");
			}
			config.groups = temp_groups;
			config.num = response.config().num();
			return config;
		}
    }
}