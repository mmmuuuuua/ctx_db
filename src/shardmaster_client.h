#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>


#include <unistd.h>
#include <sys/time.h>
#include <random>  //https://blog.csdn.net/dongshixian/article/details/46496787

#include <grpcpp/grpcpp.h>
#include "grpc/shardmaster_messages.grpc.pb.h"
#include "grpc/shardmaster_messages.pb.h"
#include "shardmaster_config.h"
#include "shardmaster_rpc_client.h"

class ShardmasterClient
{
    public:
        ShardmasterClient();
        ~ShardmasterClient();
        void AddShardmasterNode(std::string Addr)
        {
            shardmasterRpcClients[Addr]=std::make_shared<ShardmasterRpcClient>(Addr);
            if(currentAddr.empty())
                currentAddr = Addr;
        }
        void SetCurrentAddr(const std::string& addr)
        {
            currentAddr = addr;
        }

        int GetNextRequestSeq()
        {
            ShardmasterClientMu.lock();
            requestSeq++;
            ShardmasterClientMu.unlock();
            return requestSeq;
        }
		
		unsigned long long nrand()
		{
			std::independent_bits_engine<std::default_random_engine,32,unsigned long long> engine;
			return engine();
		}
		
		void Join(const std::unordered_map<int,std::vector<std::string>>& servers);
		void Leave(const std::vector<int>& gids);
		void Move(const int& gid,const int& shard);
		Config Query(const int& num);
    private:
        std::mutex ShardmasterClientMu;
        std::vector<std::string> addrs;
        std::unordered_map<std::string,std::shared_ptr<ShardmasterRpcClient>> shardmasterRpcClients;
        int currentIndex;////当前尝试发送的node
        std::string currentAddr;
		
		///
        long long requestSeq;
		unsigned long long clientId;
};


