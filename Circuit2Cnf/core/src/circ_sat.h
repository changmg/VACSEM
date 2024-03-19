#include "my_abc.h"
#include "cmdline.hpp"
#include "header.h"


namespace abc {
typedef struct Tas_Par_t_ Tas_Par_t;
struct Tas_Par_t_ {
    // conflict limits
    int           nBTLimit;     // limit on the number of conflicts
    int           nJustLimit;   // limit on the size of justification queue
    // current parameters
    int           nBTThis;      // number of conflicts
    int           nBTThisNc;    // number of conflicts
    int           nJustThis;    // max size of the frontier
    int           nBTTotal;     // total number of conflicts
    int           nJustTotal;   // total size of the frontier
    // activity
    float         VarDecay;     // variable activity decay
    int           VarInc;       // variable increment
    // decision heuristics
    int           fUseActive;   // use most active
    int           fUseHighest;  // use node with the highest ID
    int           fUseLowest;   // use node with the highest ID
    int           fUseMaxFF;    // use node with the largest fanin fanout
    // other
    int           fVerbose;
};

typedef struct Tas_Cls_t_ Tas_Cls_t;
struct Tas_Cls_t_ {
    int           iNext[2];     // beginning of the queue
    int           nLits;        // the number of literals
    int           pLits[0];     // clause literals
};

typedef struct Tas_Sto_t_ Tas_Sto_t;
struct Tas_Sto_t_ {
    int           iCur;         // current position
    int           nSize;        // allocated size
    int *         pData;        // clause information
};

typedef struct Tas_Que_t_ Tas_Que_t;
struct Tas_Que_t_ {
    int           iHead;        // beginning of the queue
    int           iTail;        // end of the queue
    int           nSize;        // allocated size
    Gia_Obj_t **  pData;        // nodes stored in the queue
};

struct Tas_Man_t_ {
    Tas_Par_t     Pars;         // parameters
    Gia_Man_t *   pAig;         // AIG manager
    Tas_Que_t     pProp;        // propagation queue
    Tas_Que_t     pJust;        // justification queue
    Tas_Que_t     pClauses;     // clause queue
    Gia_Obj_t **  pIter;        // iterator through clause vars
    Vec_Int_t *   vLevReas;     // levels and decisions
    Vec_Int_t *   vModel;       // satisfying assignment
    Vec_Ptr_t *   vTemp;        // temporary storage
    // watched clauses
    Tas_Sto_t     pStore;       // storage for watched clauses
    int *         pWatches;     // watched lists for each literal
    Vec_Int_t *   vWatchLits;   // lits whose watched are assigned
    int           nClauses;     // the counter of clauses
    // activity
    float *       pActivity;    // variable activity
    Vec_Int_t *   vActiveVars;  // variables with activity
    // SAT calls statistics
    int           nSatUnsat;    // the number of proofs
    int           nSatSat;      // the number of failure
    int           nSatUndec;    // the number of timeouts
    int           nSatTotal;    // the number of calls
    // conflicts
    int           nConfUnsat;   // conflicts in unsat problems
    int           nConfSat;     // conflicts in sat problems
    int           nConfUndec;   // conflicts in undec problems
    // runtime stats
    abctime       timeSatUnsat; // unsat
    abctime       timeSatSat;   // sat
    abctime       timeSatUndec; // undecided
    abctime       timeTotal;    // total runtime
    // model counting
    // int           maxLevel;
    ll            nModels;
};
}

void CircSatTest(cmdline::parser& opt);