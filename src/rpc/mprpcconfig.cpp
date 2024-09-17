#include "include/mprpcconfig.h"

#include<iostream>
#include<string>

//ȥ���ַ���ǰ��Ŀո�
void MprpcConfig::Trim(std::string &src_buf)
{
    //��ѯ��һ�������ڴ����е��ַ����ϵ��ַ����ֵ�λ��
    int index=src_buf.find_first_not_of(' ');
    //˵�����ڲ������ַ������е�Ԫ�أ���ôindex�������±�    
    if(index !=-1){
        src_buf=src_buf.substr(index,src_buf.size()-index);
    }
    //ȥ���ַ����������Ŀո�
    index=src_buf.find_last_not_of(' ');
    if(index!=-1){
        src_buf=src_buf.substr(0,index+1);
    }

}

std::string MprpcConfig::Load(const std::string & key)
{
    if(m_configMap.count(key) )
        return m_configMap[key];
    return "";
}

//������������ļ�
void MprpcConfig::LoadConfigFile(const char * config_file)
{
//    std::cout<<"go in"<<std::endl;
    FILE * pf=fopen(config_file,"r");
    if(pf==nullptr){
        std::cout<<config_file<<" is not exist! "<<std::endl;
        exit(EXIT_FAILURE); //��ʾ�Ǳ�׼�˳�
    }
    while(!feof(pf) ) //�ж��ļ��Ƿ񵽴�ĩβ
    {
        char buf[512]={0};
        fgets(buf,512,pf); //��ȡһ�е����ݣ����������������ҪС��һ�У���ô�´λ������ȡͬһ�е����ݣ�ֱ���������з�
        //ȥ��ǰ�󲿷ֶ���Ŀո�
        std::string read_buf(buf);
        Trim(read_buf);

            // �ж�#��ע��
        if (read_buf[0] == '#' || read_buf.empty()) {
            continue;
        }

        // ����������
        int idx = read_buf.find('=');
        if (idx == -1) {
            // ������Ϸ�
            continue;
        }
        std::string key;
        std::string value;
        key=read_buf.substr(0,idx);
        Trim(key);

        int endidx = read_buf.find('\n', idx); //�����һ���ַ���λ�ã���Ϊ��Ϊ���з�����ȻҲ��Ҫ����
        value = read_buf.substr(idx + 1, endidx - idx - 1);
        Trim(value);
        m_configMap.insert({key, value});

    }
    fclose(pf);
}