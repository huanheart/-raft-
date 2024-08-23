#ifndef RAFT_H
#define RAFT_H

#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "ApplyMsg.h"
#include "Persister.h"
#include "boost/any.hpp"
#include "boost/serialization/serialization.hpp"
#include "config.h"
#include "monsoon.h"
#include "raftRpcUtil.h"
#include "util.h"

/// @brief //////////// ����״̬��ʾ  todo��������rpc��ɾ�����ֶΣ�ʵ�����������ò�����.
constexpr int Disconnected =
    0;  // �������������ʱ��debug�������쳣��ʱ��Ϊdisconnected��ֻҪ����������ΪAppNormal����ֹmatchIndex[]�����쳣��С
constexpr int AppNormal = 1;

///////////////ͶƱ״̬

constexpr int Killed = 0;
constexpr int Voted = 1;   //�����Ѿ�Ͷ��Ʊ��
constexpr int Expire = 2;  //ͶƱ����Ϣ����ѡ�ߣ�����
constexpr int Normal = 3;

class Raft : public raftRpcProctoc::raftRpc{
private:
    std::mutex m_mtx;
    std::vector<std::shared_ptr<RaftRpcUtil> > m_peers;
     std::shared_ptr<Persister> m_persister;
    int m_me;
    int m_currentTerm;
    int m_votedFor;
    std::vector<raftRpcProctoc::LogEntry> m_logs;  //������״̬��Ҫִ�е�ָ����Լ��յ��쵼ʱ�����ںţ�Ҳ��������ȷ���������Ƶģ�

    int m_commitIndex;
    int m_lastApplied;  // �Ѿ��㱨��״̬�����ϲ�Ӧ�ã���log ��index�����Ǻܶ�����������

    std::vector<int>  m_nextIndex;  // ������״̬���±�1��ʼ����Ϊͨ��commitIndex��lastApplied��0��ʼ��Ӧ����һ����Ч��index������±��1��ʼ
    std::vector<int>  m_matchIndex; //���������ĵ�1
    //��ʾ��ǰ���
    enum Status { 
        Follower, 
        Candidate,
         Leader 
    };
     Status m_status;
    //�����������֮�������Լ���ͨ�ŷ�ʽ�������ǿͻ�����raft��ͨ�ŷ�ʽ
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan;  // client������ȡ��־��2B����client��raftͨ�ŵĽӿ�
    
    // ѡ�ٳ�ʱ
    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime; 
    // ������ʱ������leader
    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime;

    // 2D�����ڴ�����յ�
    // �����˿����е����һ����־��Index��Term
    int m_lastSnapshotIncludeIndex;
    int m_lastSnapshotIncludeTerm;

    // Э�̿����Լ���װ��Э�̣�����ǵ�����
    std::unique_ptr<monsoon::IOManager> m_ioManager = nullptr;

public:
    void AppendEntries1(const raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *reply);
    void applierTicker();
    bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot);
    void doElection();

    //brief ����������ֻ��leader����Ҫ��������
    void doHeartBeat();

    // ÿ��һ��ʱ����˯��ʱ������û�����ö�ʱ����û����˵����ʱ��(ԭ�������ǰ���û���յ��쵼�ߵ���������û�б��������ã���ô������Ϊ�쵼��ʧЧ��
    //���Կ��ܻᷢ��ͶƱѡ����
    // ����������ú���˯��ʱ�䣺˯�ߵ�����ʱ��+��ʱʱ�䣨����ʱ����������ã�
    void electionTimeOutTicker();
    std::vector<ApplyMsg> getApplyLogs();
    int getNewCommandIndex();
    void getPrevLogInfo(int server, int *preIndex, int *preTerm);
    void GetState(int *term, bool *isLeader);
    void InstallSnapshot(const raftRpcProctoc::InstallSnapshotRequest *args,
                           raftRpcProctoc::InstallSnapshotResponse *reply);
    void leaderHearBeatTicker();
    void leaderSendSnapShot(int server);
    void leaderUpdateCommitIndex();
    bool matchLog(int logIndex, int logTerm);
    void persist();
    void RequestVote(const raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *reply);
    bool UpToDate(int index, int term);
    int getLastLogIndex();
    int getLastLogTerm();
    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm);
    int getLogTermFromLogIndex(int logIndex);
    int GetRaftStateSize();
    int getSlicesIndexFromLogIndex(int logIndex);

    bool sendRequestVote(int server, std::shared_ptr<raftRpcProctoc::RequestVoteArgs> args,
                        std::shared_ptr<raftRpcProctoc::RequestVoteReply> reply, std::shared_ptr<int> votedNum);
    bool sendAppendEntries(int server, std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> args,
                             std::shared_ptr<raftRpcProctoc::AppendEntriesReply> reply, std::shared_ptr<int> appendNums);
    //���ﲢû��������ִ������ˣ���ֹ������ʱ��̫����Ӱ������
    //������뵽kvserver����
    void pushMsgToKvServer(ApplyMsg msg);
    void readPersist(std::string data);
    std::string persistData();

    void Start(Op command, int *newLogIndex, int *newLogTerm, bool *isLeader);

    //�����������ҪĿ���ǽ��Ѿ������ڿ����е���־��Ŀ�����������µĿ��������滻��ǰ״̬��
    //ͬʱ����������¿����±꣨��index������ʾϵͳ�Ѿ�Ӧ�����������֮ǰ��������־��Ŀ��
    void Snapshot(int index, std::string snapshot);
public:
      // ��д���෽��,��ΪrpcԶ�̵����������õ����������
    //���л��������л��Ȳ���rpc��ܶ��Ѿ������ˣ��������ֻ��Ҫ��ȡֵȻ���������ñ��ط������ɡ�
    //������������ȵ���AppendEntries1�������
    void AppendEntries(google::protobuf::RpcController *controller, const ::raftRpcProctoc::AppendEntriesArgs *request,
                         ::raftRpcProctoc::AppendEntriesReply *response, ::google::protobuf::Closure *done) override;
    void InstallSnapshot(google::protobuf::RpcController *controller,
                           const ::raftRpcProctoc::InstallSnapshotRequest *request,
                           ::raftRpcProctoc::InstallSnapshotResponse *response, ::google::protobuf::Closure *done) override;
    void RequestVote(google::protobuf::RpcController *controller, const ::raftRpcProctoc::RequestVoteArgs *request,
                       ::raftRpcProctoc::RequestVoteReply *response, ::google::protobuf::Closure *done) override;

public:
    void init(std::vector<std::shared_ptr<RaftRpcUtil>> peers, int me, std::shared_ptr<Persister> persister,
            std::shared_ptr<LockQueue<ApplyMsg>> applyCh);

private:
    //���persist���֣��е㲻��Ϊʲô��ô��ĳ�Ա��raft�����������ͬ
class BoostPersistRaftNode {
    public:
        friend class boost::serialization::access;
        //serializeģ�庯�����ú�����Boost.Serialization���һ������
        //�����ڲ�����&������ʹ�����л�������ֻ�õ���&�Ϳ���ֱ��ʹ����Щ�����л��ˣ�
        template <class Archive>
        void serialize(Archive &ar, const unsigned int version) { 
            ar &m_currentTerm;
            ar &m_votedFor;
            ar &m_lastSnapshotIncludeIndex;
            ar &m_lastSnapshotIncludeTerm;
            ar &m_logs;
        }
        int m_currentTerm;
        int m_votedFor;
        int m_lastSnapshotIncludeIndex;
        int m_lastSnapshotIncludeTerm;
        std::vector<std::string> m_logs;
        std::unordered_map<std::string, int> umap;
   public:
  };

};


#endif  // RAFT_H