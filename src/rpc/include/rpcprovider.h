#pragma once
// #include <google/protobuf/descriptor.h>
// #include <muduo/net/EventLoop.h>
// #include <muduo/net/InetAddress.h>
// #include <muduo/net/TcpConnection.h>
// #include <muduo/net/TcpServer.h>
// #include <functional>
// #include <string>
// #include <unordered_map>
// #include "google/protobuf/service.h"

#include <google/protobuf/descriptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <functional>
#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"


// (�ƺ��ͻ��˺ͷ���˶����õ�����ࣿ)  ���������������������������ص�

//����ṩ��ר�ŷ���rpc��������������
//����rpc�ͻ��˱���� �����ӣ����rpc�������������ṩһ����ʱ�������ԶϿ��ܾ�û����������ӡ�

class RpcProvider {
public:
    // �����ǿ���ṩ���ⲿʹ�õģ����Է���rpc�����ĺ����ӿ�
    void NotifyService(google::protobuf::Service * service);
    //����rpc�����㣬��ʼ�ṩrpcԶ��������÷���
    void Run(int nodeIndex,short port);

private:
    //����¼�ѭ��
    muduo::net::EventLoop m_eventLoop;
    std::shared_ptr<muduo::net::TcpServer> m_muduo_server;
    
      // service����������Ϣ
    struct ServiceInfo {
        google::protobuf::Service *m_service;                                                     // ����������
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor *> m_methodMap;  // ������񷽷�
    };
    // �洢ע��ɹ��ķ�����������񷽷���������Ϣ
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    //�µ�socket���ӻص�:����һ�����ӻص����������µĿͻ�������ʱ������
    void OnConnection(const muduo::net::TcpConnectionPtr & );

    //�ѽ��������û����¼��ص�:���ڴ����ѽ������ӵ��û��Ķ�д�¼������յ��ͻ��˵�����ʱ����������������������������ݣ���������Ӧ
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);

    //Closure�Ļص��������������л�rpc����Ӧ�����緢��
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);
public:
    ~RpcProvider();
};