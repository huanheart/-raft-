#include "raftServerRpcUtil.h"

//������raftKVRpcProctoc�ж�������Щ�����Լ������Լ������ռ�����֣���ô�Ϳ���ֱ��ʹ����Щ������

raftServerRpcUtil::raftServerRpcUtil(std::string ip,short port)
{
    //����rpc�����Լ�����rpc����
    stub=new raftKVRpcProctoc::kvServerRpc_Stub(new MprpcChannel(ip,port,false) ); //����������false����Ȼ��û�д������ӣ�ʹ���ӳ�����
    //�������ڸ���clerk��Ҫ���ӵķ�����ip���˿ں���Щ
}

raftServerRpcUtil::~raftServerRpcUtil() { delete stub; }

//���������ڻ�ȡ���ݵķ���
bool raftServerRpcUtil::Get(raftKVRpcProctoc::GetArgs * GetArgs ,raftKVRpcProctoc::GetReply * reply )
{
    MprpcController controller;
    stub->Get(&controller,GetArgs,reply,nullptr);
    return !controller.Failed();
}

//�������޸ģ���������ݻ���׷�ӡ������������ݵķ�ʽ
bool raftServerRpcUtil::PutAppend(raftKVRpcProctoc::PutAppendArgs * args,raftKVRpcProctoc::PutAppendReply * reply )
{
    MprpcController controller;
    stub->PutAppend(&controller, args, reply, nullptr);
    if (controller.Failed()) {
        std::cout << controller.ErrorText() << endl;
    }
     return !controller.Failed();
}
