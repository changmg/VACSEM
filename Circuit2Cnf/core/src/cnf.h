#pragma once

#include "my_abc.h"


void My_NtkDarToCnf(abc::Abc_Ntk_t * pNtk, char * pFileName, int fFastAlgo, int fChangePol, int fVerbose);
NetMan BuildMit(NetMan& accNet, NetMan& appNet, NetMan& mitNet);
void MiterToCnf(NetMan& mit, std::string outp, int iOutp = -1);