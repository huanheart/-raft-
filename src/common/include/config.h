//��ͳ���ٶ�α���ͷ�ļ��ķ�ʽ
#ifndef CONFIG_H
#define CONFIG_H

const bool Debug = true; //Ĭ�Ͽ�����־

const int debugMul = 1;  // ʱ�䵥λ��time.Millisecond����ͬ���绷��rpc�ٶȲ�ͬ�������Ҫ����һ��ϵ��
const int HeartBeatTimeout = 25 * debugMul;  // ����ʱ��һ��Ҫ��ѡ�ٳ�ʱСһ��������
const int ApplyInterval = 10 * debugMul;     //

const int minRandomizedElectionTime = 300 * debugMul;  // ms
const int maxRandomizedElectionTime = 500 * debugMul;  // ms

const int CONSENSUS_TIMEOUT = 500 * debugMul;  // ms

// Э���������

const int FIBER_THREAD_NUM = 1;              // Э�̿����̳߳ش�С
const bool FIBER_USE_CALLER_THREAD = false;  // �Ƿ�ʹ��caller_threadִ�е�������


#endif  // CONFIG_H