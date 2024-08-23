#include "raft.h"
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <memory>
#include "config.h"
#include "util.h"

int Raft::GetRaftStateSize() 
{
    return m_persister->RaftStateSize(); 
}


//��Ϊ���շ��ķ���
void Raft::AppendEntries1(const raftRpcProctoc::AppendEntriesArgs* args, raftRpcProctoc::AppendEntriesReply* reply){
    std::lock_guard<std::mutex> locker(m_mtx);
    reply->set_appstate(AppNormal); //���õ�ǰ������������Ϊ���Ե��ﵱǰ���
    //����쵼�ߵ�����С��׷����
    if (args->term() < m_currentTerm) {
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(-100);  // ���-100���øо�ûɶ�ã���Ϊ���ͷ����߼����쵼�ߵ����ڱȵ�ǰ���ҪС��ʱ��ֻ����µ�ǰ�����Լ����׷����
        DPrintf("[func-AppendEntries-rf{%d}] �ܾ��� ��ΪLeader{%d}��term{%v}< rf{%d}.term{%d}\n", m_me, args->leaderid(),
                args->term(), m_me, m_currentTerm);
        return;  // ע��ӹ��ڵ��쵼���յ���Ϣ��Ҫ���趨ʱ��
  }
    //���������̫��⣺Ϊ����Ҫ�ӳٵ���persist��������
    DEFER{
        persist();
    };
    //���Լ������ڸ���
    if(args->term() > m_currentTerm){
        m_status=Follower;
        m_currentTerm=args->term();
        m_votedFor=-1;      //����-1��������ģ���ΪͻȻ���Ҳ�ǿ���ͶƱ��
    }
     myAssert(args->term() == m_currentTerm, format("assert {args.Term == rf.currentTerm} fail"));
    //������������������ô��ѡ�߿��ܻ��յ�ͬһ��term��leader����Ϣ��Ҫת����Ϊfollower(�鿴�ĵ�2)
    m_status=Follower;
    m_lastResetElectionTime=now();

    //�Ƚ���־����������ͬ��ʱ����Ҫ�Ƚ���־��
    if(args->prevlogindex() >getLastLogIndex() ){
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(getLastLogIndex() + 1);
        return;
    }else if(args->prevlogindex() < m_lastSnapshotIncludeIndex){
        reply->set_success(false);
        reply->set_term(m_currentTerm);
        reply->set_updatenextindex(m_lastSnapshotIncludeIndex+1);
    }
    //if���ֿ�ʼ�������жϵ�ǰ
    if (matchLog(args->prevlogindex(), args->prevlogterm())){
        //forѭ����ʼ
        for(int i=0;i<args->entries_size();i++){
            auto log=args->entries(i);
            if(log.logindex() > getLastLogIndex()){
                //�����������־��һ����˵�����ǻ���������
                m_logs.push_back(log);
            }else{
                //û�����ͱȽ��Ƿ�ƥ�䣬��ƥ���ٸ��£�������ֱ�ӽض�
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() == log.logterm() &&
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command() != log.command()) {
                        //��ͬλ�õ�log ����logTerm��ȣ���������ȴ����ͬ��������raft��ǰ��ƥ�䣬�쳣�ˣ�
                        myAssert(false, format("[func-AppendEntries-rf{%d}] ���ڵ�logIndex{%d}��term{%d}��ͬ��������command{%d:%d}   "
                                        " {%d:%d}ȴ��ͬ����\n",
                                         m_me, log.logindex(), log.logterm(), m_me,
                                        m_logs[getSlicesIndexFromLogIndex(log.logindex())].command(), args->leaderid(),
                                        log.command()));
                }
                //��ƥ��͸���
                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) {
                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log ;
                }
        }
    } 
    //forѭ������
    myAssert(getLastLogIndex() >= args->prevlogindex() + args->entries_size(),
                format("[func-AppendEntries1-rf{%d}]rf.getLastLogIndex(){%d} != args.PrevLogIndex{%d}+len(args.Entries){%d}",
                m_me, getLastLogIndex(), args->prevlogindex(), args->entries_size()));
    // m_commitIndex��ʾҪ���µ�״̬������������:���Ӧ�ú�defer�ĺ����й��ˣ�����ΪʲôȡС���Լ�Ϊʲô�쵼���ύ��־����ҪС��
    //׷�������ύ��״̬������־�����ĵ�2
    if(args->leadercommit() > m_commitIndex){
         m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());
    }
    myAssert(getLastLogIndex() >= m_commitIndex,
                 format("[func-AppendEntries1-rf{%d}]  rf.getLastLogIndex{%d} < rf.commitIndex{%d}", m_me,
                        getLastLogIndex(), m_commitIndex));
    reply->set_success(true);
    reply->set_term(m_currentTerm);
    return ;
    }
    //���뵽else���֣�
    // PrevLogIndex ���Ⱥ��ʣ����ǲ�ƥ�䣬�����ǰѰ�� ì�ܵ�term�ĵ�һ��Ԫ��
    // Ϊʲô��term����־����ì�ܵ��أ�Ҳ��һ������ì�ܵģ�ֻ����ô�Ż�����rpc����
    // ��ʲôʱ��term��ì���أ��ܶ����������leader��������־֮�����Ͼͱ����ȵ�
    reply->set_updatenextindex(args->prevlogindex());

    for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {
        if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {
            reply->set_updatenextindex(index + 1);
            break;
        }
    }
    reply->set_success(false);
    reply->set_term(m_currentTerm);
    return ;
}

//��Ϊ���ͷ��Ĵ��룺
bool Raft::sendAppendEntries(int server, std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> args,
                             std::shared_ptr<raftRpcProctoc::AppendEntriesReply> reply,
                             std::shared_ptr<int> appendNums)
{
    DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader ��ڵ�{%d}����AE rpc�_ʼ �� args->entries_size():{%d}", m_me,
          server, args->entries_size());
    //��ǰserver���Ͷ�Ӧ��Ϣ
    //���ok�������Ƿ�����ͨ�ŵ�ok��������requestVote rpc�Ƿ�ͶƱ��rpc
    //ͨ������raftRpcUtil���������ķ������������ǵ���raft�������AppendEntries->AppendEntries1����������������
    //��ǰ�Ŀ�������ά����raftRpcUtil������ģ�����ϵĲ���
   
    bool ok = m_peers[server]->AppendEntries(args.get(), reply.get());
    //�������������
    //���͵�ʱ���������ģ���Ȼ�ߵ���һ������������ϣ����ҽ�����Ӧ���
    if(!ok){
        DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader ��ڵ�{%d}����AE rpcʧ��", m_me, server);
        return ok;
    }
    DPrintf("[func-Raft::sendAppendEntries-raft{%d}] leader ��ڵ�{%d}����AE rpc�ɹ�", m_me, server);
    if (reply->appstate() == Disconnected) { //appstate��ʶ�������״̬��
        return ok;
    }

    std::lock_guard<std::mutex> lg1(m_mtx);

    //��reply���д���
    // ����rpcͨ�ţ�����ʲôʱ��Ҫ���term
    if(reply->term() > m_currentTerm ){
        m_status=Follower;
        m_currentTerm=reply->term();   
        //���µ�ǰ׷���ߵ����ڣ���������������쵼�߱��ֵ�������㲿�֣���ô������㲿�ֻ�ֵ�����һ���ط�
        //���ʱ������������ѡ�٣�ʹ�õ�ǰ�쵼������ҪС������һ���쵼�ߣ���Ȼ��������
        m_votedFor=-1;
        return ok;
    }else if (reply->term() < m_currentTerm) {
        DPrintf("[func -sendAppendEntries  rf{%d}]  �ڵ㣺{%d}��term{%d}<rf{%d}��term{%d}\n", m_me, server, reply->term(),
                m_me, m_currentTerm);
        return ok;
    }

    if (m_status != Leader) {
        //�������leader����ô�Ͳ�Ҫ�Է��ص�������д�����
        //��Ϊ��ѭ�����õģ���Ȼ������if(reply->term() > m_currentTerm)��ʱ�򣬻�return�������ھͲ����쵼���ˣ���ѭ��Ҫ������Ϣ����һ�ζ���
        //��ʱ�򣬷����������쵼���ˣ��Ͳ��������洦����,��ʱ����뵽���շ��� if (args->term() < m_currentTerm)���֣�ֱ�ӽ���return false
        return ok;
    }

    //˵������term��ͬ
    myAssert(reply->term() == m_currentTerm,
           format("reply.Term{%d} != rf.currentTerm{%d}   ", reply->term(), m_currentTerm));
    //˵��������ͬ������־��Ӧ���ڲ�ƥ��������matchLog����false�����
    if (!reply->success()) {
        //���Ӧ����Զ����Ϊ-100�ɣ�������ͬ���ҿ���Ӧ����û������-100��ʱ��
        if(reply->updatenextindex() != -100){
            DPrintf("[func -sendAppendEntries  rf{%d}]  ���ص���־term��ȣ����ǲ�ƥ�䣬����nextIndex[%d]��{%d}\n", m_me,
                    server, reply->updatenextindex());
            m_nextIndex[server] = reply->updatenextindex();  //ʧ���ǲ�����mathIndex��
        }

    }else{
        //��ʾ�ɹ���׷����+1�������鿴�Ƿ�������ɹ��ı�־
        *appendNums = *appendNums + 1;
        DPrintf("---------------------------tmp------------------------- ���c{%d}����true,��ǰ*appendNums{%d}", server,
                *appendNums);
        m_matchIndex[server] = std::max(m_matchIndex[server], args->prevlogindex() + args->entries_size()); //������忴�ĵ�2
         m_nextIndex[server] = m_matchIndex[server] + 1; //��һ�θ��µ�����+1
        int lastLogIndex = getLastLogIndex();
        //���Խ�������������£��������κ���������ȴ����ʱ�򣬻��ǵ��ڵģ�������ʹ��m_nextIndex+1,��Ȼ����lastLogIndex+1Ҳ�����
        //��׷���ߺ��쵼�ߵ���һ�θ�����־������ͬ
        myAssert(m_nextIndex[server] <= lastLogIndex + 1,
            format("error msg:rf.nextIndex[%d] > lastLogIndex+1, len(rf.logs) = %d   lastLogIndex{%d} = %d", server,
                m_logs.size(), server, lastLogIndex));
        //˵������ֱ���ύ��
        if (*appendNums >= 1 + m_peers.size() / 2) {
            *appendNums = 0;
            //�����־
            if (args->entries_size() > 0) {
                DPrintf("args->entries(args->entries_size()-1).logterm(){%d}   m_currentTerm{%d}",
                    args->entries(args->entries_size() - 1).logterm(), m_currentTerm);
            }
            //�����ǰ���͵���־>0���ҵ�ǰ��־�����ڸ�m_currentTerm��ͬ
            if (args->entries_size() > 0 && args->entries(args->entries_size() - 1).logterm() == m_currentTerm) {
                DPrintf(
                "---------------------------tmp------------------------- ��ǰterm��log�ɹ��ύ������leader��m_commitIndex "
                "from{%d} to{%d}",
                m_commitIndex, args->prevlogindex() + args->entries_size());
                //����m_commitIndex��m_commitIndex��ʾ��Щ��־���ύ��
                m_commitIndex = std::max(m_commitIndex, args->prevlogindex() + args->entries_size());
            }
            myAssert(m_commitIndex <= lastLogIndex,
                format("[func-sendAppendEntries,rf{%d}] lastLogIndex:%d  rf.commitIndex:%d\n", m_me, lastLogIndex,
                        m_commitIndex));
        }
    }
    return ok;
}


void Raft::getLastLogIndexAndTerm(int * lastLogIndex,int *lastLogTerm){

    if(m_logs.empty()){
        *lastLogIndex = m_lastSnapshotIncludeIndex;
        *lastLogTerm = m_lastSnapshotIncludeTerm;
    }else {
        *lastLogIndex = m_logs[m_logs.size() - 1].logindex();
        *lastLogTerm = m_logs[m_logs.size() - 1].logterm();
    }

}


//����ڶ������������ģ�ֻ���ĵ�ǰ����־��index
int Raft::getLastLogIndex(){
    int lastLogIndex =-1;
    int _=-1;
    getLastLogIndexAndTerm(&lastLogIndex,&_);
    return lastLogIndex;
}
//������ͬ��
int Raft::getLastLogTerm(){

    int _=-1;
    int lastLogTerm=-1;
    getLastLogIndexAndTerm(&_,&lastLogTerm);
    return lastLogTerm;
}

//��д���෽�����֣�
void Raft::AppendEntries(google::protobuf::RpcController* controller,
                         const ::raftRpcProctoc::AppendEntriesArgs* request,
                         ::raftRpcProctoc::AppendEntriesReply* response, ::google::protobuf::Closure* done) {
  AppendEntries1(request, response);
  done->Run();
}

void Raft::InstallSnapshot(google::protobuf::RpcController* controller,
                           const ::raftRpcProctoc::InstallSnapshotRequest* request,
                           ::raftRpcProctoc::InstallSnapshotResponse* response, ::google::protobuf::Closure* done) {
  InstallSnapshot(request, response);

  done->Run();
}

void Raft::RequestVote(google::protobuf::RpcController* controller, const ::raftRpcProctoc::RequestVoteArgs* request,
                       ::raftRpcProctoc::RequestVoteReply* response, ::google::protobuf::Closure* done) {
  RequestVote(request, response);
  done->Run();
}
////��д���෽����������

//��ȡ��ǰ��־�±�����Ӧ����������λ��
int Raft::getSlicesIndexFromLogIndex(int logIndex)
{
    myAssert(logIndex > m_lastSnapshotIncludeIndex,
           format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} <= rf.lastSnapshotIncludeIndex{%d}", m_me,
                  logIndex, m_lastSnapshotIncludeIndex));
    int lastLogIndex=getLastLogIndex();
     myAssert(logIndex <= lastLogIndex, format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}",
                                            m_me, logIndex, lastLogIndex));
    int SliceIndex=logIndex - m_lastSnapshotIncludeIndex-1;
    return SliceIndex;
}

int Raft::getLogTermFromLogIndex(int logIndex)
{
    myAssert(logIndex >= m_lastSnapshotIncludeIndex,
        format("[func-getSlicesIndexFromLogIndex-rf{%d}]  index{%d} < rf.lastSnapshotIncludeIndex{%d}", m_me,
                logIndex, m_lastSnapshotIncludeIndex));
    int lastLogIndex=getLastLogIndex();
    myAssert(logIndex <= lastLogIndex, format("[func-getSlicesIndexFromLogIndex-rf{%d}]  logIndex{%d} > lastLogIndex{%d}",
                                        m_me, logIndex, lastLogIndex));
    //�����ǰ����־���ǿ��յ����һ�����֣���ô�Ͳ��úķѺ���ȥ���ң�ֱ�ӷ�����
    //��ȡ����־�±�����Ӧ��������������
    if(logIndex == m_lastSnapshotIncludeIndex){
        return m_lastSnapshotIncludeTerm;
    }
    return m_logs[getSlicesIndexFromLogIndex(logIndex)].logterm();
}

bool Raft::matchLog(int logIndex,int logTerm){
    myAssert(logIndex >= m_lastSnapshotIncludeIndex && logIndex <= getLastLogIndex(),
        format("�����㣺logIndex{%d}>=rf.lastSnapshotIncludeIndex{%d}&&logIndex{%d}<=rf.getLastLogIndex{%d}",
                logIndex, m_lastSnapshotIncludeIndex, logIndex, getLastLogIndex()));
    //�鿴��ǰ��־����Ӧ�������Ƿ���ͬ
    return logTerm==getLogTermFromLogIndex(logIndex);
}

void Raft::doHeartBeat()
{
    std::lock_guard<std::mutex> g(m_mtx);
    if(m_status != Leader)
        return ;
    DPrintf("[func-Raft::doHeartBeat()-Leader: {%d}] Leader��������ʱ�����������õ�mutex����ʼ����AE\n", m_me);
    auto appendNums = std::make_shared<int>(1);  //��ȷ���صĽڵ������,��һ��ʼ��ʾ�Ѿ��������Լ�
    for(int i=0;i<m_peers.size();i++){
        if(i==m_me)
            continue;
        DPrintf("[func-Raft::doHeartBeat()-Leader: {%d}] Leader��������ʱ�������� index:{%d}\n", m_me, i);
        myAssert(m_nextIndex[i] >= 1, format("rf.nextIndex[%d] = {%d}", i, m_nextIndex[i]));
        //�ж��Ƿ���Ҫ���Ϳ��գ����׷���ߵĲ���С���쵼�ߵĿ��ղ��֣���ô�ͷ��Ϳ��գ���Ϊ֮ǰ����־�Ѿ���ɾ����
        if(m_nextIndex[i]<=m_lastSnapshotIncludeIndex){
            std::thread t(&Raft::leaderSendSnapShot, this, i);  // �������̲߳�ִ��b�����������ݲ���
            t.detach();
            continue;
        }
        //������־��
        int preLogIndex = -1;
        int PrevLogTerm = -1;
        //��ȡ��ǰ�쵼��Ӧ�ô���һ��λ�ÿ�ʼ����Ӧ����־����ĳһ��׷����
        getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);
        std::shared_ptr<raftRpcProctoc::AppendEntriesArgs> appendEntriesArgs =
        std::make_shared<raftRpcProctoc::AppendEntriesArgs>();
        appendEntriesArgs->set_term(m_currentTerm);
        appendEntriesArgs->set_leaderid(m_me);
        appendEntriesArgs->set_prevlogindex(preLogIndex);
        appendEntriesArgs->set_prevlogterm(PrevLogTerm);
        appendEntriesArgs->clear_entries();
        appendEntriesArgs->set_leadercommit(m_commitIndex);
        if (preLogIndex != m_lastSnapshotIncludeIndex) {
            for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {
                raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                *sendEntryPtr = m_logs[j];  //=�ǿ��Ե��ȥ�ģ����Ե��ȥ����protobuf�����д�����
            }
        } else {
            for (const auto& item : m_logs) {
                raftRpcProctoc::LogEntry* sendEntryPtr = appendEntriesArgs->add_entries();
                *sendEntryPtr = item;  //=�ǿ��Ե��ȥ�ģ����Ե��ȥ����protobuf�����д�����
            }
        }
        int lastLogIndex = getLastLogIndex();
        // leader��ÿ���ڵ㷢�͵���־���̲�һ�����Ƕ���֤��prevIndex����ֱ�����
        myAssert(appendEntriesArgs->prevlogindex() + appendEntriesArgs->entries_size() == lastLogIndex,
                   format("appendEntriesArgs.PrevLogIndex{%d}+len(appendEntriesArgs.Entries){%d} != lastLogIndex{%d}",
                          appendEntriesArgs->prevlogindex(), appendEntriesArgs->entries_size(), lastLogIndex));
        //���췵��ֵ��
        const std::shared_ptr<raftRpcProctoc::AppendEntriesReply> appendEntriesReply =
        std::make_shared<raftRpcProctoc::AppendEntriesReply>();
        appendEntriesReply->set_appstate(Disconnected);
        std::thread t(&Raft::sendAppendEntries, this, i, appendEntriesArgs, appendEntriesReply,
                        appendNums);  // �������̲߳�ִ��b�����������ݲ���
        t.detach();
    }
    m_lastResetHearBeatTime = now();  //��������ʱ��
}

void Raft::getPrevLogInfo(int server, int* preIndex, int* preTerm)
{
    //m_lastSnapshotIncludeIndex���һ�����յ��±���ʵ������־�ĵ�һ���±�֮��
    //ͨ��ǰ���жϿ��Լ���getSlicesIndexFromLogIndex�ĺ������ã���ʵֱ�������Ǹ�Ҳû����
    if (m_nextIndex[server] == m_lastSnapshotIncludeIndex + 1) {
        //Ҫ���͵���־�ǵ�һ����־�����ֱ�ӷ���m_lastSnapshotIncludeIndex��m_lastSnapshotIncludeTerm
        *preIndex = m_lastSnapshotIncludeIndex;
        *preTerm = m_lastSnapshotIncludeTerm;
        return;
    }
    auto nextIndex = m_nextIndex[server];
    *preIndex = nextIndex - 1;
    *preTerm = m_logs[getSlicesIndexFromLogIndex(*preIndex)].logterm();
}


void Raft::InstallSnapshot(const raftRpcProctoc::InstallSnapshotRequest* args,
                           raftRpcProctoc::InstallSnapshotResponse* reply) 
{
    m_mtx.lock();
    DEFER { m_mtx.unlock(); };
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        return;
    }
    if (args->term() > m_currentTerm) {
        //�������������Ҫ������־
        m_currentTerm = args->term();
        m_votedFor = -1;
        m_status = Follower;
        persist();
    }
    m_status = Follower;
    m_lastResetElectionTime = now();
    //����֮ǰ�Ѿ�ͬ����������
    if (args->lastsnapshotincludeindex() <= m_lastSnapshotIncludeIndex) {
        return;
    }
    //�������ڱ�������ˣ���Ȼ��Ҫ�Կ��հ����ķ�Χ�ڵ���־����һ�����
    auto lastLogIndex = getLastLogIndex();
    if (lastLogIndex > args->lastsnapshotincludeindex()) {
        m_logs.erase(m_logs.begin(), m_logs.begin() + getSlicesIndexFromLogIndex(args->lastsnapshotincludeindex()) + 1);
    } else {
        m_logs.clear();
    }
    m_commitIndex = std::max(m_commitIndex, args->lastsnapshotincludeindex());
    m_lastApplied = std::max(m_lastApplied, args->lastsnapshotincludeindex());
    m_lastSnapshotIncludeIndex = args->lastsnapshotincludeindex();
    m_lastSnapshotIncludeTerm = args->lastsnapshotincludeterm();
    reply->set_term(m_currentTerm);
    //���ﻹ��Ҫ��⣺
    ApplyMsg msg;
    msg.SnapshotValid = true;
    msg.Snapshot = args->data();
    msg.SnapshotTerm = args->lastsnapshotincludeterm();
    msg.SnapshotIndex = args->lastsnapshotincludeindex();
    applyChan->Push(msg);     //��ɾ�����о���Ӧ��ִ������
    std::thread t(&Raft::pushMsgToKvServer, this, msg);  // �������̲߳�ִ��b�����������ݲ���
    t.detach();
    m_persister->Save(persistData(), args->data());

}

void Raft::pushMsgToKvServer(ApplyMsg msg) 
{
    applyChan->Push(msg); 
}

void Raft::leaderSendSnapShot(int server)
{
    m_mtx.lock();
    raftRpcProctoc::InstallSnapshotRequest args;
    args.set_leaderid(m_me);
    args.set_term(m_currentTerm);
    args.set_lastsnapshotincludeindex(m_lastSnapshotIncludeIndex);
    args.set_lastsnapshotincludeterm(m_lastSnapshotIncludeTerm);
    //���ݼ����������persister�ļ�����
    args.set_data(m_persister->ReadSnapshot());
    raftRpcProctoc::InstallSnapshotResponse reply;
    m_mtx.unlock();
    //��ʼ����
    bool ok = m_peers[server]->InstallSnapshot(&args, &reply);
    m_mtx.lock();
    DEFER { m_mtx.unlock(); };
    if (!ok) {
        return;
    }

    if (m_status != Leader || m_currentTerm != args.term()) {
        return;  //�м��ͷŹ���������״̬�Ѿ��ı��ˣ��ر�ϸ�ڵ�һ���ط���m_peers[server]->InstallSnapshot(&args, &reply);�������������
        //������Ϊ��׷����
    }
    if (reply.term() > m_currentTerm) {
        //����
        m_currentTerm = reply.term();
        m_votedFor = -1;
        m_status = Follower;
        persist();   //�������߼��ÿ�����ļ���
        m_lastResetElectionTime = now();
        return;
    }
    // m_matchIndex��ʾ��ǰ׷���߸��µ���־������������ǰ�쵼��ͬ����������
    //m_next��ʾ�´�ͬ��������
    m_matchIndex[server] = args.lastsnapshotincludeindex();
    m_nextIndex[server] = m_matchIndex[server] + 1;
}

bool Raft::CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot) 
{
    return true;
}

void Raft::doElection() 
{
    std::lock_guard<std::mutex> g(m_mtx);
    if (m_status == Leader)
        return ;
    DPrintf("[       ticker-func-rf(%d)              ]  ѡ�ٶ�ʱ�������Ҳ���leader����ʼѡ�� \n", m_me);
    m_status = Candidate;
        ///��ʼ��һ�ֵ�ѡ��
    m_currentTerm += 1;
    m_votedFor = m_me;  //�����Լ����Լ�Ͷ��Ҳ����candidate��ͬ����candidateͶ
    persist();
    std::shared_ptr<int> votedNum = std::make_shared<int>(1) ;
    m_lastResetElectionTime = now();
    for (int i = 0; i < m_peers.size(); i++) {
        if (i == m_me) {
            continue;
        }
        //���͵�ʱ��ҲҪЯ���Լ��������־����Ϊ��������ͬ��ʱ������־���бȽϣ��鿴�Ƿ�ͶƱ
        int lastLogIndex = -1, lastLogTerm = -1;
        getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);  //��ȡ���һ��log��term���±�
        std::shared_ptr<raftRpcProctoc::RequestVoteArgs> requestVoteArgs =
        std::make_shared<raftRpcProctoc::RequestVoteArgs>();
        requestVoteArgs->set_term(m_currentTerm);
        requestVoteArgs->set_candidateid(m_me);
        requestVoteArgs->set_lastlogindex(lastLogIndex);
        requestVoteArgs->set_lastlogterm(lastLogTerm);
        auto requestVoteReply = std::make_shared<raftRpcProctoc::RequestVoteReply>();

        std::thread t(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply,
                        votedNum);  // �������̲߳�ִ��b�����������ݲ���
        t.detach();
    }

}

bool Raft::sendRequestVote(int server, std::shared_ptr<raftRpcProctoc::RequestVoteArgs> args,
                           std::shared_ptr<raftRpcProctoc::RequestVoteReply> reply, std::shared_ptr<int> votedNum)
{
    auto start = now();
    DPrintf("[func-sendRequestVote rf{%d}] ��server{%d} �l�� RequestVote �_ʼ", m_me, m_currentTerm, getLastLogIndex());
    bool ok = m_peers[server]->RequestVote(args.get(), reply.get());
    DPrintf("[func-sendRequestVote rf{%d}] ��server{%d} �l�� RequestVote �ꮅ���ĕr:{%d} ms", m_me, m_currentTerm,
            getLastLogIndex(), now() - start);

    if (!ok) {
        return ok;  //��������Ļ����������崻�����������
    }
    ///�Ի�Ӧ���д���Ҫ�ǵ�����ʲôʱ���յ��ظ���Ҫ���term
    //  std::lock_guard<std::mutex> lg(m_mtx);
    if (reply->term() > m_currentTerm) {
        m_status = Follower;  //���䣺��ݣ�term����ͶƱ
        m_currentTerm = reply->term();
        m_votedFor = -1;
        persist();
        return true;
    } else if (reply->term() < m_currentTerm) {
        return true;
    }
    myAssert(reply->term() == m_currentTerm, format("assert {reply.Term==rf.currentTerm} fail"));
    //���ͶƱ����ͨ��
    if (!reply->votegranted()) {
        return true;
    }
    *votedNum = *votedNum + 1;
    if (*votedNum < m_peers.size() / 2 + 1)
        return true;
    //��Ҫ����쵼����
    *votedNum = 0;
    if (m_status == Leader) {
      myAssert(false,
               format("[func-sendRequestVote-rf{%d}]  term:{%d} ͬһ��term�������쵼��error", m_me, m_currentTerm));
    }
    m_status = Leader;
    DPrintf("[func-sendRequestVote rf{%d}] elect success  ,current term:{%d} ,lastLogIndex:{%d}\n", m_me, m_currentTerm,
            getLastLogIndex());
    int lastLogIndex = getLastLogIndex();
    for (int i = 0; i < m_nextIndex.size(); i++) {
        m_nextIndex[i] = lastLogIndex + 1;  //��������Ϊ��һ����Ҫ���ӵ���־�±꣬����Ҳû��ϵ�������
        m_matchIndex[i] = 0;                
        //ÿ��һ���쵼���Ǵ�0��ʼ����ʾ׷����һ�����쵼����ͬ����־���֣��������ﲻ֪�����쵼�ߺ�׷������־��Щ��ͬ
        //����Ȼ����Ϊ0
    }
    std::thread t(&Raft::doHeartBeat, this);  //�����������ڵ������Լ�����leader
    t.detach();

    persist();
    return true;
}

void Raft::Start(Op command, int* newLogIndex, int* newLogTerm, bool* isLeader)
{
    std::lock_guard<std::mutex> lg1(m_mtx);
    //ֻ���쵼�߿�������
    if (m_status != Leader) {
        DPrintf("[func-Start-rf{%d}]  is not leader");
        *newLogIndex = -1;
        *newLogTerm = -1;
        *isLeader = false;
        return;
    }
    raftRpcProctoc::LogEntry newLogEntry;
    //������һ���Զ��庯����ͨ�����ڽ�ĳ�����ݽṹ������ bytes�����������ݡ����ض��ࣩת��Ϊ�ַ���
    newLogEntry.set_command(command.asString());
    newLogEntry.set_logterm(m_currentTerm);
    newLogEntry.set_logindex(getNewCommandIndex());
    m_logs.emplace_back(newLogEntry);
    int lastLogIndex = getLastLogIndex();
    DPrintf("[func-Start-rf{%d}]  lastLogIndex:%d,command:%s\n", m_me, lastLogIndex, &command);

    persist();
    *newLogIndex = newLogEntry.logindex();
    *newLogTerm = newLogEntry.logterm();
    *isLeader = true;
/*
 ����������Ա��Ҫ����һ�� Raft ������������ Raft ������������������Ķ˿�
 ���� peers[] �С����
 �������Ķ˿��� peers[me]�����з������� peers[] ����
 ������ͬ��˳��persister �Ǵ˷�����������־�״̬�ĵط������������������������״̬������У���
 applyCh �ǲ�����Ա��������� Raft ���� ApplyMsg ��Ϣ��ͨ����
 */
}
// ��ȡ������Ӧ�÷����Index
int Raft::getNewCommandIndex() 
{
    auto lastLogIndex = getLastLogIndex();
    return lastLogIndex + 1;
}

//��ʼ��ĳһ��raft��㣺
void Raft::init(std::vector<std::shared_ptr<RaftRpcUtil>> peers, int me, std::shared_ptr<Persister> persister,
                std::shared_ptr<LockQueue<ApplyMsg>> applyCh) 
{
    m_peers = peers;
    m_persister = persister;
    m_me = me;

    m_mtx.lock();
    //�ͻ��˺�raft������ͨ�ŵĲ��֣�
    this->applyChan = applyCh;
    m_currentTerm = 0;
    m_status = Follower;
    m_commitIndex = 0;
    m_lastApplied = 0;
    m_logs.clear();
    for (int i = 0; i < m_peers.size(); i++) {
        m_matchIndex.push_back(0);
        m_nextIndex.push_back(0);
    }
    //��ʾ����ѡ�ٵ���˼
    m_votedFor = -1;
    m_lastSnapshotIncludeIndex = 0;
    m_lastSnapshotIncludeTerm = 0;
    m_lastResetElectionTime = now();
    m_lastResetHearBeatTime = now();
    //��֮ǰ�����״̬���г�ʼ��,��ȡ֮ǰ���������
    readPersist(m_persister->ReadRaftState());
    //���ﻹ��Ҫ��һ��
    if (m_lastSnapshotIncludeIndex > 0) {
        m_lastApplied = m_lastSnapshotIncludeIndex;
        // rf.commitIndex = rf.lastSnapshotIncludeIndex   todo �������ָ�Ϊ�β��ܶ�ȡcommitIndex��������
    }
    DPrintf("[Init&ReInit] Sever %d, term %d, lastSnapshotIncludeIndex {%d} , lastSnapshotIncludeTerm {%d}", m_me,
            m_currentTerm, m_lastSnapshotIncludeIndex, m_lastSnapshotIncludeTerm);
    m_mtx.unlock();
    m_ioManager = std::make_unique<monsoon::IOManager>(FIBER_THREAD_NUM, FIBER_USE_CALLER_THREAD);
    //���ѡ�ٲ��֣���������Э�̶�ʱ�����д���
    //����������leaderHearBeatTicker ��electionTimeOutTickerִ��ʱ���Ǻ㶨��
    //applierTickerʱ���ܵ����ݿ���Ӧ�ӳٺ�����apply֮������������Ӱ�죬�������������������ܲ�̫��������仹������һ���߳�
    m_ioManager->scheduler([this]() -> void { this->leaderHearBeatTicker(); });
    m_ioManager->scheduler([this]() -> void { this->electionTimeOutTicker(); }); ////���ڼ���Ƿ�������Ҫ����ѡ��
    std::thread t3(&Raft::applierTicker, this);
    t3.detach();

}

//���ڼ���Ƿ�������Ҫ����ѡ��
void Raft::electionTimeOutTicker()
{
    //�����˯�ߣ���ô����leader�����������һֱ��ת���˷�cpu���Ҽ���Э��֮�󣬿�ת�ᵼ������Э���޷����У�����ʱ�����е�AE���ᵼ�������޷��������͵����쳣
    while(true){
        while (m_status == Leader) {
            //����΢��𣺺�sleep����������һ����λ
            usleep(HeartBeatTimeout);  //��ʱʱ��û���Ͻ����ã���ΪHeartBeatTimeout��ѡ�ٳ�ʱһ��Сһ������������˾�����ΪHeartBeatTimeout��
        }
        //�߼���������ʱ��࣬getRandomizedElectionTimeout�������λ��commonn�ļ����ڣ�������һ������������ʱʱ���
        //�����
        std::chrono::duration<signed long int, std::ratio<1, 1000000000>> suitableSleepTime{};
        std::chrono::system_clock::time_point wakeTime{};
        {
            m_mtx.lock();
            wakeTime = now();
            suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime - wakeTime;
            m_mtx.unlock();
        }
        if (std::chrono::duration<double, std::milli>(suitableSleepTime).count() > 1) {
            // ��ȡ��ǰʱ���
            auto start = std::chrono::steady_clock::now();
            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
            // ��ȡ�������н������ʱ���
            auto end = std::chrono::steady_clock::now();
            // ����ʱ������������λΪ���룩
            std::chrono::duration<double, std::milli> duration = end - start;
            // ʹ��ANSI�������н������ɫ�޸�Ϊ��ɫ
            std::cout << "\033[1;35m electionTimeOutTicker();��������˯��ʱ��Ϊ: "
                    << std::chrono::duration_cast<std::chrono::milliseconds>(suitableSleepTime).count() << " ����\033[0m"
                    << std::endl;
            std::cout << "\033[1;35m electionTimeOutTicker();����ʵ��˯��ʱ��Ϊ: " << duration.count() << " ����\033[0m"
                    << std::endl;
        }
        if (std::chrono::duration<double, std::milli>(m_lastResetElectionTime - wakeTime).count() > 0) {
        //˵��˯�ߵ����ʱ�������ö�ʱ������ô��û�г�ʱ���ٴ�˯��
            continue;
        }
        doElection();
    }

}




void Raft::leaderHearBeatTicker() 
{
    while(true){
        //  ����leader�Ļ���û�б�Ҫ���к������������һ�Ҫ��������Ӱ�����ܣ�Ŀǰ��˯�ߣ��������Ż��Ż�
        while (m_status != Leader) {
            usleep(1000 * HeartBeatTimeout);
        }
        static std::atomic<int32_t> atomicCount = 0; //ͳ�������˶��ٴ�
        std::chrono::duration<signed long int, std::ratio<1, 1000000000>> suitableSleepTime{};
        std::chrono::system_clock::time_point wakeTime{}; //��ʾ��ǰʱ��
        //ͨ��milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - wakeTime�����Ƿ�ʱ
        //�������1�Ļ�����ô��˵�����볬ʱʱ�仹�������Խ���ǰ�߳��ó�����sleep��û��Ҫ����������εĻ���ֱ�Ӳ�˯����
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            wakeTime = now();
            suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - wakeTime;
        }
        if (std::chrono::duration<double, std::milli>(suitableSleepTime).count() > 1) {
            std::cout << atomicCount << "\033[1;35m leaderHearBeatTicker();��������˯��ʱ��Ϊ: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(suitableSleepTime).count() << " ����\033[0m"
                << std::endl;
            // ��ȡ��ǰʱ���
            auto start = std::chrono::steady_clock::now();

            usleep(std::chrono::duration_cast<std::chrono::microseconds>(suitableSleepTime).count());
            // std::this_thread::sleep_for(suitableSleepTime);

            // ��ȡ�������н������ʱ���
            auto end = std::chrono::steady_clock::now();

            // ����ʱ������������λΪ���룩
            std::chrono::duration<double, std::milli> duration = end - start;

            // ʹ��ANSI�������н������ɫ�޸�Ϊ��ɫ
            std::cout << atomicCount << "\033[1;35m leaderHearBeatTicker();����ʵ��˯��ʱ��Ϊ: " << duration.count()
                    << " ����\033[0m" << std::endl;
            ++atomicCount;
        }
        //˵���з�������
        if (std::chrono::duration<double, std::milli>(m_lastResetHearBeatTime - wakeTime).count() > 0) {
        //˯�ߵ����ʱ�������ö�ʱ����û�г�ʱ���ٴ�˯��
            continue;
        }
        doHeartBeat();
    }
}


std::vector<ApplyMsg> Raft::getApplyLogs()
{
    std::vector<ApplyMsg> applyMsgs;
    myAssert(m_commitIndex <= getLastLogIndex(), format("[func-getApplyLogs-rf{%d}] commitIndex{%d} >getLastLogIndex{%d}",
                                                    m_me, m_commitIndex, getLastLogIndex()));
    while(m_lastApplied < m_commitIndex){
        m_lastApplied++;
        myAssert(m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex() == m_lastApplied,
                 format("rf.logs[rf.getSlicesIndexFromLogIndex(rf.lastApplied)].LogIndex{%d} != rf.lastApplied{%d} ",
                        m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].logindex(), m_lastApplied));
        ApplyMsg applyMsg;
        applyMsg.CommandValid = true;
        applyMsg.SnapshotValid = false;
        applyMsg.Command = m_logs[getSlicesIndexFromLogIndex(m_lastApplied)].command();
        applyMsg.CommandIndex = m_lastApplied;
        applyMsgs.emplace_back(applyMsg);
    }
    return applyMsgs;
}

//���Ӧ����Ӧ�õ��ͻ��˺ͷ���˵Ĺܵ��Ķ�ʱ���ƣ���ʱ��һЩ��־���з���ܵ����Ա�ͻ��˿����ã�
void Raft::applierTicker()
{
    while(true){
        m_mtx.lock();
        if (m_status == Leader) {
            DPrintf("[Raft::applierTicker() - raft{%d}]  m_lastApplied{%d}   m_commitIndex{%d}", m_me, m_lastApplied,
                    m_commitIndex);
        }
        auto applyMsgs = getApplyLogs();
        m_mtx.unlock();
        if (!applyMsgs.empty()) {
            DPrintf("[func- Raft::applierTicker()-raft{%d}] ��kvserver����applyMsgs�L�Ƞ���{%d}", m_me, applyMsgs.size());
        }
        for(auto & message : applyMsgs){
            applyChan->Push(message);
        }
        //����˯��
        sleepNMilliseconds(ApplyInterval);
    }

}

//��ȡ��ǰ�Ľ���״̬
void Raft::GetState(int* term, bool* isLeader) {
    m_mtx.lock();
    DEFER {
        // todo ��ʱ������᲻�ᵼ������
        m_mtx.unlock();
    };

    // Your code here (2A).
    *term = m_currentTerm;
    *isLeader = (m_status == Leader);
}

void Raft::persist() 
{
    //���浽�����棬��ȡ������־����Ϣ��û�л�ȡ���յľ�����Ϣ
    auto data = persistData();
    //���䱣�浽״̬����
    m_persister->SaveRaftState(data);
}


//���ý����������ݣ��������л���Ȼ����һ���ַ������ع�ȥ
std::string Raft::persistData() {
    BoostPersistRaftNode boostPersistRaftNode;
    boostPersistRaftNode.m_currentTerm = m_currentTerm;
    boostPersistRaftNode.m_votedFor = m_votedFor;
    boostPersistRaftNode.m_lastSnapshotIncludeIndex = m_lastSnapshotIncludeIndex;
    boostPersistRaftNode.m_lastSnapshotIncludeTerm = m_lastSnapshotIncludeTerm;
    for (auto& item : m_logs) {
        boostPersistRaftNode.m_logs.push_back(item.SerializeAsString()); //SerializeAsString() ��һ�������� Protobuf ����������־��Ŀת��Ϊ�������ַ�����ʾ��
    }
    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);
    oa << boostPersistRaftNode;
    return ss.str();
}

//�����л����������ݴ�״̬���ָ��������
void Raft::readPersist(std::string data) {
    if (data.empty()) {
        return;
    }
    std::stringstream iss(data);
    boost::archive::text_iarchive ia(iss);
    // read class state from archive
    BoostPersistRaftNode boostPersistRaftNode;
    ia >> boostPersistRaftNode;

    m_currentTerm = boostPersistRaftNode.m_currentTerm;
    m_votedFor = boostPersistRaftNode.m_votedFor;
    m_lastSnapshotIncludeIndex = boostPersistRaftNode.m_lastSnapshotIncludeIndex;
    m_lastSnapshotIncludeTerm = boostPersistRaftNode.m_lastSnapshotIncludeTerm;
    m_logs.clear();
    for (auto& item : boostPersistRaftNode.m_logs) {
        raftRpcProctoc::LogEntry logEntry;
        logEntry.ParseFromString(item);
        m_logs.emplace_back(logEntry);
    }
}

//�����쵼�ߵ��ύ��¼
void Raft::leaderUpdateCommitIndex() 
{
    m_commitIndex = m_lastSnapshotIncludeIndex;
    for (int index = getLastLogIndex(); index >= m_lastSnapshotIncludeIndex + 1; index--) {
        int sum = 0;
        for (int i = 0; i < m_peers.size(); i++) {
            if (i == m_me) {
                sum += 1;
                continue;
            }
            if (m_matchIndex[i] >= index) {
                sum += 1;
            }
        }
    //ֻ�е�ǰ��־��һ�����ϵĽ���Ͽ��ˣ��ſ���,������Ͽ��ˣ���ôǰ���Ҳһ���Ͽɣ�������ͨ���Ӻ���ǰ�ķ�ʽѰ�ң�����һ��
    if (sum >= m_peers.size() / 2 + 1 && getLogTermFromLogIndex(index) == m_currentTerm) {
        m_commitIndex = index;
        break;
    }
  }

}
//�ж�һ����ѡ�߻����󷽵���־�Ƿ�ȵ�ǰ�ڵ����־���ӡ��¡�
bool Raft::UpToDate(int index, int term) {
    int lastIndex = -1;
    int lastTerm = -1;
    getLastLogIndexAndTerm(&lastIndex, &lastTerm);
    return term > lastTerm || (term == lastTerm && index >= lastIndex);
}


void Raft::RequestVote(const raftRpcProctoc::RequestVoteArgs* args, raftRpcProctoc::RequestVoteReply* reply)
{
    std::lock_guard<std::mutex> lg(m_mtx);
    DEFER {
        //Ӧ���ȳ־û����ٳ���lock
        persist();
    };
    //���������ѡ�߳���������������������ֻ�������ѡ�߳��������쵼�ߵ�ͨ�Ų���,���Ǹý��ս��������쵼������
    //��Ȼ��ʱ���ս�����ڱ��������Ҫ��
    if (args->term() < m_currentTerm) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Expire);
        reply->set_votegranted(false);
        return;
    }
    if (args->term() > m_currentTerm) {
        m_status = Follower;
        m_currentTerm = args->term();
        m_votedFor = -1;   //-1��ʾ���Խ���ͶƱ����ʾƱ��û�б���
    }
    myAssert(args->term() == m_currentTerm,
               format("[func--rf{%d}] ǰ��У���args.Term==rf.currentTerm������ȴ����", m_me));
    //����Ҫ���log��term��index�ǲ���ƥ�����
    int lastLogTerm = getLastLogTerm();
    //˵����־̫����,�������������ͶƱ
    if (!UpToDate(args->lastlogindex(), args->lastlogterm())) {
        reply->set_term(m_currentTerm);
        reply->set_votestate(Voted);
        reply->set_votegranted(false);
        return ;
    }
    m_votedFor = args->candidateid(); //��ʾͶ�������ѡ��
    m_lastResetElectionTime = now();  //����Ҫ��Ͷ��Ʊ��ʱ������ö�ʱ��
    reply->set_term(m_currentTerm);
    reply->set_votestate(Normal);
    reply->set_votegranted(true);

    return;
}

void Raft::Snapshot(int index, std::string snapshot)
{
    std::lock_guard<std::mutex> lg(m_mtx);
    //��һ��������ʾ֮ǰ���Ŀ��հ���index�±�Ŀ����ˣ���β������ˡ�
    //�����ǿ��ձ���Ҫ��֤һ���ԣ���Ȼ�Ǵ������㶼�Ͽɵ���־�����Ա������ύ�ķ�Χ֮��
    if (m_lastSnapshotIncludeIndex >= index || index > m_commitIndex) {
        DPrintf(
            "[func-Snapshot-rf{%d}] rejects replacing log with snapshotIndex %d as current snapshotIndex %d is larger or "
            "smaller ",
            m_me, index, m_lastSnapshotIncludeIndex);
        return;
    }
    auto lastLogIndex = getLastLogIndex();  //Ϊ�˼��snapshotǰ����־�Ƿ�һ������ֹ���ȡ�����ٽ�ȡ��־
    //��ʾ��һ����Ҫ���յ�λ����index����Ĳ����㣬��Ϊ��ǰ���ֶ��Ѿ���������
    int newLastSnapshotIncludeIndex = index;
    int newLastSnapshotIncludeTerm = m_logs[getSlicesIndexFromLogIndex(index)].logterm();
    std::vector<raftRpcProctoc::LogEntry> trunckedLogs; //��ʾʣ�µ���־����ߺ������������յĴ��벿�֣����߼�ʵ�֣������������
    for (int i = index + 1; i <= getLastLogIndex(); i++) {
        //ע����=����ΪҪ�õ����һ����־
        trunckedLogs.push_back(m_logs[getSlicesIndexFromLogIndex(i)]);
    }
    m_lastSnapshotIncludeIndex = newLastSnapshotIncludeIndex;
    m_lastSnapshotIncludeTerm = newLastSnapshotIncludeTerm;
    m_logs = trunckedLogs;
    //�о���һ��û�б�Ҫ
    m_commitIndex = std::max(m_commitIndex, index);
    m_lastApplied = std::max(m_lastApplied, index);
    //�־û����ǿ����Ժ����־
    m_persister->Save(persistData(), snapshot);

    DPrintf("[SnapShot]Server %d snapshot snapshot index {%d}, term {%d}, loglen {%d}", m_me, index,
              m_lastSnapshotIncludeTerm, m_logs.size());
    myAssert(m_logs.size() + m_lastSnapshotIncludeIndex == lastLogIndex,
               format("len(rf.logs){%d} + rf.lastSnapshotIncludeIndex{%d} != lastLogjInde{%d}", m_logs.size(),
                      m_lastSnapshotIncludeIndex, lastLogIndex));

}
