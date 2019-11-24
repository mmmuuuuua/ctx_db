#include "node.h"
#include <condition_variable>

int RaftNode::on_handle_client_request(raft_messages::HandleClientResponse * response_ptr, const raft_messages::HandleClientRequest & request)
{
    mut.lock();
    if(!is_running_unguard()){
        return -1;
    }

    raft_messages::HandleClientResponse & response = *response_ptr;
    if(request.request_type()=="GET")
    {
        std::string key_ = request.key();
        std::string value_;
        bool flag_ = db->get(key_,value_);
        if(flag_==true)
        {
            response.set_flag(GET_SUCCESS);
            response.set_value(value_);
            mut.unlock();
            return 0;
        }
        else
        {
            response.set_flag(GET_FAIL);
            mut.unlock();
            return 0;
        }
    }
    if(request.request_type()=="SET")
    {
        if(state != NodeState::Leader)
        {
            response.set_flag(SET_AGAIN);
            response.set_leader_name(get_leader_name());
            mut.unlock();
            return 0;
        }
        //std::condition_variable cv;
        std::string key = request.key();
        std::string value = request.value();

        IndexID index = last_log_index() + 1;
        ////到这里应该释放之前lock()的锁了,因为后续do_log,on_append_entries_response等函数需要用到锁。
        ////最开始的写法是在do_log之后释放锁，然而do_log会调用guard获取锁，从而造成死锁！！！！
        mut.unlock();

        do_log(key+"="+value);
    
       
        /////选择一种线程间通信的方式，这里选择的是条件变量。
        /////思路一：用一个map,key是do_log的index,value是对应的pair<std::mutex,std::condition_variable> ,如果commit成功，
        /////       那么通知commit之前的所有信号量

        

        //map_[index] = make_pair(std::condition_variable(),std::mutex());
        std::unique_lock<std::mutex> lk(map_[index].first);

        //std::unique_lock<std::mutex> lk(raft_node->mut_);
        
        //std::chrono::duration<int>
        if(map_[(int)(index)].second.wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
        //if((raft_node->cond_).wait_for(lk,std::chrono::duration<int>{60})== std::cv_status::timeout)
        {
            lk.unlock();
            response.set_flag(SET_FAIL);
            return 0;
        }
        else{
            lk.unlock();
            response.set_flag(SET_SUCCESS);
            return 0;
        }
    }
}