#pragma once

#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <grpcpp/grpcpp.h>
#include "grpc/client_messages.grpc.pb.h"
#include "grpc/client_messages.pb.h"
#include "kv_database.h"


struct KvServer
{
    enum FLAG{
        SUCCESS=0,
        FAIL=1,
        AGAIN=2,
    };
    KvServer(const std::string& addr);
    ~KvServer();

    int on_handle_client_request(client_messages::HandleClientResponse * response_ptr, const client_messages::HandleClientRequest & request);


    struct RaftNode* node;
    mutable std::mutex kv_mut; ///注意区分这个锁和RaftNode里面的锁


    struct ClientServerContext * client_message_server = nullptr;
    kv_database * db;
    std::string dbDir;

    int sequence_;//解决幂等性的序号
    //std::mutex mut_;
    //std::condition_variable cond_;//当需要复制的条目commit或者超时的时候用于唤醒grpc服务端的调用
    std::unordered_map<int,std::pair<std::mutex,std::condition_variable>> map_;
    std::unordered_map<int,std::string> cache;
};




