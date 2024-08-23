#pragma once
#include <google/protobuf/service.h>
#include<string>

class MprpcController : public google::protobuf::RpcController
{
public:
    MprpcController();
    void Reset();
    bool Failed() const;
    std::string ErrorText() const;
    void SetFailed(const std::string & reason);

    // Ŀǰδʵ�־���Ĺ���,������Ҫʵ�ֵĻ��ý�����д�������ܱ���ɹ�
    void StartCancel();
    bool IsCanceled() const;
    void NotifyOnCancel(google::protobuf::Closure* callback);

private:
    bool m_failed;   //rpcִ�й����е�״̬
    std::string m_errText;    //rpcִ�й����еĴ�����Ϣ
};