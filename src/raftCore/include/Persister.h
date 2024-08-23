#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <fstream>
#include <mutex>

//�йس־û�
class Persister
{
private:
    std::mutex m_mtx;
    std::string m_raftState;
    std::string m_snapshot;

    //raftState���ļ���
    const std::string m_raftStateFileName;
    //�����ļ���
    const std::string m_snapshotFileName;
    //����raftState�������
    std::ofstream m_raftStateOutStream;
    //������յ������
    std::ofstream m_snapshotOutStream;
   //����raftStateSize�Ĵ�С
   //����ÿ�ζ���ȡ�ļ�����ȡ����Ĵ�С(���ﻹ���Ǻܶ�)
    long long m_raftStateSize;

public:
    //һЩ�����ӿ�
    void Save(std::string m_raftstate,std::string m_snapshot);
    std::string ReadSnapshot();
    void SaveRaftState(const std::string& data);
    long long RaftStateSize();
    std::string ReadRaftState();
    //explicit��ֹ������ʽת��������˵һ��int�Ϳ��Ը�ֵ��һ�������������
    //һ�����ڹ��캯�������������ȵط�
    explicit Persister(int me);
    ~Persister();
private:
    //������Ϊ��һ�������ļ���������׼������Ȼ����������ʱ��������Щ����
    void clearRaftState();
    void clearSnapshot();
    void clearRaftStateAndSnapshot();
};

#endif