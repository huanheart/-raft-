#include "raftRpcUtil.h"

#include <mprpcchannel.h>
#include <mprpccontroller.h>

//MprpcChannel�Լ�MprpcController,������ͨ���̳йȸ�����й�������ģ���Ȼ��Ҳ��Ϊʲô���Խ��е��ùȸ��Լ����ɵ�
//������ʱ����Դ��������Զ���Ĳ���������������һ����̬
//��stub�������Լ�ѡ�����ģ�������������һ������Ȼ�����ǿ��Խ��е���֮ǰ��proto�ļ����Զ���ķ���(����ʵ�������ǰ��������ɵ�)

bool RaftRpcUtil::AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response) 
{
    MprpcController controller;
    stub_->AppendEntries(&controller, args, response, nullptr);
    return !controller.Failed();
}

bool RaftRpcUtil::InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args,
                                  raftRpcProctoc::InstallSnapshotResponse *response) 
{
    MprpcController controller;
    stub_->InstallSnapshot(&controller, args, response, nullptr);
    return !controller.Failed();
}

bool RaftRpcUtil::RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response) 
{
    MprpcController controller;
    stub_->RequestVote(&controller, args, response, nullptr);
    return !controller.Failed();
}


//�ڽ������ӵ�ʱ�򣬼��ܶණ������й��캯����ʼ��һЩ���������������������϶���Ҫ�ȿ����˵�ǰ���������������������ܽ������ӵ�
//��Ȼ���ǿ�����ʹ�����з�����ȫ��������Ȼ�������ӣ�Ҳ���Ը�һ��ʱ���������ӵ�ʱ�������ڿ������������������
RaftRpcUtil::RaftRpcUtil(std::string ip, short port) 
{
  //*********************************************  */
  //����rpc����
    stub_ = new raftRpcProctoc::raftRpc_Stub(new MprpcChannel(ip, port, true));
}

RaftRpcUtil::~RaftRpcUtil() 
{
    delete stub_; 
}








