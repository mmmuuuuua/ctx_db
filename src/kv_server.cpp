#include "kv_rpc.h"
#include "node.h"

KvServer::KvServer(const std::string& addr)
{   
    node = new RaftNode(addr,this);
    //#if defined(USE_GRPC_STREAM)
    //raft_message_server = new RaftStreamServerContext(this);
    //#else
    client_message_server = new ClientServerContext(this);

    //open kv_database
    dbDir = "/tmp/"+addr+" tmp_database";
    db = new kv_database(dbDir);
    //#endif
}

KvServer::~KvServer()
{
    delete node;
    node = nullptr;
    delete client_message_server;
    client_message_server = nullptr;
    delete db;
}

int KvServer::on_handle_client_request(client_messages::HandleClientResponse * response_ptr, const client_messages::HandleClientRequest & request)
{ 
    client_messages::HandleClientResponse & response = *response_ptr;
    std::string type = request.request_type();
    if(node->state != RaftNode::NodeState::Leader)
    {
        response.set_flag(AGAIN);
        response.set_leader_name(node->get_leader_name());
        return 0;
    }
    kv_mut.lock();////KvServer的锁,最开始一个服务器节点由一个RaftNode对象构成，只有一个锁，这样子很容易造成死锁，在on_handle_client_request（响应客户端请求的函数）中，
                        ////因为要操作全局变量，所以需要加锁，最开始由于在调用do_log函数（添加log）之前没有解锁，由于do_log函数中需要获取这个锁，就造成了死锁，后来第二个版本中增
                        ////加了一个上层对象KvServer，将存储key-value数据的数据结构还有用于线程间通信的条件变量数据结构搬到上层对象KvServer中，这个KvServer对象中也有一个锁，
                        ////获取全局变量时只需要使用这个锁，由这个上层对象接受客户端的请求，再调用底层的RaftNode对象，这样子两个锁的机制就不会造成死锁。
    if(request.request_type()=="GET")
    {
        std::string key_ = request.key();
        std::string value_;
        bool flag_ = db->get(key_,value_);
        kv_mut.unlock();
        if(flag_==true)
        {
            response.set_flag(SUCCESS);
            response.set_value(value_);
            return 0;
        }
        else
        {
            response.set_flag(FAIL);
            return 0;
        }
    }
    if(request.request_type()=="SET")
    {
        std::string key = request.key();
        std::string value = request.value();
        long long expirerequestId = request.expirerequestid();
        long long requestId = request.requestid();

        IndexID index = node->do_log(key+"="+value, expirerequestId, requestId);  ///这里不用加锁，因为do_log函数会使用RaftNode的锁进行加锁处理，

        std::unique_lock<std::mutex> lk(map_[index].first); ///这里用map的机制对std::pair<std::mutex,std::condition_variable>默认初始化，
                                                            ///应该选择更好的初始化机制？
        kv_mut.unlock();
        //std::unique_lock<std::mutex> lk(raft_node->mut_);

        if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{5})== std::cv_status::timeout)
        //if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
        {
            lk.unlock();
            response.set_flag(FAIL);
            return 0;
        }
        else{
            lk.unlock();
            response.set_flag(SUCCESS);
            return 0;
        }
    }


    
}
