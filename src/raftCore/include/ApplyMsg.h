#ifndef APPLYMSG_H
#define APPLYMSG_H

#include<string>


//������װ��һЩ������Ϣ������˵����������������־�±갡ʲô��
class ApplyMsg
{
public:
    bool CommandValid;
    std::string Command;
    int CommandIndex;
    bool SnapshotValid;
    std::string Snapshot;
    int SnapshotTerm;
    int SnapshotIndex;

public:
    ApplyMsg()
        : CommandValid(false),
          Command(),
          CommandIndex(-1),
          SnapshotValid(false),
          SnapshotTerm(-1),
          SnapshotIndex(-1){

        };         


};

#endif