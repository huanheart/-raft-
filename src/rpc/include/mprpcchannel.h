#ifndef MPRPCCHANNEL_H
#define MPRPCCHANNEL_H

// #include <google/protobuf/descriptor.h>
// #include <google/protobuf/message.h>
// #include <google/protobuf/service.h>
// #include <algorithm>
// #include <algorithm>  // ���� std::generate_n() �� std::generate() ������ͷ�ļ�
// #include <functional>
// #include <iostream>
// #include <map>
// #include <random>  // ���� std::uniform_int_distribution ���͵�ͷ�ļ�
// #include <string>
// #include <unordered_map>
// #include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <algorithm>
#include <algorithm>  // ���� std::generate_n() �� std::generate() ������ͷ�ļ�
#include <functional>
#include <iostream>
#include <map>
#include <random>  // ���� std::uniform_int_distribution ���͵�ͷ�ļ�
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class MprpcChannel : public google::protobuf::RpcChannel{
public:
  void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request, google::protobuf::Message *response,
                  google::protobuf::Closure *done) override;
    MprpcChannel(string ip,short port,bool connectNow);
private:
    int m_clientFd;
    const std::string m_ip; //����ip�Ͷ˿ڣ��Է�����˿��Խ���һ�������Ĳ���
    const uint16_t m_port;
  /// @brief ����ip�Ͷ˿�,������m_clientFd
  /// @param ip ip��ַ�������ֽ���
  /// @param port �˿ڣ������ֽ���
  /// @return �ɹ����ؿ��ַ��������򷵻�ʧ����Ϣ
  bool newConnect(const char *ip, uint16_t port, string *errMsg);
};

#endif
