#ifndef SKIP_LIST_ON_RAFT_KVSERVER_H
#define SKIP_LIST_ON_RAFT_KVSERVER_H

// #include <boost/any.hpp>
// #include <boost/archive/binary_iarchive.hpp>
// #include <boost/archive/binary_oarchive.hpp>
// #include <boost/archive/text_iarchive.hpp>
// #include <boost/archive/text_oarchive.hpp>
// #include <boost/foreach.hpp>
// #include <boost/serialization/export.hpp>
// #include <boost/serialization/serialization.hpp>
// #include <boost/serialization/unordered_map.hpp>
// #include <boost/serialization/vector.hpp>
// #include <iostream>
// #include <mutex>
// #include <unordered_map>
// #include "../../raftRpcPro/include/kvServerRPC.pb.h"
// #include "raft.h"
// #include "../../skipList/include/skipList.h"


// #include <sys/stat.h>
// #include <sys/types.h>
#include <netinet/in.h>
#include <boost/any.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>



#include "kvServerRPC.pb.h"
#include "raft.h"
#include "skipList.h"

class KvServer : raftKVRpcProctoc::kvServerRpc
{

private:
    std::mutex m_mtx;
    int m_me;
    std::shared_ptr<Raft> m_raftNode;
    std::shared_ptr<LockQueue<ApplyMsg> > applyChan;  // kvServer��raft�ڵ��ͨ�Źܵ�
    int m_maxRaftState;                               // �����־��������ô������п���
    //���л��������
    std::string m_serializedKVData; 
    SkipList<std::string, std::string> m_skipList;
    //m_kvDB��֮ǰû��д�����ʱ���õĹ�ϣ��
    std::unordered_map<std::string, std::string> m_kvDB;
    std::unordered_map<int, LockQueue<Op> *> waitApplyCh;

    std::unordered_map<std::string, int> m_lastRequestId;  // clientid -> requestID  //һ��kV�������������Ӷ��client

    int m_lastSnapShotRaftLogIndex;
public:
    KvServer() = delete;

    KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port);

    void StartKVServer();

    void DprintfKVDB();

    void ExecuteAppendOpOnKVDB(Op op);
    //��Ҫ��ͨ��key��ȡ����ǰ��value
    void ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist);
    //��������
    void ExecutePutOpOnKVDB(Op op);

    void Get(const raftKVRpcProctoc::GetArgs *args,raftKVRpcProctoc::GetReply *reply);
    void GetCommandFromRaft(ApplyMsg message);
    //�Ƿ��ظ�����
    bool ifRequestDuplicate(std::string ClientId, int RequestId);

      // clerk ʹ��RPCԶ�̵���
    void PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply);

    ///һֱ�ȴ�raft������applyCh
    void ReadRaftApplyCommandLoop();

    void ReadSnapShotToInstall(std::string snapshot);

    bool SendMessageToWaitChan(const Op &op, int raftIndex);

     // ����Ƿ���Ҫ�������գ���Ҫ�Ļ�����raft֮����������
    void IfNeedToSendSnapShotCommand(int raftIndex, int proportion);
    //�������� kv.rf.applyCh �� SnapShot
    void GetSnapShotFromRaft(ApplyMsg message);
    std::string MakeSnapShot();
public:  // for rpc
    void PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                 ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) override;

    void Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
           ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done) override;

private:
    friend class boost::serialization::access;

    template <class Archive>
    void serialize(Archive &ar, const unsigned int version)  //������Ҫ���л��ͷ����л����ֶ�
    {
        ar &m_serializedKVData;

        // ar & m_kvDB;
        ar &m_lastRequestId;
    }
    //m_serializedKVData���￴��û�б����л���������ʵ����oa << *this;��������л���*this,�����л���m_serializedKVData�Լ�m_lastRequestId
    //�鿴serialize�������
    std::string getSnapshotData() 
    {
        m_serializedKVData = m_skipList.dump_file();
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        oa << *this;
        m_serializedKVData.clear();
        return ss.str();
    }
    //���н����Լ��洢
    void parseFromString(const std::string &str) 
    {
        std::stringstream ss(str);
        boost::archive::text_iarchive ia(ss);
        ia >> *this;
        m_skipList.load_file(m_serializedKVData);
        m_serializedKVData.clear();
    }


};

#endif