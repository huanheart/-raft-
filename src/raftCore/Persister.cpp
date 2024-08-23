#include "Persister.h"
#include "util.h"



void Persister::Save(const std::string raftstate, const std::string snapshot) 
{
    std::lock_guard<std::mutex> lg(m_mtx);
    clearRaftStateAndSnapshot();
    // ��raftstate��snapshotд�뱾���ļ�
    m_raftStateOutStream << raftstate;
    m_snapshotOutStream << snapshot;
    //����Ӧ�ü���m_raftStateSize+=size()�ɣ�
}

std::string Persister::ReadSnapshot() 
{
    std::lock_guard<std::mutex> lg(m_mtx);
    if (m_snapshotOutStream.is_open()) {
      m_snapshotOutStream.close();
    }
    //�����ӳٵ��õ����壺��Ϊ����߳��У������ж����д���ڷ���������������������������������д
    //��;���е�һ�룬��ôǰ���Ǳ�������򿪵ģ����Ե����Ƕ�ȡ��ʱ����ܾͳ����������ˣ���Ȼ���ǽ����Ƚ��йر�
    //Ȼ���ʱ����ͨ������һ�������ж�ȡ��ǰ�ļ���״̬�������ȡ��ϣ��ٽ���һ���ļ��Ĺرռ���,���������������ļ����´�
    //�����Ǹ�û����������ľͿ��Լ������в����ˣ���ϸ�ڵĴ���,
    DEFER {
      m_snapshotOutStream.open(m_snapshotFileName);  //Ĭ����׷��
    };
    std::fstream ifs(m_snapshotFileName, std::ios_base::in);
    if (!ifs.good()) {       //˼����Ϊʲô���ﲻ�õ���close������ ����ڲ�������(����RAII�ķ�ʽ)
      return "";
    }
    std::string snapshot;
    ifs >> snapshot;
    //���ܲ���RAII�ķ�ʽ��������ʾ������Ȼ��ֱ�۸�������
    ifs.close();
    return snapshot;
}


void Persister::SaveRaftState(const std::string &data) 
{
    std::lock_guard<std::mutex> lg(m_mtx);
    clearRaftState();
    m_raftStateOutStream << data;
    m_raftStateSize += data.size();
}


long long Persister::RaftStateSize() 
{
    std::lock_guard<std::mutex> lg(m_mtx);
    return m_raftStateSize;
}


//���������ȡ�ļ���Ȼ�󽫶�ȡ�������ݸ�ֵ��snapshot�����з���
std::string Persister::ReadRaftState() 
{
    std::lock_guard<std::mutex> lg(m_mtx);
    std::fstream ifs(m_raftStateFileName, std::ios_base::in); //std::ios_base::in�Զ��ķ�ʽ������ļ�
    if (!ifs.good()) { //�鿴����״̬�Ƿ����ã����Ƿ���������
      return "";
    }
    std::string snapshot;
    ifs >> snapshot;
    ifs.close();
    return snapshot;
}


Persister::Persister(const int me)
    : m_raftStateFileName("raftstatePersist" + std::to_string(me) + ".txt"),
      m_snapshotFileName("snapshotPersist" + std::to_string(me) + ".txt"),
      m_raftStateSize(0) 
{
    bool fileOpenFlag = true;
    std::fstream file(m_raftStateFileName, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
      file.close();
    } else {
      fileOpenFlag = false;
    }
    file = std::fstream(m_snapshotFileName, std::ios::out | std::ios::trunc);
    if (file.is_open()) {
      file.close();
    } else {
      fileOpenFlag = false;
    }
    if (!fileOpenFlag) {
      DPrintf("[func-Persister::Persister] file open error");
    }
    //���������Ҫ�������ļ��Ƿ��ܱ������򿪹ر���Щ���������Ų��ʱ�򣬿��Բ鿴�ļ����Ƿ��������
    //���������������������ã�ofstream�����д�ļ����࣬�������д�ļ���
    m_raftStateOutStream.open(m_raftStateFileName);
    m_snapshotOutStream.open(m_snapshotFileName);
}


Persister::~Persister()
{
    if (m_raftStateOutStream.is_open()) {
      m_raftStateOutStream.close();
    }
    if (m_snapshotOutStream.is_open()) {
      m_snapshotOutStream.close();
    }

}


void Persister::clearRaftState() {
    m_raftStateSize = 0;
    // �ر��ļ���
    if (m_raftStateOutStream.is_open()) {
      m_raftStateOutStream.close();
    }
    // ���´��ļ���������ļ�����(�൱�ڽ���һ��ˢ��)
    //���ܱ����ɵ�����ûʲô����,���ҷ�ֹ�ļ����ͣ����н϶���ļ�
    m_raftStateOutStream.open(m_raftStateFileName, std::ios::out | std::ios::trunc); //���´򿪵�ʱ�򣬻������֮ǰ���������ݵķ�ʽ��
}


void Persister::clearSnapshot()
{
    if(m_snapshotOutStream.is_open()){
        m_snapshotOutStream.close();
    }
    //Ϊ�µ�д��������׼��
    m_snapshotOutStream.open(m_snapshotFileName,std::ios::out | std::ios::trunc );
    //����ر��ִ򿪵Ĳ�����Ϊ��һ�������ļ�����׼��������std::ios::out����˼�ǣ���׷�ӵ���ʽȥ�����ļ�������Ĳ�����ʾ���
    //�ļ����������ݣ���ô��ѡ�����
    
}


void Persister::clearRaftStateAndSnapshot() {
    clearRaftState();
    clearSnapshot();
}