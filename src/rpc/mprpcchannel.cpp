#include "include/mprpcchannel.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include "include/mprpccontroller.h"
#include "rpcheader.pb.h"
#include "../common/include/util.h"

/*
header_size + service_name method_name args_size + args
*/

//�������ֻҪͨ��stub�������rpc��������ô�����ߵ����
//�ú������������ݵ����л��Լ����緢�͵�
//�����done���������һ���ص��������ڹرյ�ʱ��ִ���������
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                              google::protobuf::Message* response, google::protobuf::Closure* done)
{
    if (m_clientFd == -1) {
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        if (!rt) {
            DPrintf("[func-MprpcChannel::CallMethod]������ip��{%s} port{%d}ʧ��", m_ip.c_str(), m_port);
            controller->SetFailed(errMsg);
            return;
        } else {
            DPrintf("[func-MprpcChannel::CallMethod]����ip��{%s} port{%d}�ɹ�", m_ip.c_str(), m_port);
        }
    }

    const google::protobuf::ServiceDescriptor * sd=method->service(); //�õ�һ������������,����֪��һЩ��Ϣ
    std::string service_name = sd->name();    
    std::string method_name = method->name();  

    //��ȡ���������л��ַ����ĳ���
    uint32_t args_size{};
    std::string args_str;
    //�������л����ַ����������ȡ����
    if (request->SerializeToString(&args_str)) {
        args_size = args_str.size();
    } else {
        controller->SetFailed("serialize request error!");
        return;
    }
    RPC::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);
   
    //��ȡͷ����С��Ϣ
    std::string rpc_header_str;
    if (!rpcHeader.SerializeToString(&rpc_header_str)) {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

      //������Ҫ�ǽ�һЩ��Ϣת�������������Է�����淢��������
    std::string send_rpc_str;  // �����洢���շ��͵�����
    {
        // ����һ��StringOutputStream����д��send_rpc_str
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);

        // ��д��header�ĳ��ȣ��䳤���룩
        coded_output.WriteVarint32(static_cast<uint32_t>(rpc_header_str.size()));

       // ����Ҫ�ֶ�д��header_size����Ϊ�����WriteVarint32�Ѿ�������header�ĳ�����Ϣ
        // Ȼ��д��rpc_header����
        coded_output.WriteString(rpc_header_str);
    }

    //��󣬽�����������ӵ�send_rpc_str����
    send_rpc_str+= args_str;
    //���ڿ�ʼ����rpc����,������ʵ�����������string�ַ����������ˣ�������Ϊ��һЩrpc��ͷ����Ϣ����ȻҪʹ�ø�code�������
    while(-1==send(m_clientFd,send_rpc_str.c_str(),send_rpc_str.size(),0) ){
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        std::cout << "�����������ӣ��Է�ip��" << m_ip << " �Է��˿�" << m_port << std::endl;
        close(m_clientFd);
        m_clientFd = -1;
        std::string errMsg;
        bool rt = newConnect(m_ip.c_str(), m_port, &errMsg);
        if (!rt) {
            controller->SetFailed(errMsg);
            return;
        }
    }
    //����������������������һ���ܽ��յ���Ϣ,��ֻ��һ�Σ�����������С�����йأ�)
    //����rpc�������Ӧֵ
    //Ϊʲô����һ��recv�����ˣ�webserver�е��ö�Σ����忴�ĵ���
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientFd, recv_buf, 1024, 0))) {
        close(m_clientFd);
        m_clientFd = -1;
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
    return;
    }
    //�����л�rpc���õ���Ӧ����
    if (!response->ParseFromArray(recv_buf, recv_size)) {
        char errtxt[1050] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
}


bool MprpcChannel::newConnect(const char* ip, uint16_t port, string* errMsg)
{
    //�����ǿͻ��˴���һ��socket����
    // �����׽���
    int clientfd=socket(AF_INET,SOCK_STREAM,0); //��ʾʹ��ipv4�������������͵�Э�飬ʹ��tcp
    if(clientfd==-1){
        char errtxt[512]={0};
        sprintf(errtxt,"create socket fail! errno: %d ",errno);
        m_clientFd=-1;
        *errMsg =errtxt;
        return false;
    }
    //���׽���
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    
    //����rpc������,���ӷ����
    if(-1==connect(clientfd,(struct sockaddr*)&server_addr,sizeof(server_addr) ) ){
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect fail! errno:%d", errno);
        m_clientFd = -1;
        *errMsg = errtxt;
        return false;
    }
    //�ɹ����ÿͻ��˱�ʶ
    m_clientFd=clientfd;
    return true;
}

MprpcChannel::MprpcChannel(string ip, short port, bool connectNow) : m_ip(ip), m_port(port), m_clientFd(-1)
{
    //�ӳ����ӵı����������ǰ���Ǻܽ�������ô���ǿ��Բ��ý������²�������������
    if(!connectNow){
        return ;
    }
    std::string errMsg;
    auto rt = newConnect(ip.c_str(), port, &errMsg);
    int tryCount =3;
    //�����ǰû�����ӻ�����Ҫ�������ӵĻ�����ô����������������3�Σ��������ʧ�ܣ���ô��û�취��
    while(!rt && tryCount--){
        std::cout<<errMsg<<std::endl;
        rt=newConnect(ip.c_str(), port, &errMsg);
    }

}