// Copyright 2008-2018 Yolo Technologies, Inc. All Rights Reserved. https://www.comblockengine.com

#ifndef KBE_REDIS_HELPER_H
#define KBE_REDIS_HELPER_H

#include "common.h"
#include "common/common.h"
#include "common/stringconv.h"
#include "common/memorystream.h"
#include "helper/debug_helper.h"
#include "db_interface_redis.h"

namespace KBEngine{ 

class RedisHelper
{
public:
	RedisHelper()
	{
	}

	virtual ~RedisHelper()
	{
	}

	static bool expireKey(DBInterfaceRedis* pdbi, const std::string& key, int secs, bool printlog = true)
	{
		if (!pdbi->query(printlog, "EXPIRE %s %d", key.c_str(), secs))
			return false;
	}
	
	static bool check_array_results(redisReply* pRedisReply)
	{
		for(size_t j = 0; j < pRedisReply->elements; ++j) 
		{
			if(pRedisReply->element[j]->type != REDIS_REPLY_INTEGER && 
				pRedisReply->element[j]->type != REDIS_REPLY_STRING)
			{
				return false;
			}
		}
		
		return true;
	}
	
	static bool hasTable(DBInterfaceRedis* pdbi, const std::string& name, bool printlog = true)
	{
		redisReply* pRedisReply = NULL;
		
		if (!pdbi->query(fmt::format("scan 0 MATCH {}", name), &pRedisReply, printlog))
			return false;
		
		size_t size = 0;
		
		if(pRedisReply)
		{
			if(pRedisReply->elements == 2 && pRedisReply->element[1]->type == REDIS_REPLY_ARRAY)
			{
				size = pRedisReply->element[1]->elements;
			}
			
			freeReplyObject(pRedisReply); 
		}
		
		return size > 0;
	}
	
	static bool dropTable(DBInterfaceRedis* pdbi, const std::string& tableName, bool printlog = true)
	{
		uint64 index = 0;
		
		while(true)
		{
			redisReply* pRedisReply = NULL;
			
			pdbi->query(fmt::format("scan {} MATCH {}", index, tableName), &pRedisReply, printlog);
			
			if(pRedisReply)
			{
				if(pRedisReply->elements == 2)
				{
					KBE_ASSERT(pRedisReply->element[0]->type == REDIS_REPLY_STRING);
					
					// 下一次由这个index开始
					StringConv::str2value(index, pRedisReply->element[0]->str);
					
					redisReply* r0 = pRedisReply->element[1];
					KBE_ASSERT(r0->type == REDIS_REPLY_ARRAY);
					
					for(size_t j = 0; j < r0->elements; ++j) 
					{
						redisReply* r1 = r0->element[j];
						KBE_ASSERT(r1->type == REDIS_REPLY_STRING);
							
						pdbi->query(fmt::format("del {}", r1->str), &pRedisReply, printlog);
					}
				}
				
				freeReplyObject(pRedisReply); 
			}
			else
			{
				return false;
			}
			
			if(index == 0)
				break;
		}
		
		return true;
	}
	
	static bool dropTableItem(DBInterfaceRedis* pdbi, const std::string& tableName, 
		const std::string& itemName, bool printlog = true)
	{
		uint64 index = 0;
		
		while(true)
		{
			redisReply* pRedisReply = NULL;
			
			pdbi->query(fmt::format("scan {} MATCH {}", index, tableName), &pRedisReply, printlog);
			
			if(pRedisReply)
			{
				if(pRedisReply->elements == 2)
				{
					KBE_ASSERT(pRedisReply->element[0]->type == REDIS_REPLY_STRING);
					
					// 下一次由这个index开始
					StringConv::str2value(index, pRedisReply->element[0]->str);
					
					redisReply* r0 = pRedisReply->element[1];
					KBE_ASSERT(r0->type == REDIS_REPLY_ARRAY);
					
					for(size_t j = 0; j < r0->elements; ++j) 
					{
						redisReply* r1 = r0->element[j];
						KBE_ASSERT(r1->type == REDIS_REPLY_STRING);

						pdbi->query(fmt::format("hdel {} {}", r1->str, itemName), &pRedisReply, printlog);
					}
				}
				
				freeReplyObject(pRedisReply); 
			}
			else
			{
				return false;
			}
			
			if(index == 0)
				break;
		}
		
		return true;
	}

	static bool writeEntity(DBInterfaceRedis* pdbi, redis::DBContext& context)
	{
		std::string mainKey = context.getMainKey();

		if (context.dbid > 0)
		{
			//已经有这个实体， 对此实体进行更新
			std::vector<std::string> formerKeys;

			redisReply* now = NULL;
		}
		else
		{
			//代表没有这个实体 直接写入
			redisReply* pIncReply = NULL;
			DBID entityDBID = 0;

			//原子+1 获取最新的DBID
			pdbi->query("INCR entityDBIDs", &pIncReply, true);

			if (pIncReply)
			{
				entityDBID = pIncReply->integer;

				if (entityDBID <= 0)
				{
					ERROR_MSG(fmt::format("error in incr entity dbid"));
					freeReplyObject(pIncReply);
					return false;
				}

				freeReplyObject(pIncReply);
			}

			redisReply* pInsertReply = NULL;
			mainKey = context.getMainKeyByDBID(entityDBID);

			for (int i = 0; i != context.items.size(); ++i)
			{
				KBEShared_ptr<redis::DBContext::DB_ITEM_DATA> pSotvs = context.items[i];
				pdbi->query(fmt::format("hset {} {} {}", mainKey, pSotvs->name, pSotvs->val), &pInsertReply, true);

				if (pInsertReply)
				{
					freeReplyObject(pInsertReply);
					pInsertReply = NULL;
				}
				else
				{
					ERROR_MSG("write entity main key error\n");
					return false;
				}
			}
		}

		return false;
	}
};

}
#endif // KBE_REDIS_HELPER_H
