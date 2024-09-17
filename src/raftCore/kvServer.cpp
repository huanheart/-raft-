#include "include/kvServer.h"

#include <rpcprovider.h>

#include "mprpcconfig.h"


void KvServer::DprintfKVDB() {
    if (!Debug) {
        return;
    }
    std::lock_guard<std::mutex> lg(m_mtx);
    DEFER {
        m_skipList.display_list();
    };
}

void KvServer::ExecuteAppendOpOnKVDB(Op op) 
{
    m_mtx.lock();

    m_skipList.insert_set_element(op.Key, op.Value);
    m_lastRequestId[op.ClientId] = op.RequestId;

    m_mtx.unlock();
    DprintfKVDB();
}

void KvServer::ExecuteGetOpOnKVDB(Op op, std::string *value, bool *exist) {
  m_mtx.lock();
  *value = "";
  *exist = false;
  if (m_skipList.search_element(op.Key, *value)) {
    *exist = true;
  }
  m_lastRequestId[op.ClientId] = op.RequestId;
  m_mtx.unlock();

  DprintfKVDB();
}

void KvServer::ExecutePutOpOnKVDB(Op op)
{
    m_mtx.lock();
    m_skipList.insert_set_element(op.Key, op.Value);
    m_lastRequestId[op.ClientId] = op.RequestId;
    m_mtx.unlock();

    DprintfKVDB();
}



void KvServer::PutAppend(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::PutAppendArgs *request,
                         ::raftKVRpcProctoc::PutAppendReply *response, ::google::protobuf::Closure *done) 
{
    KvServer::PutAppend(request, response);
    done->Run();
}

void KvServer::Get(google::protobuf::RpcController *controller, const ::raftKVRpcProctoc::GetArgs *request,
                   ::raftKVRpcProctoc::GetReply *response, ::google::protobuf::Closure *done)
{
    KvServer::Get(request, response);
    done->Run();
}


KvServer::KvServer(int me, int maxraftstate, std::string nodeInforFileName, short port) : m_skipList(10)
{
//    sleep(20);
    std::shared_ptr<Persister> persister = std::make_shared<Persister>(me); //����ʹ��
    m_me = me;
    //��ʲôʱ����Ҫ������־����
    m_maxRaftState = maxraftstate;
    //kvServer��raft�ڵ��ͨ�Źܵ�
    applyChan = std::make_shared<LockQueue<ApplyMsg> >();
    //clerk���� kvserver����rpc���ܹ���
    //    ͬʱraft��raft�ڵ�֮��ҲҪ����rpc���ܣ����������ע��

    m_raftNode = std::make_shared<Raft>();


    //����һ���̣߳�ע��һ��rpc�����Լ��������rpc����
    //��Ȼ����֮�����kvserver������һ���߳̽�����ж˿ڵļ���
      std::thread t([this, port]() -> void {
    // provider��һ��rpc���������󡣰�UserService���󷢲���rpc�ڵ���
    //����һ��ע������
    RpcProvider provider;
    //����Ӧ�����kvServer�Լ�raft�ķ��񴴽������provider��
    provider.NotifyService(this);
//    std::cout<<"notify servicd now"<<std::endl;
    provider.NotifyService(this->m_raftNode.get());  
        // ����һ��rpc���񷢲��ڵ�   Run�Ժ󣬽��̽�������״̬���ȴ�Զ�̵�rpc��������
        provider.Run(m_me, port);
    });
    t.detach();

    ////����rpcԶ�̵�����������Ҫע�����Ҫ��֤���нڵ㶼����rpc���ܹ���֮����ܿ���rpcԶ�̵�������
    ////����ʹ��˯������֤
    ////�����ͨ������̣߳������˵�ǰraft����tcpserver���������������м�������raft��������
    ////��֤ÿ��raft���ķ��񶼿����ˣ��������캯�����������������raft���������Ӳ�����������ܽ���Զ��ͨ�ţ�ͨ��RPCUTIL����ࣩ
    std::cout << "raftServer node:" << m_me << " start to sleep to wait all ohter raftnode start!!!!" << std::endl;
    sleep(6);
    std::cout << "raftServer node:" << m_me << " wake up!!!! start to connect other raftnode" << std::endl;
    //��ȡ����raft�ڵ�ip��port ������������  ,Ҫ�ų��Լ�
    MprpcConfig config;
    //nodeInforFileName�����ʵ�������test.conf��������ļ�
    //��������ļ�����������raft������Ϣ
    config.LoadConfigFile(nodeInforFileName.c_str());
    std::vector<std::pair<std::string, short> > ipPortVt;
    for (int i = 0; i < INT_MAX - 1; ++i) {
        std::string node = "node" + std::to_string(i);
        std::string nodeIp = config.Load(node + "ip");
        std::string nodePortStr = config.Load(node + "port");
        if (nodeIp.empty()) {
            break;
        }
        ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str())); 
    }
    //��ʼ�������ӣ�RaftRpcUtil�ڲ�����һ��stub������ͨ����
    std::vector<std::shared_ptr<RaftRpcUtil> > servers;
    for (int i = 0; i < ipPortVt.size(); ++i) {
        //�����Լ�����
        if (i == m_me) {
            servers.push_back(nullptr);
            continue;
        }
        std::string otherNodeIp = ipPortVt[i].first;
        short otherNodePort = ipPortVt[i].second;
        //ÿһ��kvserver���ж��RaftRpcUtil��ÿһ��RaftRpcUtil�ڲ�����һ��channelͨ����������ʹ��RaftRpcUtil��ʱ����ж�Ӧ�����ĵ���
        //GRPC�ڲ��Ὣchannel���ڲ��������лص�,������CallMethod����
        auto *rpc = new RaftRpcUtil(otherNodeIp, otherNodePort);
        servers.push_back(std::shared_ptr<RaftRpcUtil>(rpc));

        std::cout << "node" << m_me << " ����node" << i << "success!" << std::endl;
    }
    sleep(ipPortVt.size() - me);  //�ȴ����нڵ��໥���ӳɹ���������raft
    m_raftNode->init(servers, m_me, persister, applyChan); //applyChan�Ǻ�raft����ͨ������ž�����raft�������kvserver����ط������ݣ�Ȼ��kvserver�ٽ���һ���־û���
    //  // kv��serverֱ����raftͨ�ţ���kv��ֱ����raftͨ�ţ�������Ҫ��ApplyMsg��chan������ȥ����ͨ�ţ����ߵ�persistҲ�ǹ��õ�

    m_skipList;
    waitApplyCh;
    m_lastRequestId;
    m_lastSnapShotRaftLogIndex = 0; 
    auto snapshot = persister->ReadSnapshot();
    if (!snapshot.empty()) {
        ReadSnapShotToInstall(snapshot);
    }
    std::thread t2(&KvServer::ReadRaftApplyCommandLoop, this);  //�����������ڵ������Լ�����leader
    t2.join();  //����ReadRaftApplyCommandLoopһֱ����Y�����ﵽһֱ�������Ŀ��
    //������Ӧ����Ϊ�˼���һ���̵߳Ŀ������߶࿪һ��ר�ŵĺ��������Ǹо���̫������ôд
}


//�����廹��ϸ����
// raft����persist�㽻����kvserver��Ҳ�ᣬ��Ϊkvserver�㿪ʼ��ʱ����Ҫ�ָ�kvdb��״̬
//  ���ڿ���raft����persist�Ľ���������kvserver������snapshot������leaderInstallSnapshot RPC��ʱ��Ҳ��Ҫ��ȡsnapshot��
//  ���snapshot�ľ����ʽ����kvserver�������ģ�raftֻ���𴫵��������
//  snapShot�������kvserver��Ҫά����persist_lastRequestId �Լ�kvDB�������������persist_kvdb
void KvServer::ReadSnapShotToInstall(std::string snapshot) 
{
    if (snapshot.empty()) {
        return;
    }
    parseFromString(snapshot);
}

//����ѭ�����֣���kvServer��raft�ڵ��ͨ�Źܵ�����ȡ����
//Ȼ��ִ���������˵ִ�ж�Ӧ�Ĳ��뵽kv��ֵ��������߸��ĵĲ���
void KvServer::ReadRaftApplyCommandLoop()
{
    while(true){
        //applyChan�Լ�����
        auto message =applyChan->Pop(); //��������
        DPrintf("---------------tmp-------------[func-KvServer::ReadRaftApplyCommandLoop()-kvserver{%d}] �յ�����raft����Ϣ",m_me);
        if (message.CommandValid) {
            GetCommandFromRaft(message);
        }
        if (message.SnapshotValid) {
            GetSnapShotFromRaft(message);
        }
    }
}

void KvServer::GetSnapShotFromRaft(ApplyMsg message)
{
    std::lock_guard<std::mutex> lg(m_mtx);
    if (m_raftNode->CondInstallSnapshot(message.SnapshotTerm, message.SnapshotIndex, message.Snapshot)) {
        ReadSnapShotToInstall(message.Snapshot);
        m_lastSnapShotRaftLogIndex = message.SnapshotIndex;
    }
}



void KvServer::GetCommandFromRaft(ApplyMsg message) 
{
    //���������Ϊ�ַ�����ʽ
    Op op;
    op.parseFromString(message.Command);

    DPrintf(
          "[KvServer::GetCommandFromRaft-kvserver{%d}] , Got Command --> Index:{%d} , ClientId {%s}, RequestId {%d}, "
            "Opreation {%s}, Key :{%s}, Value :{%s}",
            m_me, message.CommandIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);    

    //���Ӧ���ǿ��յĲ��֣�������־�Ĳ��֣���Ȼ��return
    //���仰˵�������־�Ѿ���������ˣ�������ʽ�����Կ��յ���ʽ�������
    if (message.CommandIndex <= m_lastSnapShotRaftLogIndex) {
        return;
    }
    // ״̬����KVServer����ظ����⣩
    // ����ִ���ظ�������
    //������ظ��������ô��ִ��
    if (!ifRequestDuplicate(op.ClientId, op.RequestId)) {
        if (op.Operation == "Put") {
            ExecutePutOpOnKVDB(op);
        }
        if (op.Operation == "Append") {
            ExecuteAppendOpOnKVDB(op);
        }
    }
    ////������kvDB�Ѿ������˿���
    ////m_maxRaftState�����ʾ������־������Ҫ���п���
    if (m_maxRaftState != -1) {
        //�ж��Ƿ���Ҫ���գ���Ҫ���ڲ���ֱ�ӽ��ж�Ӧ�Ĳ���
        IfNeedToSendSnapShotCommand(message.CommandIndex, 9);  //���9�о�һ���ö�û��
        //���raft��log̫�󣨴���ָ���ı������Ͱ���������
    }
    SendMessageToWaitChan(op, message.CommandIndex);
}

bool KvServer::ifRequestDuplicate(std::string ClientId, int RequestId)
{
    std::lock_guard<std::mutex> lg(m_mtx);
    //˵������û������Ͳ����ڣ���ô�϶�����false
    if (m_lastRequestId.find(ClientId) == m_lastRequestId.end()) {
        return false;
    }
    //�鿴�Ƿ�֮ǰ��������ļ�¼�±�,�ж��Ƿ����ظ�����
    return RequestId <= m_lastRequestId[ClientId];
}

void KvServer::IfNeedToSendSnapShotCommand(int raftIndex, int proportion)
{
    if (m_raftNode->GetRaftStateSize() > m_maxRaftState / 10.0) {
        auto snapshot = MakeSnapShot(); //��������
        m_raftNode->Snapshot(raftIndex, snapshot); //����һ�����գ�Ȼ����raftindexλ��ǰ����־��ɾ�����Ҵ�ſ���
    }
}

std::string KvServer::MakeSnapShot() {
    std::lock_guard<std::mutex> lg(m_mtx);
    std::string snapshotData = getSnapshotData(); //��keserver.hͷ�ļ���
    ////��߷����˶�Ӧ���л�������ݣ����統ǰkvserver���������ļ��е����ݣ��е����ݣ���ǰkvserver���ࣨ�������������������ݣ��Լ�
    ////raft������������ͨ�ŵ����ݣ���һ��Ӧ��������raft���͵������±�ӦΪ���٣�
    return snapshotData;
}

bool KvServer::SendMessageToWaitChan(const Op &op, int raftIndex)
{
    std::lock_guard<std::mutex> lg(m_mtx);
    DPrintf(
            "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
            "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
            m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        return false;
    }
    waitApplyCh[raftIndex]->Push(op);
    DPrintf(
            "[RaftApplyMessageSendToWaitChan--> raftserver{%d}] , Send Command --> Index:{%d} , ClientId {%d}, RequestId "
            "{%d}, Opreation {%v}, Key :{%v}, Value :{%v}",
            m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
    return true; 
}

// ��������clerk��Get RPC
void KvServer::Get(const raftKVRpcProctoc::GetArgs *args, raftKVRpcProctoc::GetReply *reply)
{
    Op op;
    op.Operation = "Get";
    op.Key = args->key();
    op.Value = "";
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();

    int raftIndex = -1;
    int _ = -1;
    bool isLeader = false;
    //��ȡ�µ���־�±��λ�ã�Ӧ�ñ��ŵ�����
    m_raftNode->Start(op, &raftIndex, &_,&isLeader); 

    if (!isLeader) {
        reply->set_err(ErrWrongLeader);
        return;
    }
    // create waitForCh
    m_mtx.lock();
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        //����һ����־���У�ר��Ϊ���raftIndex
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    auto chForRaftIndex = waitApplyCh[raftIndex];

    m_mtx.unlock();  //ֱ�ӽ������ȴ�����ִ����ɣ�����һֱ�����ȴ�

    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
        //�����ǰ��ʱ�ˣ�����֮ǰ�ɹ��ύ������ô����Ҳ������Ϊ���ǳɹ���
        int _ = -1;
        bool isLeader = false;
        m_raftNode->GetState(&_, &isLeader);
        if (ifRequestDuplicate(op.ClientId, op.RequestId) && isLeader){
            //�����ʱ������raft��Ⱥ����֤�Ѿ�commitIndex����־������������Ѿ��ύ����get�����ǿ�����ִ�еġ�
            std::string value;
            bool exist = false;
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            }else{
                reply->set_err(ErrNoKey);
                reply->set_value("");
            }      
        
        
        }else{ //������ܲ����쵼�ߣ�
            reply->set_err(ErrWrongLeader);  //�����������ʵ������clerk��һ���ڵ�����
        }
    }else{ //˵�� raft�Ѿ��ύ�˸�command��op����������ʽ��ʼִ����
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
            std::string value;
            bool exist = false;
            ExecuteGetOpOnKVDB(op, &value, &exist);
            if (exist) {
                reply->set_err(OK);
                reply->set_value(value);
            } else {
                reply->set_err(ErrNoKey);
                reply->set_value("");
            }
        }else{
            reply->set_err(ErrWrongLeader);
        }
        
    }
    m_mtx.lock(); 
    auto tmp = waitApplyCh[raftIndex];
    waitApplyCh.erase(raftIndex);
    delete tmp;
    m_mtx.unlock();

}


void KvServer::PutAppend(const raftKVRpcProctoc::PutAppendArgs *args, raftKVRpcProctoc::PutAppendReply *reply)
{
    Op op;
    op.Operation = args->op();
    op.Key = args->key();
    op.Value = args->value();
    op.ClientId = args->clientid();
    op.RequestId = args->requestid();
    int raftIndex = -1;
    int _ = -1;
    bool isleader = false;

    m_raftNode->Start(op, &raftIndex, &_, &isleader);


    if (!isleader) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, raftIndex %d , but "
            "not leader",
            m_me, &args->clientid(), args->requestid(), m_me, &op.Key, raftIndex);

        reply->set_err(ErrWrongLeader);
        return;
    }

    DPrintf(
        "[func -KvServer::PutAppend -kvserver{%d}]From Client %s (Request %d) To Server %d, key %s, raftIndex %d , is "
        "leader ",
        m_me, &args->clientid(), args->requestid(), m_me, &op.Key, raftIndex);
    m_mtx.lock();
    if (waitApplyCh.find(raftIndex) == waitApplyCh.end()) {
        //����һ����־���У�ר��Ϊ���raftIndex
        waitApplyCh.insert(std::make_pair(raftIndex, new LockQueue<Op>()));
    }
    auto chForRaftIndex = waitApplyCh[raftIndex];
    m_mtx.unlock();  //ֱ�ӽ������ȴ�����ִ����ɣ�����һֱ�����ȴ�
    // timeout
    Op raftCommitOp;

    if (!chForRaftIndex->timeOutPop(CONSENSUS_TIMEOUT, &raftCommitOp)) {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]TIMEOUT PUTAPPEND !!!! Server %d , get Command <-- Index:%d , "
            "ClientId %s, RequestId %s, Opreation %s Key :%s, Value :%s",
            m_me, m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);

        if (ifRequestDuplicate(op.ClientId, op.RequestId)) {
            reply->set_err(OK);  // ��ʱ��,����Ϊ���ظ������󣬷���ok��ʵ���Ͼ���û�г�ʱ��������ִ�е�ʱ��ҲҪ�ж��Ƿ��ظ�
        } else {
            reply->set_err(ErrWrongLeader);  ///���ﷵ�������Ŀ����clerk���³���
        }
  } else {
        DPrintf(
            "[func -KvServer::PutAppend -kvserver{%d}]WaitChanGetRaftApplyMessage<--Server %d , get Command <-- Index:%d , "
            "ClientId %s, RequestId %d, Opreation %s, Key :%s, Value :%s",
            m_me, m_me, raftIndex, &op.ClientId, op.RequestId, &op.Operation, &op.Key, &op.Value);
        if (raftCommitOp.ClientId == op.ClientId && raftCommitOp.RequestId == op.RequestId) {
        //���ܷ���leader�ı��������־�����ǣ���˱�����
            reply->set_err(OK);
        } else {
            reply->set_err(ErrWrongLeader);
        }
  }
  m_mtx.lock();
  auto tmp = waitApplyCh[raftIndex];
  waitApplyCh.erase(raftIndex);
  delete tmp;
  m_mtx.unlock();

}


