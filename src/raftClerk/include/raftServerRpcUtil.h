#ifndef RAFTSERVERRPC_H
#define RAFTSERVERRPC_H

#include <iostream>
#include "kvServerRPC.pb.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "rpcprovider.h"

//ά����ǰ�ڵ������ĳһ����������rpcͨ�ţ��������������ڵ��rpc�ͷ���
//ͨ������raftKVRpcProctoc�����ռ��Լ����proto�ļ������������������Զ�������Լ�����
//Ȼ������ͨ����������һ����װ���õ�һ���µ���
class raftServerRpcUtil{

private:
    raftKVRpcProctoc::kvServerRpc_Stub* stub;
public:
    //������Ӧ��������
    bool Get(raftKVRpcProctoc::GetArgs* GetArgs, raftKVRpcProctoc::GetReply* reply);
    bool PutAppend(raftKVRpcProctoc::PutAppendArgs* args, raftKVRpcProctoc::PutAppendReply* reply);

    raftServerRpcUtil(std::string ip,short port);
    ~raftServerRpcUtil();
};

#endif