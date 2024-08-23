#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <condition_variable>  // pthread_condition_t
#include <functional>
#include <iostream>
#include <mutex>  // pthread_mutex_t
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include "config.h"

//������RAII��˼�룬�Լ�go��˼�룬��ĩβ��ʱ���Զ��ͷ�һЩ���ݣ���ֹ�ڴ�й©,����Ϊ���ܿ�������
//������һ������ϸ�ڣ������뿴�ĵ�2�е�util����
template < class F>
class DeferClass {
public:
    DeferClass(F&& f) : m_func(std::forward<F>(f)) {}
    DeferClass(const F& f) : m_func(f) {}
    ~DeferClass() { m_func(); }

    DeferClass(const DeferClass& e) = delete;
    DeferClass& operator=(const DeferClass& e) = delete;

private:
    F m_func;
};


#define _CONCAT(a, b) a##b
#define _MAKE_DEFER_(line) DeferClass _CONCAT(defer_placeholder, line) = [&]()

#undef DEFER
#define DEFER _MAKE_DEFER_(__LINE__)

void DPrintf(const char* format, ...);

void myAssert(bool condition, std::string message = "Assertion failed!");

template <typename... Args>
std::string format(const char * format_str,Args... args)
{
    std::stringstream ss;
    int _[]={ ( (ss << args),0 )... };
    (void)_;
    return ss.str();
}


std::chrono::_V2::system_clock::time_point now();

std::chrono::milliseconds getRandomizedElectionTimeout();
void sleepNMilliseconds(int N);


//�������첽д��־����־����:
template <typename T>
class LockQueue 
{
public:
    //���ڶ��worker�̶߳���д��־����Ȼ����ֱ�ӽ�����װ�ڶ��е��ڲ���ʵ��һ����������
    void Push(const T& data){
        std::lock_guard<std::mutex> lock(m_mutex); //RAII˼��;
        m_queue.push(data);
        m_condvariable.notify_one(); //Ϊʲô����һ������Ϊֻ��һ���߳̽��ж�ȡ�������������߳̽��ж�ȡ
    }

    T Pop(){
        std::unique_lock<std::mutex> lock(m_mutex);
        while(m_queue.empty()){
            //��־���пյ�ʱ����ô�߳�ֱ�ӽ���wait״̬��ֱ��������������
            m_condvariable.wait(lock);
        }
        T data=m_queue.front();
        m_queue.pop();
        return data;
    }
    //���һ����ʱ������Ĭ��Ϊ50ms(��������ָ��ʱ���ڻ�û��pop����ô���ǾͲ�pop��)����ֹһֱ����һ���ȴ�
    bool timeOutPop(int timeout,T* ResData)  
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // ��ȡ��ǰʱ��㣬���������ʱʱ��
        auto now = std::chrono::system_clock::now();
        auto timeout_time = now + std::chrono::milliseconds(timeout);

        // �ڳ�ʱ֮ǰ�����ϼ������Ƿ�Ϊ��
        while (m_queue.empty()) {
        // ����Ѿ���ʱ�ˣ��ͷ���һ���ն���
            if (m_condvariable.wait_until(lock, timeout_time) == std::cv_status::timeout) {
                return false;
            } else {
                continue;
            }
        }

        T data = m_queue.front();
        m_queue.pop();
        *ResData = data;
        return true;
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};



//���Op��kv���ݸ�raft��command
class Op{
public:
    //�������е��ֶα����Դ�д��ͷ������rpc���ᱻ�жϣ�
    std::string Operation; //ѡ��ж���get put append�ĸ�����
    std::string Key;
    std::string Value;
    std::string ClientId; //�ͻ��˺���
    int RequestId;         //�ͻ��˺��������Request�����кţ�Ϊ�˱�֤����һ����
    //�ظ�����ܱ�Ӧ�����Σ�ֻ������ PUT �� APPEND

public:
    //���ڿ��Ի��ɸ��߼������л�����������protobuf
    //Ϊ��Э��raftRPC�е�commandֻ���ó���string,��������ƾ��������ַ��в��ܰ���|(������|�����Ϊ�ָ��������ǲ�������ﲻ��protobuf��)
    //�����ﲢû�����л���protobuf���ƺ�ֱ������text_oarchive��Щ����ֱ�����л������ڶ�ȡ���ı���ʽ
    std::string asString() const {
        std::stringstream ss;
        boost::archive::text_oarchive oa(ss);
        //��һ����ʵ��д�뵽����
        oa<<*this;

        return ss.str();
    }
    bool parseFromString(std::string str){
        std::stringstream iss(str);
        boost::archive::text_iarchive ia(iss);
        //��ȡ
        ia >> *this;
        return true;  //�������ʧ����δ����ÿ�һ��boost��Դ��
    }

public:
    //����Ϊ��Ԫ��ԭ��boost����Ҫ���ʵ��ҷ�װ�����˽�г�Ա����Ȼ��Ҫ������Ԫ����
    friend std::ostream& operator<<(std::ostream& os, const Op& obj) {
        os << "[MyClass:Operation{" + obj.Operation + "},Key{" + obj.Key + "},Value{" + obj.Value + "},ClientId{" +
              obj.ClientId + "},RequestId{" + std::to_string(obj.RequestId) + "}";  // ������ʵ���Զ���������ʽ
        return os;
    }

private:
    friend class boost::serialization::access ;
    //����Ϊ��Ԫ��ԭ��boost����Ҫ���ʵ��ҷ�װ�����˽�г�Ա����Ȼ��Ҫ������Ԫ����
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) { //�������л��ģ�Ϊʲôʹ��&�����忴�ֲ�ʽ�洢�ĵ�2,������һ����λ�룬���ܽ���������
        ar& Operation;
        ar& Key;
        ar& Value;
        ar& ClientId;
        ar& RequestId;
    }


};

const std::string OK = "OK";
const std::string ErrNoKey = "ErrNoKey";
const std::string ErrWrongLeader = "ErrWrongLeader";

//��ȡ���ö˿�
bool isReleasePort(unsigned short usPort);

bool getReleasePort(short& port);


#endif