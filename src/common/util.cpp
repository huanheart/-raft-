#include "include/util.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iomanip>


void myAssert(bool condition,std::string message)  //�Լ�ʵ��һ��С����(�ж�)
{
    if(!condition){
        std::cerr << "Error: "<< message <<std::endl; 
        std::exit(EXIT_FAILURE); //�������˳�
    }
}


//�߾���ʱ�ӣ�now���������̰߳�ȫ��
std::chrono::_V2::system_clock::time_point now() 
{ 
    return std::chrono::high_resolution_clock::now(); 
}




//��ȡ���ѡ�ٳ�ʱ
std::chrono::milliseconds getRandomizedElectionTimeout()
{
    //һ�����������
    std::random_device rd; 
    std::mt19937 rng(rd());
    //std::mt19937 ��һ��α�����������������÷ɭ��ת�㷨��Mersenne Twister�������� std::random_device ���ɵ��������Ϊ����
    //ȷ��ÿ�γ�������ʱ����������в�ͬ��std::mt19937 �ṩ��һ���ܺõ�ƽ�⣬���и�Ч�������ٶȣ����нϳ�������
    
    //���ȷֲ�������һ��min��max��������������ÿһ�����ֵĸ�����ͬ
    std::uniform_int_distribution<int> dist(minRandomizedElectionTime, maxRandomizedElectionTime); //minRandomizedElectionTime������config.h��
    //�����ɵ������ת����ms����һ������
     return std::chrono::milliseconds(dist(rng));
}

void sleepNMilliseconds(int N)  //����һ��˯��
{ 
    std::this_thread::sleep_for(std::chrono::milliseconds(N)); 
};

//�鿴�ĸ��˿ڿ���ʹ��
bool getReleasePort(short &port) 
{
    short num=0;
    while(!isReleasePort(port)&&num<30 ){
        ++port;
        ++num;
    }
    if(num>=30){
        port = -1;
        return false;
    }
    return true;
}


//�жϵ�ǰ�˿��Ƿ�����ʹ��
bool isReleasePort(unsigned short usPort)
{
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(usPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int ret = ::bind(s, (sockaddr *)&addr, sizeof(addr));

    if(ret!=0){ //˵������˿����ڱ�ռ�ã�������ʧ����
        close(s);
        return false;
    }
    close(s);
    return true;
}

// ��ȡ��ǰ�����ڣ�Ȼ��ȡ��־��Ϣ��д����Ӧ����־�ļ����� a+
void DPrintf(const char * format, ...)
{
    if(Debug){
        time_t now=time(nullptr); //��ȡ��ǰʱ��
        tm *nowtm =localtime(&now); //ת���ɱ���ʱ��
        va_list args;     //va_list ���ڷ��ʿɱ亯���еĿɱ����
        va_start(args, format);
        //�����Ӧʱ��
        std::printf("[%d-%d-%d-%d-%d-%d] ", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday, nowtm->tm_hour,
                nowtm->tm_min, nowtm->tm_sec);
        //��������ɱ�����ĺ���
        std::vprintf(format, args);
        std::printf("\n");
        //�����ɱ�����ķ���
        va_end(args);
    }
}
