#include "shardmaster_config.h"

int key2shard(std::string key)
{
	int shard = 0;
	if(key.size()>0)
	{
		shard = (int)(key[0]);
	}
	shard %= NShards;
	return shard;
}