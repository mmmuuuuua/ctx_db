#pragma once

#include <gtest/gtest.h>
#include "../node.h"
#include "../utils.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <future>
#include <tuple>
#include "../kv_client.h"
#include "../kv_server.h"

//#include "../shardmaster_config.h"

#if defined(_HIDE_TEST_DEBUG)
#define debug_test(...)
#else
#define debug_test printf
#endif

#define RN RaftNode::NodeState

uint16_t new_port_base = 7200;
uint16_t port_base = 7100;
uint16_t shardmaster_raftnode_port_base = 6100;
uint16_t shardmaster_port_base = 6200;
uint16_t shardkv_raftnode_port_base = 6300;
uint16_t shardkv_port_base = 6400;
uint16_t shardkv_inner_port_base = 6500;

std::vector<RaftNode *> nodes;
std::vector<KvServer*> servers;
std::vector<ShardmasterServer*> shardmasterServers;

std::vector<ShardkvServer*> shardkvServers;

struct Group{
	int gid;
	std::vector<ShardkvServer*> shardkvServers;
};

std::vector<Group*> groups;

struct LogMon{
    std::string data;
    bool applied = false;
    int tid = 0;
};
std::map<int, LogMon> logs;
std::map<int, std::string> storage;
std::map<int, std::string> correct_storage;
std::mutex monitor_mut;

const int max_wait_election = 1000*30;

std::chrono::duration<int, std::milli> TimeEnsureElection(){
    std::chrono::duration<int, std::milli> ms{default_timeout_interval_lowerbound};
    return ms;
}
std::chrono::duration<int, std::milli> TimeEnsureSuccessfulElection(){
    std::chrono::duration<int, std::milli> ms{(default_timeout_interval_upperbound + default_election_fail_timeout_interval) * 3};
    return ms;
}
std::chrono::duration<int, std::milli> TimeEnsureNoElection(){
    std::chrono::duration<int, std::milli> ms{default_timeout_interval_lowerbound / 2};
    return ms;
}

// std::function<NuftResult(NUFT_CB_TYPE, NuftCallbackArg *)> 
NuftResult NopCallback(NUFT_CB_TYPE type, NuftCallbackArg * arg){
    return NUFT_OK;
}

NuftCallbackFunc NopCallbackFunc = NopCallback;

void FreeRaftNodes(){
    // We move Leaders to the last of vector `nodes`, so we delete them at the end.
    // Before Leaders die, they will send their last AppendEntries RPC, which,
    // if handled without care, will interfere tests in the future.
    std::sort(nodes.begin(), nodes.end(), [](auto a, auto b){
        return a->state < b->state;
    });
    debug_test("=== Clearing %d\n", nodes.size());
    using namespace std::chrono_literals;
    for(auto nd: nodes){
        debug_test("=== Deleting %s\n", nd->name.c_str());
        delete nd;
        nd = nullptr;
    }
    nodes.clear();
    std::this_thread::sleep_for(1s);
}

void MakeRaftNodes(int n){
    // Must do cleaning first, because ASSERT will abort.
    FreeRaftNodes();
    nodes.resize(n);

    for(int i = 0; i < n; i++){
        nodes[i] = new RaftNode(std::string("127.0.0.1:") + std::to_string(port_base + i));
    }
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            if(i == j) continue;
            nodes[i]->add_peer(nodes[j]->name);
        }
    }
    for(auto nd: nodes){
        nd->run();
    }
}

void FreeKvServers()
{
    std::sort(servers.begin(), servers.end(), [](auto a, auto b){
        return a->node->state < b->node->state;
    });
    using namespace std::chrono_literals;
    for(auto nd: servers){
        debug_test("=== Deleting %s\n", nd->node->name.c_str());
        delete nd;
        nd = nullptr;
    }
    servers.clear();
    std::this_thread::sleep_for(1s);
}

void MakeKvServers(int n)
{
    FreeRaftNodes();
    servers.resize(n);
    nodes.resize(n);

    for(int i=0;i<n;i++)
    {
        servers[i] = new KvServer(std::string("127.0.0.1:") + std::to_string(port_base + i));
        nodes[i] = servers[i]->node;
    }
    for(int i=0;i<n;i++)
    {
        for(int j=0;j<n;j++)
        {
            if(i==j)
                continue;
            servers[i]->node->add_peer(servers[j]->node->name);
        }
    }

    for(auto it:servers)
        it->node->run();
}

void FreeShardmasterServers()
{
    std::sort(shardmasterServers.begin(),shardmasterServers.end(),[](auto a,auto b){
        return a->node->state<b->node->state;
    });
    using namespace std::chrono_literals;
    for(auto nd:shardmasterServers){
        debug_test("=== Deleting %s\n", nd->node->name.c_str());
        delete nd;
        nd = nullptr;
    }
    shardmasterServers.clear();
    std::this_thread::sleep_for(1s);
}


void MakeShardmasterServers(int n)
{
    FreeShardmasterServers();
    shardmasterServers.resize(n);
    nodes.resize(n);

    for(int i=0;i<n;i++)
    {
        shardmasterServers[i] = new ShardmasterServer(std::string("127.0.0.1:") + std::to_string(shardmaster_raftnode_port_base + i),
													  std::string("127.0.0.1:") + std::to_string(shardmaster_port_base + i));
        nodes[i] = shardmasterServers[i]->node;
    }
    for(int i=0;i<n;i++)
    {
        for(int j=0;j<n;j++)
        {
            if(i==j)
                continue;
            shardmasterServers[i]->node->add_peer(shardmasterServers[j]->node->name);
        }
    }
    for(auto it:shardmasterServers)
        it->node->run();
}


//MakeShardkvServers和FreeShardkvServers暂时闲置，无用
/*
void FreeShardkvServers()
{
    std::sort(shardkvServers.begin(),shardkvServers.end(),[](auto a,auto b){
        return a->node->state<b->node->state;
    });
    using namespace std::chrono_literals;
    for(auto nd:shardkvServers){
        debug_test("=== Deleting %s\n", nd->node->name.c_str());
        delete nd;
        nd = nullptr;
    }
    shardkvServers.clear();
    std::this_thread::sleep_for(1s);
}

void MakeShardkvServers(int gn,int n)
{
    FreeShardkvServers();
    shardkvServers.resize(n);

    for(int i=0;i<n;i++)
    {
        shardkvServers[i] = new ShardkvServer(std::string("127.0.0.1:") + std::to_string(port_base + i));
        nodes[i] = shardkvServers[i]->node;
    }
    for(int i=0;i<n;i++)
    {
        for(int j=0;j<n;j++)
        {
            if(i==j)
                continue;
            shardkvServers[i]->node->add_peer(shardkvServers[j]->node->name);
        }
    }
    for(auto it:shardkvServers)
        it->node->run();
}
*/
void FreeGroups()
{
	
}

void MakeGroups(int gn,int n)
{
	groups.resize(gn);
	for(int i=0;i<gn;i++)
	{
		groups[i] = new Group();
		groups[i]->gid = 100+i;
		(groups[i]->shardkvServers).resize(n);
		for(int j=0;j<n;j++)
		{
			groups[i]->shardkvServers[j] = new ShardkvServer(std::string("127.0.0.1:")+std::to_string(shardkv_raftnode_port_base+groups[i]->gid*10+j),
															std::string("127.0.0.1:")+std::to_string(shardkv_port_base+groups[i]->gid*10+j),
															std::string("127.0.0.1:")+std::to_string(shardkv_inner_port_base+groups[i]->gid*10+j),
															groups[i]->gid);
		}	
		for(int j=0;j<n;j++)
		{
			for(int k=0;k<n;k++)
			{
				if(j==k)
					continue;
				groups[i]->shardkvServers[j]->node->add_peer(groups[i]->shardkvServers[k]->node->name);
			}
		}
		for(auto it:groups[i]->shardkvServers)
			it->node->run();
	}
	
	
	for(int i=0;i<gn;i++)
	{
		for(int j=0;j<n;j++)
		{
			for(int k=0;k<gn;k++)
			{
				for(int g=0;g<n;g++)
				{
					if(i==k)
						continue;
					//以std::string型的地址作为key寻找对应的ShardkvInnerRpcClient
					groups[i]->shardkvServers[j]->AddShardkvInnerRpcClients(groups[k]->shardkvServers[g]->innerRpcAddr);
				}
			}
		}
	}
}


// RaftNode * MakeNewRaftNode(uint16_t port, const std::vector<std::string> & app, const std::vector<std::string> & rem){
RaftNode * AddRaftNode(uint16_t port){
    RaftNode * new_nd = new RaftNode(std::string("127.0.0.1:") + std::to_string(port));
    for(auto nd: nodes){
        // Only set up newly added nodes.
        if(new_nd != nd)
            new_nd->add_peer(nd->name);
    }
    new_nd->run();
    nodes.push_back(new_nd);
    return new_nd;
}

int CountLeader(){
    // TODO Is it necessary to take term into account?
    int tot = 0;
    for(auto nd: nodes){
        if(nd->state == RN::Leader && !nd->paused){
            tot ++;
        }
    }
    return tot;
}

int CountLeader2(){
    int tot = 0;
    for(auto nd:servers){
        if(nd->node->state == RN::Leader&&!nd->node->paused){
            tot++;
        }
    }
}

RaftNode * PickNode(std::unordered_set<int> nt){
    // This function won't check if the are multiple Leader.
    for(auto nd: nodes){
        if(nt.find(nd->state) != nt.end() && !nd->paused){
            return nd;
        }
    }
    return nullptr;
}


//从指定的group中寻找到一个state等于nt中state的node
RaftNode * PickGroupNode(int groupIndex,std::unordered_set<int> nt){
    // This function won't check if the are multiple Leader.
	Group* group = groups[groupIndex];
    for(auto server: group->shardkvServers)
	{
        if(nt.find(server->node->state) != nt.end() && !server->node->paused){
            return server->node;
        }
    }
    return nullptr;
}


int PickIndex(std::unordered_set<int> nt, int order = 0){
    // This function won't check if the are multiple Leader.
    for(int i = 0; i < nodes.size(); i++){
        RaftNode * nd = nodes[i];
        if(nt.find(nd->state) != nt.end() && !nd->paused){
            if(order){
                order--;
            }else{
                return i;
            }
        }
    }
    return -1;
}

void DisableNode(RaftNode * victim, std::lock_guard<std::mutex> & guard){
    debug_test("GTEST: DisableNode %s\n", victim->name.c_str());
    victim->stop(guard);
    for(auto nd: nodes){
        // Now all other nodes can't receive RPC from victim.
        // We don't disable send because we want to observe.
        if(nd != victim){
            // NOTICE The guard only protects `victim`
            nd->disable_receive(victim->name);
        }
    }
}

void DisableNode(RaftNode * victim){
    std::lock_guard<std::mutex> guard((victim->mut));
    DisableNode(victim, guard);
}
void EnableNode(RaftNode * victim, std::lock_guard<std::mutex> & guard){
    victim->resume(guard);
    for(auto nd: nodes){
        if(nd != victim){
            nd->enable_receive(victim->name);
        }
    }
}
void EnableNode(RaftNode * victim){
    std::lock_guard<std::mutex> guard((victim->mut));
    EnableNode(victim, guard);
}

////新增
void DisableGroupNode(int groupIndex,int serverIndex, std::lock_guard<std::mutex> & guard)
{
	ShardkvServer* server= groups[groupIndex].shardkvServers[serverIndex]
	server->node->stop(guard);
	Group* group = groups[groupIndex];
	for(int i=0;i<(group->shardkvServers).size();i++)
	{
		if(i!=serverIndex)
			(group->shardkvServers)[i]->node->disable_receive(server->node->name);
	}
	server->tobe_paused = true;
}
void DisableGroupNode(int groupIndex,int serverIndex)
{
	ShardkvServer* server= groups[groupIndex].shardkvServers[serverIndex]
	std::lock_guard<std::mutex> guard((server->shardkvServerMu));
	DisableGroupNode(groupIndex,serverIndex,guard);
}

void DisableGroup(int groupIndex)
{
	Group* group = groups[groupIndex];
	for(int i=0;i<(group->shardkvServers).size();i++)
	{
		DisableGroupNode(groupIndex,i);
	}
}


void EnableGroupNode(int groupIndex,int serverIndex, std::lock_guard<std::mutex> & guard)
{
	ShardkvServer* server= groups[groupIndex].shardkvServers[serverIndex];
	server->node->resume(guard);
	Group* group = groups[groupIndex];
	for(int i=0;i<(group->shardkvServers).size();i++)
	{
		if(i!=serverIndex)
			(group->shardkvServers)[i]->node->enable_receive(server->node->name);
	}
	server->tobe_paused = false;
}
void EnableGroupNode(int groupIndex,int serverIndex)
{
	ShardkvServer* server= groups[groupIndex].shardkvServers[serverIndex]
	std::lock_guard<std::mutex> guard((server->shardkvServerMu));
	EnableGroupNode(groupIndex,serverIndex,guard);
}

void EnableGroup(int groupIndex)
{
	Group* group = groups[groupIndex];
	for(int i=0;i<(group->shardkvServers).size();i++)
	{
		EnableGroupNode(groupIndex,i);
	}
}



void DisableSend(RaftNode * victim, std::vector<int> nds){
    for(int i: nds){
        RaftNode * nd = nodes[i];
        if(nd != victim){
            victim->disable_send(nodes[i%nodes.size()]->name);
        }
    }
}
void EnableSend(RaftNode * victim, std::vector<int> nds){
    for(int i: nds){
        RaftNode * nd = nodes[i];
        if(nd != victim){
            victim->enable_send(nodes[i%nodes.size()]->name);
        }
    }
}
void DisableReceive(RaftNode * victim, std::vector<int> nds){
    for(int i: nds){
        RaftNode * nd = nodes[i];
        if(nd != victim){
            victim->disable_receive(nodes[i%nodes.size()]->name);
        }
    }
}
void EnableReceive(RaftNode * victim, std::vector<int> nds){
    for(int i: nds){
        RaftNode * nd = nodes[i];
        if(nd != victim){
            victim->enable_receive(nodes[i%nodes.size()]->name);
        }
    }
}

void CrashNode(RaftNode * victim, std::lock_guard<std::mutex> & guard){
    // NOTICE `CrashNode` is guarded, so we must handle with later arrived RPCs carefully, 
    // To avoid a complicated deadlock, See client_stream_sync.cpp:RaftMessagesStreamClientSync::handle_response():t2
    // See stream.CrashNode.block.log
    debug_test("GTEST: Crash Node %s\n", victim->name.c_str());
    DisableNode(victim, guard);
    victim->logs.clear();
    victim->current_term = 77777;
    victim->vote_for = "LLLLL";
    victim->reset_peers(guard);
    victim->last_applied = -1;
    victim->commit_index = -1;
    if(victim->trans_conf) {
        delete victim->trans_conf;
    }
    victim->trans_conf = nullptr;
    debug_test("GTEST: Crash Node %s FINISHED\n", victim->name.c_str());
}
void CrashNode(RaftNode * victim){
    std::lock_guard<std::mutex> guard((victim->mut));
    CrashNode(victim, guard);
}

void RecoverNode(RaftNode * victim, std::lock_guard<std::mutex> & guard){
    debug_test("GTEST: Recover Node %s\n", victim->name.c_str());
    victim->run(guard, victim->name);
    EnableNode(victim, guard);
}
void RecoverNode(RaftNode * victim){
    std::lock_guard<std::mutex> guard((victim->mut));
    RecoverNode(victim, guard);
}

template <typename F>
void WaitAny(RaftNode * ob, NUFT_CB_TYPE ev, F f, uint64_t timeout = 0){
    std::mutex mut;
    std::condition_variable cv;

    NuftCallbackFunc cb = [&mut, &cv, f](NUFT_CB_TYPE type, NuftCallbackArg * arg) -> NuftResult{
        if(f(type, arg)){
            std::unique_lock<std::mutex> lk(mut);
            cv.notify_one();
        }
        return 0;
    };
    // NOTICE Must firstly `set_callback`, then require mut.
    // Consider:
    // 1. `WaitAny` holds `lk(mut)`, Original `ob` holds RaftNode's mutex
    // 2. `WaitAny` calls `set_callback`, which requires RaftNode's mutex. So `WaitAny` blocks on `set_callback`
    // 3. `ob` requires `lk(mut)` to notify, so `ob` will also block.
    ob->set_callback(ev, cb);
    {
        std::unique_lock<std::mutex> lk(mut);
        // fprintf(stderr, "Start wait\n");
        if(timeout){
            cv.wait_for(lk, std::chrono::duration<int, std::milli> {timeout});
        }else{
            cv.wait(lk);
        }
        // fprintf(stderr, "End wait\n");
    }
    // NOTICE Blocking may happen after this line
    ob->set_callback(ev, NopCallbackFunc);
    return;
}


int WaitApplied(RaftNode * ob, IndexID index){
    debug_test("GTEST: Wait Applied[%d] at %s\n", index, ob->name.c_str());
    if(ob->last_applied >= index){
        debug_test("GTEST: May already applied. ob->last_applied %lld >= index %lld\n", ob->last_applied, index);
        return 1;
    }
    // NOTICE Applied operation may happen exactly HERE before we setup a hook.
    // If no entries are add later(such as test Persist.FrequentLost whcih demands entries applied one by one),
    // WaitAny will block forever. See persist.frequentcrash.log. So we add a timeout to WaitAny here.
    // NOTICE There is a slight chance of SEGEV in early versions, see persist.frequentcrash.core2.log
    // Now, we use `set_callback` which requires a lock when setting `callbacks`, this will eliminate the problem.
    while(1){
        auto cbb = [index](int type, NuftCallbackArg * arg){
            ApplyMessage * applymsg = (ApplyMessage *)(arg->p1);
            if(applymsg->index >= index){
                debug_test("GTEST: Applied! %lld %lld\n", applymsg->index, index);
                return 1;
            }
            debug_test("GTEST: Not enough. applymsg->index %lld < index %lld\n", applymsg->index, index);
            return 0;
        };
        WaitAny(ob, NUFT_CB_ON_APPLY, cbb, 1000);
        if(ob->last_applied >= index){
            // debug_test("GTEST: Oops! Maybe we set up a hook too slowly at %lld!\n", index);
            break;
        }else{
            debug_test("GTEST: WaitApplied Timeout! Retry\n");
        }
    }
    debug_test("GTEST: Wait Applied[%d] Finish at %s\n", index, ob->name.c_str());
    return 1;
}


void WaitElection(RaftNode * ob){
    // ob->debugging = 1;
    debug_test("GTEST: WaitElection ob %s\n", ob->name.c_str());
    WaitAny(ob, NUFT_CB_ELECTION_END, [&](NUFT_CB_TYPE type, NuftCallbackArg * arg){
        if(Nuke::in(arg->a1, {RaftNode::ELE_SUC, RaftNode::ELE_FAIL, RaftNode::ELE_SUC_OB})){
            debug_test("GTEST: WaitElection Finish with %d.\n", arg->a1);
            return 1;
        }
        return 0;
    }, max_wait_election);
    // ob->debugging = 0;
}

void WaitElectionStart(RaftNode * ob, std::unordered_set<int> ev){
    debug_test("GTEST: WaitElectionStart ob %s\n", ob->name.c_str());
    WaitAny(ob, NUFT_CB_ELECTION_START, [&](NUFT_CB_TYPE type, NuftCallbackArg * arg){
        if(std::find(ev.begin(), ev.end(), arg->a1) != ev.end()){
            debug_test("GTEST: WaitElectionStart ob %s, Finish with %d.\n", ob->name.c_str(), arg->a1);
            return 1;
        }
        return 0;
    });
}

void WaitConfig(RaftNode * ob, NUFT_CB_TYPE ev, std::unordered_set<int> st){
    debug_test("GTEST: WaitConfig ob %s\n", ob->name.c_str());
    WaitAny(ob, ev, [&](NUFT_CB_TYPE type, NuftCallbackArg * arg){
        if(std::find(st.begin(), st.end(), arg->a1) != st.end()){
            debug_test("GTEST: WaitConfig Finish with %d.\n", arg->a1);
            return 1;
        }
        return 0;
    });
}

int CheckKvCommit(IndexID index, const std::string & key,const std::string & value){
    // NOTICE This function requires RaftNode's inner lock.
    int support = 0;
    for(auto nd: nodes){
        ::raft_messages::LogEntry log;
        if((!nd->is_running()) || nd->get_log(index, log) != NUFT_OK){
            // Invalid node or no log found at index.
            if(!nd->is_running())
                debug_test("GTEST: In CheckCommit: %s is not running.\n", nd->name.c_str());
            if(nd->get_log(index, log) != NUFT_OK)
                debug_test("GTEST: In CheckCommit: %s have no %lld log.\n", nd->name.c_str(), index);
            continue;
        }

        std::string tmp_value;
        bool flag = ((KvServer*)nd->server_)->db->get(key,tmp_value);
        if(flag==1&&tmp_value == value)
            support++;
    }
    return support;
}

int CheckCommit(IndexID index, const std::string & value){
    // NOTICE This function requires RaftNode's inner lock.
    int support = 0;
    for(auto nd: nodes){
        ::raft_messages::LogEntry log;
        if((!nd->is_running()) || nd->get_log(index, log) != NUFT_OK){
            // Invalid node or no log found at index.
            if(!nd->is_running())
                debug_test("GTEST: In CheckCommit: %s is not running.\n", nd->name.c_str());
            if(nd->get_log(index, log) != NUFT_OK)
                debug_test("GTEST: In CheckCommit: %s have no %lld log.\n", nd->name.c_str(), index);
            continue;
        }
        TermID term = ~0;
        if(log.data() == value){
            if(term == ~0){
                term = log.term();
            }else if(term != log.term()){
                // Conflict at index
                debug_test("GTEST: In CheckCommit: Conflict at %lld, %s's term is %llu, term2 is %llu'.\n", index, nd->name.c_str(), log.term(), term);
                return -1;
            }
            if(nd->commit_index >= index){
                support++;
            }
        }else{
            // Conflict at index
            debug_test("GTEST: In CheckCommit: Conflict at %lld, %s is '%s', want '%s'.\n", index, nd->name.c_str(), log.data().c_str(), value.c_str());
            return -1;
        }
    }
    return support;
}

int CheckLog(IndexID index, const std::string & value){
    int support = 0;
    for(auto nd: nodes){
        ::raft_messages::LogEntry log;
        if((!nd->is_running()) || nd->get_log(index, log) != NUFT_OK){
            // Invalid node or no log found at index.
            if(!nd->is_running())
                debug_test("GTEST: CheckLog: %s is not running.\n", nd->name.c_str());
            if(nd->get_log(index, log) != NUFT_OK)
                debug_test("GTEST: CheckLog: %s have no %lld log.\n", nd->name.c_str(), index);
            continue;
        }
        TermID term = ~0;
        if(log.data() == value){
            if(term == ~0){
                term = log.term();
            }else if(term != log.term()){
                // Term conflict at index
                return -1;
            }
            support++;
        }else{
            // Data conflict at index
            return -1;
        }
    }
    return support;
}

template<typename F>
void KvDatabaseApplied(RaftNode * ob,F f){
    auto cb = [f](int type,NuftCallbackArg * arg)->int{
        std::lock_guard<std::mutex> guard((monitor_mut));
        ApplyMessage * applymsg = (ApplyMessage *)(arg->p1);
        f(arg, applymsg, guard);
        return NUFT_OK;
    };
    ob->set_callback(NUFT_CB_ON_APPLY, cb);
}

template <typename F>
void MonitorApplied(RaftNode * ob, F f){
    // std::shared_ptr<std::mutex> pmut = std::make_shared<std::mutex>();
    auto cb = [f](int type, NuftCallbackArg * arg) -> int{
        std::lock_guard<std::mutex> guard((monitor_mut));
        ApplyMessage * applymsg = (ApplyMessage *)(arg->p1);
        f(arg, applymsg, guard);
        return NUFT_OK;
    };
    ob->set_callback(NUFT_CB_ON_APPLY, cb);
}
void StopMonitorApplied(RaftNode * ob){
    ob->set_callback(NUFT_CB_ON_APPLY, NopCallbackFunc);
}

int RegisterLog(std::lock_guard<std::mutex> & guard, const std::string & s, RaftNode * leader, int log_id, int tid = 0){
    debug_test("GTEST: RegisterLog at %d\n", log_id);
    logs[log_id] = LogMon{s, false, tid};
    return log_id;
}
int RegisterLog(const std::string & s, RaftNode * leader, int log_id, int tid = 0){
    std::lock_guard<std::mutex> guard((monitor_mut));
    return RegisterLog(guard, s, leader, log_id, tid);
}

int MajorityCount(int n){
    return (n + 1) / 2;
}

void NetworkPartition(std::vector<int> p1, std::vector<int> p2){
    std::vector<std::string> sp1, sp2;
    std::transform(p1.begin(), p1.end(), std::back_inserter(sp1), [](int x){return std::to_string(x);});
    std::transform(p2.begin(), p2.end(), std::back_inserter(sp2), [](int x){return std::to_string(x);});
    debug_test("GTEST: NetworkPartition: part1 {%s}, part2 {%s}.\n", Nuke::join(sp1.begin(), sp1.end(), ",").c_str(), Nuke::join(sp2.begin(), sp2.end(), ",").c_str());
    for(auto pp1: p1){
        for(auto pp2: p2){
            nodes[pp1%nodes.size()]->disable_receive(nodes[pp2%nodes.size()]->name);
            nodes[pp2%nodes.size()]->disable_receive(nodes[pp1%nodes.size()]->name);
        }
    }
}

void RecoverNetworkPartition(std::vector<int> p1, std::vector<int> p2){
    debug_test("GTEST: RecoverNetworkPartition: part1 {%s}, part2 {%s}.\n", 
            Nuke::join(p1.begin(), p1.end(), ",", [](int x){return std::to_string(x);}).c_str(),
            Nuke::join(p2.begin(), p2.end(), ",", [](int x){return std::to_string(x);}).c_str());
    for(auto pp1: p1){
        for(auto pp2: p2){
            nodes[pp1]->enable_receive(nodes[pp2]->name);
            nodes[pp2]->enable_receive(nodes[pp1]->name);
        }
    }
}

void DisconnectAfterReplicateTo(RaftNode * leader, const std::string & log_str, std::unordered_set<int> nds){
    using namespace std::chrono_literals;
    std::unordered_set<int> mset;
    std::vector<int> sset;
    for(auto x: nds){
        mset.insert(x % nodes.size());
    }
    for(int i = 0; i < nodes.size(); i++){
        if(std::find(mset.begin(), mset.end(), i) == mset.end()){
            sset.push_back(i);
        }
    }
    debug_test("GTEST: In DisconnectAfterReplicateTo: Mute %s\n", Nuke::join(sset.begin(), sset.end(), ";", [](int x){return std::to_string(x);}).c_str());
    DisableSend(leader, sset);
    leader->do_log(log_str);
    // Make sure already sent to sset
    // NOTICE This time may not be enough, if RPC is delayed too much
    // See seq.delayed.leaderchange.log
    std::this_thread::sleep_for(TimeEnsureNoElection());
    DisableNode(leader);
}

int One(int index, int support){
    uint64_t now = get_current_ms();
    while(get_current_ms() < now + 5000){
        RaftNode * leader = PickNode({RN::Leader});
    }
}

void print_state(){
    debug_test("%15s %12s %5s %9s %7s %7s %7s %6s %6s\n",
            "Name", "State", "Term", "log size", "commit", "lastapp", "peers", "run", "trans");
    for(auto nd: nodes){
        debug_test("%15s %12s %5llu %9u %7lld %7lld %7u %6s %6d\n", nd->name.c_str(), 
                RaftNode::node_state_name(nd->state), nd->current_term, 
                nd->logs.size(), nd->commit_index, nd->last_applied, nd->peers.size(),
                nd->is_running_unguard()?"T":"F", (!nd->trans_conf)?0:nd->trans_conf->state);
    }
}

void print_peers(RaftNode * ob){
    debug_test("Node %s's peers are %s\n", ob->name.c_str(), Nuke::join(ob->peers.begin(), ob->peers.end(), ";", [](auto & pr){
        return pr.second->name;
    }).c_str());
}