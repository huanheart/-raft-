#include "clerk.h"

#include "raftServerRpcUtil.h"

#include "util.h"

#include <string>
#include <vector>

//�������������ȡkeyֵ����һ������Ҳ�൱����һ����־��
std::string Clerk::Get(std::string key)
{
    m_requestId++;
    auto requestId = m_requestId;
    int server = m_recentLeaderId;
    raftKVRpcProctoc::GetArgs args;
    args.set_key(key);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);

    while(true){
        raftKVRpcProctoc::GetReply reply;
        bool ok = m_servers[server]->Get(&args, &reply);
        if(!ok || reply.err()==ErrWrongLeader  ){
            server = (server + 1) % m_servers.size();
            continue;
        }
        if (reply.err() == ErrNoKey) {
            m_recentLeaderId=server;
            return "";
        }
        if(reply.err()==OK){
            m_recentLeaderId=server;
            return reply.value();
        }
    }
    return "";
}

//�����ʵ���ǿͻ��˺��쵼��ͨ�ŵĹ�����
void Clerk::PutAppend(std::string key, std::string value, std::string op)
{
    //��ǰ������+1
    m_requestId++;
    auto requestId = m_requestId;
    auto server = m_recentLeaderId;

    while(true){
        // ��Щ��������PutAppendArgs ���������
        raftKVRpcProctoc::PutAppendArgs args;
        args.set_key(key);
        args.set_value(value);
        args.set_op(op);
        args.set_clientid(m_clientId);
        args.set_requestid(requestId);
        raftKVRpcProctoc::PutAppendReply reply;
        bool ok = m_servers[server]->PutAppend(&args, &reply); //�����������.//�������޸ģ���������ݻ���׷�ӡ������������ݵķ�ʽ
        if (!ok || reply.err() == ErrWrongLeader) {
            DPrintf("��Clerk::PutAppend��ԭ��Ϊ��leader��{%d}����ʧ�ܣ�����leader{%d}����  ��������{%s}", server, server + 1,
                  op.c_str());
            if (!ok) {
                DPrintf("����ԭ�� ��rpcʧ�� ��");
            }
            if (reply.err() == ErrWrongLeader) {
                DPrintf("��ԇԭ�򣺷�leader");
            }
            server = (server + 1) % m_servers.size();  // try the next server
            continue;
        }
        if(reply.err()==OK){
            m_recentLeaderId=server;  //˵���ҵ����쵼�ߣ��ҷ�����Ϣ�ɹ���
            return ;
        }
    }

}


void Clerk::Put(std::string key,std::string value)
{
    PutAppend(key, value, "Put");
}

void Clerk::Append(std::string key,std::string value)
{
    PutAppend(key, value, "Append");
}

void Clerk::Init(std::string configFileName)
{
    //��ȡ���е�ip�Ͷ˿ڣ����������ӣ�ͨ��MprpcConfig�������ļ���д��Ŀո������ɾ�����Լ����仺�浽��Ӧ�Ĺ�ϣ������
    MprpcConfig config;
    config.LoadConfigFile(configFileName.c_str());
    std::vector<std::pair<std::string, short>> ipPortVt;
    for (int i = 0; i < INT_MAX - 1; ++i) {
        std::string node = "node" + std::to_string(i);

        std::string nodeIp = config.Load(node + "ip");
        std::string nodePortStr = config.Load(node + "port");
         //��������Ĭ��Ū��INT_MAX��������ʵ���ϲ���һ���ļ�������ô�����Ⱥ����������Ȼ����ȡ���ļ�û�������ʱ�򣬾�˵���Ѿ���ͷ��
        if (nodeIp.empty()) {   
            break;
        }
        ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str()));  //�]��atos���������Կ��]�Լ�ʵ��
    }

    //��ʼ����
    for(const auto&item : ipPortVt){
        std::string ip=item.first;
        short port =item.second;
        auto * rpc=new raftServerRpcUtil(ip, port); 
        //�������ӻ�������
        m_servers.push_back(std::shared_ptr<raftServerRpcUtil>(rpc) );
    }
}


Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_recentLeaderId(0) {}