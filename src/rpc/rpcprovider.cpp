#include "include/rpcprovider.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <string>
#include "rpcheader.pb.h"
#include "../common/include/util.h"

//���ڷ�������Ľӿ�(����ע������ڴ浱�У�����������ע�����ģ��������ܻ�ͨ�������֪����Щ�ӿڣ���Щ�����ܱ�ʹ��)
void RpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info; //���ڽ��շ����һЩ��Ϣ����

    //��ȡ��������������Ϣ
    const google::protobuf::ServiceDescriptor * pserviceDesc =service->GetDescriptor();

    //��ȡ���������
    std::string service_name=pserviceDesc->name();
    //��ȡ�������service�ķ���������,����������ж��ٸ��ӿڿ��Ը����ǵ���?
    int method_cnt=pserviceDesc->method_count();

    std::cout << "service_name:" << service_name << std::endl;
    for(int i=0;i<method_cnt;++i){
        //��ȡÿһ������Ľӿ���Ϣ��Ȼ��浽��ϣ����
        const google::protobuf::MethodDescriptor * pmethodDesc = pserviceDesc->method(i);
        std::string method_name=pmethodDesc->name();
        service_info.m_methodMap.insert( {method_name,pmethodDesc} );
    }
    service_info.m_service=service;
    m_serviceMap.insert( {service_name,service_info} );
}

//��һ�����񱻷���֮�󣬻���Ҫ�������˲���ʹ��
void RpcProvider::Run(int nodeIndex,short port)
{
    char * ipC;
    char hname[128];
    struct hostent* hent;
    gethostname(hname,sizeof(hname) ); //��ȡ��������
    hent=gethostbyname(hname); //��������������ƻ�ȡip��ַ
    for(int i=0;hent->h_addr_list[i];i++){
        ipC = inet_ntoa(*(struct in_addr *)(hent->h_addr_list[i]) );  // IP��ַ
    }

    std::string ip=std::string(ipC);

    //д���ļ� "test.conf"
    std::string node="node"+std::to_string(nodeIndex);
    std::ofstream outfile;
    outfile.open("test.conf",std::ios::app); //���ļ���׷��д��
    if(!outfile.is_open()){
        std::cout<<"file open failed 54"<<std::endl;
        exit(EXIT_FAILURE); //�������˳�
    }
    outfile << node + "ip=" + ip << std::endl;
    outfile << node + "port=" + std::to_string(port) << std::endl;
    outfile.close();

    //InetAddress������ip��ַ�Լ��˿�
    muduo::net::InetAddress address(ip,port);

    // ����TcpServer����
    m_muduo_server = std::make_shared<muduo::net::TcpServer>(&m_eventLoop, address, "RpcProvider");
 // �����ӻص�����Ϣ��д�ص�����  ��������������ҵ�����
  /*
  bind�����ã�
  �����ʹ��std::bind���ص�������TcpConnection���������
  ��ô�ڻص������о��޷�ֱ�ӷ��ʺ��޸�TcpConnection�����״̬��
  ��Ϊ�ص���������Ϊһ�������ĺ��������õģ���û�е�ǰ�������������Ϣ����thisָ�룩,
  Ҳ���޷�ֱ�ӷ��ʵ�ǰ�����״̬��
  ���Ҫ�ڻص������з��ʺ��޸�TcpConnection�����״̬����Ҫͨ����������ʽ����ǰ�����ָ�봫�ݽ�ȥ��
  ���ұ�֤�ص������ڵ�ǰ����������Ļ����б����á����ַ�ʽ�Ƚϸ��ӣ����׳���Ҳ�����ڴ���ı�д��ά����
  ��ˣ�ʹ��std::bind���ص�������TcpConnection��������������Ը��ӷ��㡢ֱ�۵ط��ʺ��޸Ķ����״̬��
  ͬʱҲ���Ա���һЩ�����Ĵ���
  */
    //�������ӵ�ʱ��Ļص�
    m_muduo_server->setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    //�����ж�д�¼�������ʱ��Ļص�
    m_muduo_server->setMessageCallback(
    std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    //���ø�muduo������ж��ٸ��߳�
    m_muduo_server->setThreadNum(4);
    //rpc�����׼����������ӡ��Ϣ
    std::cout<<" RpcProvider start service at ip "<<ip<<" port: "<<port<<std::endl;

    //�����������
    m_muduo_server->start();
    m_eventLoop.loop(); //�����¼�ѭ��
  /*
  ��δ��������������������¼�ѭ��������server��һ��TcpServer����m_eventLoop��һ��EventLoop����
    ���ȵ���server.start()�����������������Muduo���У�TcpServer���װ�˵ײ��������
    ����TCP���ӵĽ����͹رա����տͻ������ݡ��������ݸ��ͻ��˵ȵȡ�ͨ������TcpServer�����start����
    ���������ײ�������񲢼����ͻ������ӵĵ�����
    ����������m_eventLoop.loop()���������¼�ѭ������Muduo���У�EventLoop���װ���¼�ѭ���ĺ����߼�
    ������ʱ����IO�¼����źŵȵȡ�ͨ������EventLoop�����loop���������������¼�ѭ�����ȴ��¼��ĵ����������¼���
    ����δ����У����������������Ȼ������¼�ѭ���׶Σ��ȴ�����������¼������������¼�ѭ����������Զ�����ģ��
    ���ǵ�����˳��͵��÷�ʽ����ȷ���ġ������������ͨ�������¼�ѭ��֮ǰ����Ϊ����������¼�ѭ���Ļ�����
    �����¼�ѭ����������Ӧ�ó���ĺ��ģ����е��¼������¼�ѭ���б�����
  */
}

//�µ����ӵ�ʱ��Ļص�����

void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr & conn)
{
    //�������Ϊ�ڴ������ӵ�ʱ����������⣬����û�������ϣ����󴥷�����ص�������һЩ���������٣�
    if(!conn->connected() ){
        conn->shutdown();
    }
}

// �ѽ��������û��Ķ�д�¼��ص� ���Զ����һ��rpc����ĵ���������ôOnMessage�����ͻ���Ӧ
// �������Ŀ϶���һ��Զ�̵�������
// ��˱�������Ҫ���������󣬸��ݷ���������������������������service����callmethod�����ñ��ص�ҵ��
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp)
{
    //����������(��ȡ����������������Ȼ�󲢷���һ���ַ���)
    std::string recv_buf=buffer->retrieveAllAsString();

    //ʹ��protobuf��CodedInputStream������������
    google::protobuf::io::ArrayInputStream array_input(recv_buf.data(), recv_buf.size());
    google::protobuf::io::CodedInputStream coded_input(&array_input);
    uint32_t header_size{};

    coded_input.ReadVarint32(&header_size);  // ���������ö�ȡ�����ͣ���int32���͵�

    // ����header_size��ȡ����ͷ��ԭʼ�ַ����������л����ݣ��õ�rpc�������ϸ��Ϣ
    std::string rpc_header_str;
    RPC::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;

// ����header_size����ֹ��ȡ��������ݣ�����ֹ����������Ķ���ʲô�ģ���ȷ��֤��ȡ��ͷ����Ϣ��
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);
    coded_input.ReadString(&rpc_header_str,header_size);
    //�ָ�֮ǰ�����ƣ��Ա㰲ȫ�ؼ�����ȡ��������,�������Զ�ȡ����head_size�Ĵ�С�ˣ������Զ�ȡ�����岿��������

    coded_input.PopLimit(msg_limit);
    uint32_t args_size{};

    //��ʼ�����л�(��Ҫ��ͷ����һЩ��Ϣ�ó���)
    if(rpcHeader.ParseFromString(rpc_header_str) ){
        service_name=rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }else{
            // ����ͷ�����л�ʧ��
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }
    // Ȼ�����ó�����ȡ�����������
    std::string args_str;
    // ֱ�Ӷ�ȡargs_size���ȵ��ַ�������(ע�⣬��ʱ�����û�����л�)
    bool read_args_success = coded_input.ReadString(&args_str, args_size);

    if(!read_args_success){
        std::cout<<"read data fail"<<std::endl;
        return ;
    }
      // ��ȡservice�����method����
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end()) {
        std::cout << "����" << service_name << " is not exist!" << std::endl;
        std::cout << "��ǰ�Ѿ��еķ����б�Ϊ:";
        for (auto item : m_serviceMap) {
        std::cout << item.first << " ";
        }
        std::cout << std::endl;
        return;
    }
    //�Ҷ�Ӧ������ĳһ������ķ���
    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end()) {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        return;
    }
    google::protobuf::Service *service = it->second.m_service;       // ��ȡservice����  new UserService
    const google::protobuf::MethodDescriptor *method = mit->second;  // ��ȡmethod����  Login

    //��װһ��ȫ�µ�request,�������ݵĸ����ԣ�������ʱ����������̰߳�ȫ�������ʹ��ԭ����request����ô����
    //����ͨ���˺�������Ⱦ
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str)) {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();


    // �������method�����ĵ��ã���һ��Closure�Ļص�����
    // closure(����Ϊ�رգ�����������һЩ����)��ִ���걾�ط���֮��ᷢ���Ļص��������Ҫ������л��ͷ���������Ĳ���
    google::protobuf::Closure *done =
    google::protobuf::NewCallback<RpcProvider, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(
    this, &RpcProvider::SendRpcResponse, conn, response);

    //�������÷���
    service->CallMethod(method, nullptr, request, response, done);
}

void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response )
{

    //��Ӧ��ȥ������(������Ҫ�Ǳ��˵���������OnMessage������Ȼ��������Ӧ��ȥ��)
    std::string response_str;
    if(response->SerializeToString(&response_str) ) //�������л�����(�ŵ���Ӧ�ַ�����)   
    {
        //Ȼ�����л��Ľ�����͵�rpc���÷�
        conn->send(response_str);
    }else {
        std::cout << "serialize response_str error!" << std::endl;
    }

}

RpcProvider::~RpcProvider()
{
    //��ӡ��־��˵���÷���ֹͣ��
    std::cout << "[func - RpcProvider::~RpcProvider()]: ip��port��Ϣ��" << m_muduo_server->ipPort() << std::endl;
    m_eventLoop.quit();
}