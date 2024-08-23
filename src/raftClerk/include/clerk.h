#ifndef SKIP_LIST_ON_RAFT_CLERK_H
#define SKIP_LIST_ON_RAFT_CLERK_H
#include <arpa/inet.h>
#include <netinet/in.h>
#include <raftServerRpcUtil.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <vector>
#include "kvServerRPC.pb.h"
#include "mprpcconfig.h"


class Clerk{
private:
    std::vector<std::shared_ptr<raftServerRpcUtil> > m_servers; //��������raft����fd��һ��ʼȫ����ʼ��Ϊ-1.��ʾû��������
    std::string m_clientId;
    int m_requestId;
    int m_recentLeaderId; //���ڼ�¼��һ��������쵼��˭����Ϊ�쵼������ʱ��仯����Ȼ��һ�����ڻ����쵼

    //��������ģ�Ψһ�Ŀͻ��˱�ʶ�����ͻ��ˣ�������1000��֮1�ĸ��ʣ�����ʹ��������������
    std::string Uuid(){
        return std::to_string(rand()) + std::to_string(rand()) + std::to_string(rand()) + std::to_string(rand());
    }
    void PutAppend(std::string key, std::string value, std::string op);

public:
    void Init(std::string configFileName);

    void Put(std::string key,std::string value);
    void Append(std::string key,std::string value);

    std::string Get(std::string key);

public:
    Clerk();

};

#endif 

