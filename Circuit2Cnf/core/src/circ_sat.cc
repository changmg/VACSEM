#include "circ_sat.h"


using std::string;
using std::cout;
using std::endl;
using std::stack;
using abc::Vec_Int_t;
using abc::Vec_Str_t;
using abc::Gia_Man_t;
using abc::Gia_Obj_t;
using abc::abctime;
using abc::Tas_Man_t;
using abc::Tas_Que_t;
using abc::Tas_Cls_t;


#define Tas_QueForEachEntry( Que, pObj, i )                         \
    for ( i = (Que).iHead; (i < (Que).iTail) && ((pObj) = (Que).pData[i]); i++ )
#define Tas_ClauseForEachVar( p, hClause, pObj )                    \
    for ( (p)->pIter = (p)->pClauses.pData + hClause; (pObj = *pIter); (p)->pIter++ )
#define Tas_ClauseForEachVar1( p, hClause, pObj )                   \
    for ( (p)->pIter = (p)->pClauses.pData+hClause+1; (pObj = *pIter); (p)->pIter++ )


static inline int   Tas_VarIsAssigned( Gia_Obj_t * pVar )      { return pVar->fMark0;                        }
static inline void  Tas_VarAssign( Gia_Obj_t * pVar )          { assert(!pVar->fMark0); pVar->fMark0 = 1;    }
static inline void  Tas_VarUnassign( Gia_Obj_t * pVar )        { assert(pVar->fMark0);  pVar->fMark0 = 0; pVar->fMark1 = 0; pVar->Value = ~0; }
static inline int   Tas_VarValue( Gia_Obj_t * pVar )           { assert(pVar->fMark0);  return pVar->fMark1; }
static inline void  Tas_VarSetValue( Gia_Obj_t * pVar, int v ) { assert(pVar->fMark0);  pVar->fMark1 = v;    }
static inline int   Tas_VarIsJust( Gia_Obj_t * pVar )          { return Gia_ObjIsAnd(pVar) && !Tas_VarIsAssigned(Gia_ObjFanin0(pVar)) && !Tas_VarIsAssigned(Gia_ObjFanin1(pVar)); } 
static inline int   Tas_VarFanin0Value( Gia_Obj_t * pVar )     { return !Tas_VarIsAssigned(Gia_ObjFanin0(pVar)) ? 2 : (Tas_VarValue(Gia_ObjFanin0(pVar)) ^ Gia_ObjFaninC0(pVar)); }
static inline int   Tas_VarFanin1Value( Gia_Obj_t * pVar )     { return !Tas_VarIsAssigned(Gia_ObjFanin1(pVar)) ? 2 : (Tas_VarValue(Gia_ObjFanin1(pVar)) ^ Gia_ObjFaninC1(pVar)); }
static inline int   Tas_VarToLit( Tas_Man_t * p, Gia_Obj_t * pObj ) { assert( Tas_VarIsAssigned(pObj) ); return abc::Abc_Var2Lit( Gia_ObjId(p->pAig, pObj), !Tas_VarValue(pObj) );     }
static inline int   Tas_LitIsTrue( Gia_Obj_t * pObj, int Lit ) { assert( Tas_VarIsAssigned(pObj) ); return Tas_VarValue(pObj) != abc::Abc_LitIsCompl(Lit);                             }

static inline int         Tas_ClsHandle( Tas_Man_t * p, Tas_Cls_t * pClause ) { return ((int *)pClause) - p->pStore.pData;   }
static inline Tas_Cls_t * Tas_ClsFromHandle( Tas_Man_t * p, int h )           { return (Tas_Cls_t *)(p->pStore.pData + h);   }

static inline int         Tas_VarDecLevel( Tas_Man_t * p, Gia_Obj_t * pVar )  { assert( pVar->Value != ~0 ); return Vec_IntEntry(p->vLevReas, 3*pVar->Value);          }
static inline Gia_Obj_t * Tas_VarReason0( Tas_Man_t * p, Gia_Obj_t * pVar )   { assert( pVar->Value != ~0 ); return pVar + Vec_IntEntry(p->vLevReas, 3*pVar->Value+1); }
static inline Gia_Obj_t * Tas_VarReason1( Tas_Man_t * p, Gia_Obj_t * pVar )   { assert( pVar->Value != ~0 ); return pVar + Vec_IntEntry(p->vLevReas, 3*pVar->Value+2); }
static inline int         Tas_ClauseDecLevel( Tas_Man_t * p, int hClause )    { return Tas_VarDecLevel( p, p->pClauses.pData[hClause] );                               }

static inline int         Tas_VarHasReasonCls( Tas_Man_t * p, Gia_Obj_t * pVar ) { assert( pVar->Value != ~0 ); return Vec_IntEntry(p->vLevReas, 3*pVar->Value+1) == 0 && Vec_IntEntry(p->vLevReas, 3*pVar->Value+2) != 0; }
static inline Tas_Cls_t * Tas_VarReasonCls( Tas_Man_t * p, Gia_Obj_t * pVar )    { assert( pVar->Value != ~0 ); return Tas_ClsFromHandle( p, Vec_IntEntry(p->vLevReas, 3*pVar->Value+2) );                                 }


static inline int Tas_ManCheckLimits( Tas_Man_t * p )
{
    return p->Pars.nJustThis > p->Pars.nJustLimit || p->Pars.nBTThis > p->Pars.nBTLimit;
}


static inline void Tas_ManSaveModel( Tas_Man_t * p, Vec_Int_t * vCex )
{
    Gia_Obj_t * pVar;
    int i;
    Vec_IntClear( vCex );
    p->pProp.iHead = 0;
//    printf( "\n" );
    Tas_QueForEachEntry( p->pProp, pVar, i )
    {
        if ( Gia_ObjIsCi(pVar) )
//            Vec_IntPush( vCex, Abc_Var2Lit(Gia_ObjId(p->pAig,pVar), !Tas_VarValue(pVar)) );
            Vec_IntPush( vCex, abc::Abc_Var2Lit(Gia_ObjCioId(pVar), !Tas_VarValue(pVar)) );
/*
        printf( "%5d(%d) = ", Gia_ObjId(p->pAig, pVar), Tas_VarValue(pVar) );
        if ( Gia_ObjIsCi(pVar) )
            printf( "pi %d\n", Gia_ObjCioId(pVar) );
        else
        {
            printf( "%5d %d  &  ", Gia_ObjFaninId0p(p->pAig, pVar), Gia_ObjFaninC0(pVar) );
            printf( "%5d %d     ", Gia_ObjFaninId1p(p->pAig, pVar), Gia_ObjFaninC1(pVar) );
            printf( "\n" );
        }
*/
    }
}


static inline void Tas_ManCancelUntil( Tas_Man_t * p, int iBound )
{
    Gia_Obj_t * pVar;
    int i;
    assert( iBound <= p->pProp.iTail );
    p->pProp.iHead = iBound;
    Tas_QueForEachEntry( p->pProp, pVar, i )
        Tas_VarUnassign( pVar );
    p->pProp.iTail = iBound;
    Vec_IntShrink( p->vLevReas, 3*iBound );
}


static inline void Tas_QuePush( Tas_Que_t * p, Gia_Obj_t * pObj ) {
    if ( p->iTail == p->nSize )
    {
        p->nSize *= 2;
        p->pData = ABC_REALLOC( Gia_Obj_t *, p->pData, p->nSize ); 
    }
    p->pData[p->iTail++] = pObj;
}


extern int s_Counter2;
static inline void Tas_ManAssign( Tas_Man_t * p, Gia_Obj_t * pObj, int Level, Gia_Obj_t * pRes0, Gia_Obj_t * pRes1 ) {
    Gia_Obj_t * pObjR = Gia_Regular(pObj);
    assert( Gia_ObjIsCand(pObjR) );
    assert( !Tas_VarIsAssigned(pObjR) );
    Tas_VarAssign( pObjR );
    Tas_VarSetValue( pObjR, !Gia_IsComplement(pObj) );
    assert( pObjR->Value == ~0 );
    pObjR->Value = p->pProp.iTail;
    Tas_QuePush( &p->pProp, pObjR );
    Vec_IntPush( p->vLevReas, Level );
    if ( pRes0 == NULL && pRes1 != 0 ) // clause
    {
        Vec_IntPush( p->vLevReas, 0 );
        Vec_IntPush( p->vLevReas, Tas_ClsHandle( p, (Tas_Cls_t *)pRes1 ) );
    }
    else
    {
        Vec_IntPush( p->vLevReas, pRes0 ? pRes0-pObjR : 0 );
        Vec_IntPush( p->vLevReas, pRes1 ? pRes1-pObjR : 0 );
    }
    assert( Vec_IntSize(p->vLevReas) == 3 * p->pProp.iTail );
    s_Counter2++;
}


static inline int Tas_QueIsEmpty( Tas_Que_t * p )
{
    return p->iHead == p->iTail;
}


static inline Tas_Cls_t * Tas_ManAllocCls( Tas_Man_t * p, int nSize )
{
    Tas_Cls_t * pCls;
    if ( p->pStore.iCur + nSize > p->pStore.nSize )
    {
        p->pStore.nSize *= 2;
        p->pStore.pData  = ABC_REALLOC( int, p->pStore.pData, p->pStore.nSize ); 
    }
    pCls = Tas_ClsFromHandle( p, p->pStore.iCur ); p->pStore.iCur += nSize;
    memset( pCls, 0, sizeof(int) * nSize );
    p->nClauses++;
    return pCls;
}


static inline void Tas_ManWatchClause( Tas_Man_t * p, Tas_Cls_t * pClause, int Lit )
{
    assert( abc::Abc_Lit2Var(Lit) < Gia_ManObjNum(p->pAig) );
    assert( pClause->nLits >= 2 );
    assert( pClause->pLits[0] == Lit || pClause->pLits[1] == Lit );
    if ( pClause->pLits[0] == Lit )
        pClause->iNext[0] = p->pWatches[abc::Abc_LitNot(Lit)];  
    else
        pClause->iNext[1] = p->pWatches[abc::Abc_LitNot(Lit)];  
    if ( p->pWatches[abc::Abc_LitNot(Lit)] == 0 )
        Vec_IntPush( p->vWatchLits, abc::Abc_LitNot(Lit) );
    p->pWatches[abc::Abc_LitNot(Lit)] = Tas_ClsHandle( p, pClause );
}


static inline Tas_Cls_t * Tas_ManCreateCls( Tas_Man_t * p, int hClause )
{
    Tas_Cls_t * pClause;
    Tas_Que_t * pQue = &(p->pClauses);
    Gia_Obj_t * pObj;
    int i, nLits = 0;
    assert( Tas_QueIsEmpty( pQue ) );
    assert( pQue->pData[hClause] != NULL );
    for ( i = hClause; (pObj = pQue->pData[i]); i++ )
        nLits++;
    if ( nLits == 1 )
        return NULL;
    // create this clause
    pClause = Tas_ManAllocCls( p, nLits + 3 );
    pClause->nLits = nLits;
    for ( i = hClause; (pObj = pQue->pData[i]); i++ )
    {
        assert( Tas_VarIsAssigned( pObj ) );
        pClause->pLits[i-hClause] = abc::Abc_LitNot( Tas_VarToLit(p, pObj) );
    }
    // add the clause as watched one
    if ( nLits >= 2 )
    {
        Tas_ManWatchClause( p, pClause, pClause->pLits[0] );
        Tas_ManWatchClause( p, pClause, pClause->pLits[1] );
    }
    // increment activity
//    p->Pars.VarInc /= p->Pars.VarDecay;
    return pClause;
}


static inline void Tas_QueStore( Tas_Que_t * p, int * piHeadOld, int * piTailOld )
{
    int i;
    *piHeadOld = p->iHead;
    *piTailOld = p->iTail;
    for ( i = *piHeadOld; i < *piTailOld; i++ )
        Tas_QuePush( p, p->pData[i] );
    p->iHead = *piTailOld;
}


static inline Gia_Obj_t * Tas_ManFindActive( Tas_Man_t * p )
{
    Gia_Obj_t * pObj, * pObjMax = NULL;
    float BestCost = 0.0;
    int i, ObjId;
    Tas_QueForEachEntry( p->pJust, pObj, i )
    {
        assert( Gia_ObjIsAnd(pObj) );
        ObjId = Gia_ObjId( p->pAig, pObj );
        if ( pObjMax == NULL || 
             p->pActivity[Gia_ObjFaninId0(pObj,ObjId)] > BestCost || 
            (p->pActivity[Gia_ObjFaninId0(pObj,ObjId)] == BestCost && pObjMax < Gia_ObjFanin0(pObj)) )
        {
            pObjMax  = Gia_ObjFanin0(pObj);
            BestCost = p->pActivity[Gia_ObjFaninId0(pObj,ObjId)];
        }
        if ( p->pActivity[Gia_ObjFaninId1(pObj,ObjId)] > BestCost || 
            (p->pActivity[Gia_ObjFaninId1(pObj,ObjId)] == BestCost && pObjMax < Gia_ObjFanin1(pObj)) )
        {
            pObjMax  = Gia_ObjFanin1(pObj);
            BestCost = p->pActivity[Gia_ObjFaninId1(pObj,ObjId)];
        }
    }
    return pObjMax;
}


static inline Gia_Obj_t * Tas_ManDecideHighest( Tas_Man_t * p )
{
    Gia_Obj_t * pObj, * pObjMax = NULL;
    int i;
    Tas_QueForEachEntry( p->pJust, pObj, i )
    {
//printf( "%d %6.2f   ", Gia_ObjId(p->pAig, pObj), p->pActivity[Gia_ObjId(p->pAig, pObj)] );
        if ( pObjMax == NULL || pObjMax < pObj )
            pObjMax = pObj;
    }
//printf( "\n" );
    return pObjMax;
}


static inline Gia_Obj_t * Tas_ManDecideLowest( Tas_Man_t * p )
{
    Gia_Obj_t * pObj, * pObjMin = NULL;
    int i;
    Tas_QueForEachEntry( p->pJust, pObj, i )
        if ( pObjMin == NULL || pObjMin > pObj )
            pObjMin = pObj;
    return pObjMin;
}


static inline int Tas_VarFaninFanoutMax( Tas_Man_t * p, Gia_Obj_t * pObj )
{
    int Count0, Count1;
    assert( !Gia_IsComplement(pObj) );
    assert( Gia_ObjIsAnd(pObj) );
    Count0 = Gia_ObjRefNum( p->pAig, Gia_ObjFanin0(pObj) );
    Count1 = Gia_ObjRefNum( p->pAig, Gia_ObjFanin1(pObj) );
    return abc::Abc_MaxInt( Count0, Count1 );
}


static inline Gia_Obj_t * Tas_ManDecideMaxFF( Tas_Man_t * p )
{
    Gia_Obj_t * pObj, * pObjMax = NULL;
    int i, iMaxFF = 0, iCurFF;
    assert( p->pAig->pRefs != NULL );
    Tas_QueForEachEntry( p->pJust, pObj, i )
    {
        iCurFF = Tas_VarFaninFanoutMax( p, pObj );
        assert( iCurFF > 0 );
        if ( iMaxFF < iCurFF )
        {
            iMaxFF = iCurFF;
            pObjMax = pObj;
        }
    }
    return pObjMax; 
}


static inline void Tas_QueRestore( Tas_Que_t * p, int iHeadOld, int iTailOld )
{
    p->iHead = iHeadOld;
    p->iTail = iTailOld;
}


static inline void Tas_ManDeriveReason( Tas_Man_t * p, int Level )
{
    Tas_Que_t * pQue = &(p->pClauses);
    Gia_Obj_t * pObj, * pReason;
    int i, k, j, iLitLevel, iLitLevel2;//, Id;
    assert( pQue->pData[pQue->iHead] == NULL );
    assert( pQue->iHead + 1 < pQue->iTail );
/*
    for ( i = pQue->iHead + 1; i < pQue->iTail; i++ )
    {
        pObj = pQue->pData[i];
        assert( pObj->fPhase == 0 );
    }
*/
    // compact literals
    Vec_PtrClear( p->vTemp );
    for ( i = k = pQue->iHead + 1; i < pQue->iTail; i++ )
    {
        pObj = pQue->pData[i];
        if ( pObj->fPhase ) // unassigned - seen again
            continue;
        // assigned - seen first time
        pObj->fPhase = 1;
        Vec_PtrPush( p->vTemp, pObj );
        // bump activity
//        Id = Gia_ObjId( p->pAig, pObj );
//        if ( p->pActivity[Id] == 0.0 )
//            Vec_IntPush( p->vActiveVars, Id );
//        p->pActivity[Id] += p->Pars.VarInc;
        // check decision level
        iLitLevel = Tas_VarDecLevel( p, pObj );
        if ( iLitLevel < Level )
        {
            pQue->pData[k++] = pObj;
            continue;
        }
        assert( iLitLevel == Level );
        if ( Tas_VarHasReasonCls( p, pObj ) )
        {
            Tas_Cls_t * pCls = Tas_VarReasonCls( p, pObj );
            pReason = Gia_ManObj( p->pAig, abc::Abc_Lit2Var(pCls->pLits[0]) );
            assert( pReason == pObj );
            for ( j = 1; j < pCls->nLits; j++ )
            {
                pReason = Gia_ManObj( p->pAig, abc::Abc_Lit2Var(pCls->pLits[j]) );
                iLitLevel2 = Tas_VarDecLevel( p, pReason );
                assert( Tas_VarIsAssigned( pReason ) );
                assert( !Tas_LitIsTrue( pReason, pCls->pLits[j] ) );
                Tas_QuePush( pQue, pReason );
            }
        }
        else
        {
            pReason = Tas_VarReason0( p, pObj );
            if ( pReason == pObj ) // no reason
            {
                assert( pQue->pData[pQue->iHead] == NULL || Level == 0 );
                if ( pQue->pData[pQue->iHead] == NULL )
                    pQue->pData[pQue->iHead] = pObj;
                else
                    Tas_QuePush( pQue, pObj );
                continue;
            }
            Tas_QuePush( pQue, pReason );
            pReason = Tas_VarReason1( p, pObj );
            if ( pReason != pObj ) // second reason
                Tas_QuePush( pQue, pReason );
        }
    }
    assert( pQue->pData[pQue->iHead] != NULL );
    if ( pQue->pData[pQue->iHead] == NULL )
        printf( "Tas_ManDeriveReason(): Failed to derive the clause!!!\n" );
    pQue->iTail = k;
    // clear the marks
    Vec_PtrForEachEntry( Gia_Obj_t *, p->vTemp, pObj, i )
        pObj->fPhase = 0;
}


static inline int Tas_QueFinish( Tas_Que_t * p )
{
    int iHeadOld = p->iHead;
    assert( p->iHead < p->iTail );
    Tas_QuePush( p, NULL );
    p->iHead = p->iTail;
    return iHeadOld;
}


static inline int Tas_ManResolve( Tas_Man_t * p, int Level, int hClause0, int hClause1 )
{
    Tas_Que_t * pQue = &(p->pClauses);
    Gia_Obj_t * pObj;
    int i, LevelMax = -1, LevelCur;
    assert( pQue->pData[hClause0] != NULL );
    assert( pQue->pData[hClause0] == pQue->pData[hClause1] );
/*
    for ( i = hClause0 + 1; (pObj = pQue->pData[i]); i++ )
        assert( pObj->fPhase == 0 );
    for ( i = hClause1 + 1; (pObj = pQue->pData[i]); i++ )
        assert( pObj->fPhase == 0 );
*/
    assert( Tas_QueIsEmpty( pQue ) );
    Tas_QuePush( pQue, NULL );
    for ( i = hClause0 + 1; (pObj = pQue->pData[i]); i++ )
    {
        if ( pObj->fPhase ) // unassigned - seen again
            continue;
        // assigned - seen first time
        pObj->fPhase = 1;
        Tas_QuePush( pQue, pObj );
        LevelCur = Tas_VarDecLevel( p, pObj );
        if ( LevelMax < LevelCur )
            LevelMax = LevelCur;
    }
    for ( i = hClause1 + 1; (pObj = pQue->pData[i]); i++ )
    {
        if ( pObj->fPhase ) // unassigned - seen again
            continue;
        // assigned - seen first time
        pObj->fPhase = 1;
        Tas_QuePush( pQue, pObj );
        LevelCur = Tas_VarDecLevel( p, pObj );
        if ( LevelMax < LevelCur )
            LevelMax = LevelCur;
    }
    for ( i = pQue->iHead + 1; i < pQue->iTail; i++ )
        pQue->pData[i]->fPhase = 0;
    Tas_ManDeriveReason( p, LevelMax );
    return Tas_QueFinish( pQue );
}


ll GetUnssignedCI(Tas_Man_t * p) {
    ll unassignedCI = 0;
    for (int i = 0; i < p->pAig->nObjs; i++) {
        auto pVar = &(p->pAig->pObjs[i]);
        if (abc::Gia_ObjIsCi(pVar)) {
            if (!Tas_VarIsAssigned(pVar))
                unassignedCI++;
        }
    }
    return unassignedCI;
}


extern "C" int Tas_ManPropagate( Tas_Man_t * p, int Level );
// int maxLevel = 0;
int My_Tas_ManSolve_rec( Tas_Man_t * p, int Level ) {
    // if (Level > maxLevel) {
    //     maxLevel = Level;
    //     cout << "level = " << maxLevel << endl;
    // }
    Gia_Obj_t * pVar = NULL, * pDecVar = NULL;
    int hClause, hLearn0, hLearn1;
    int iPropHead, iJustHead, iJustTail;
    // propagate assignments, status = 0
    assert( !Tas_QueIsEmpty(&p->pProp) );
    if ( (hClause = Tas_ManPropagate( p, Level )) )
    {
        Tas_ManCreateCls( p, hClause );
        return hClause;
    }
    // check for satisfying assignment, status = 1
    assert( Tas_QueIsEmpty(&p->pProp) );
    if ( Tas_QueIsEmpty(&p->pJust) ) {
        p->nModels += (1ll << GetUnssignedCI(p));
        return 0;
    }
    // quit using resource limits, status = 2
    p->Pars.nJustThis = abc::Abc_MaxInt( p->Pars.nJustThis, p->pJust.iTail - p->pJust.iHead );
    if ( Tas_ManCheckLimits( p ) ) {
        cout << "out of limit" << endl;
        return 0;
    }
    // remember the state before branching, status = 3
    iPropHead = p->pProp.iHead;
    Tas_QueStore( &p->pJust, &iJustHead, &iJustTail );
    // find the decision variable
    if ( p->Pars.fUseActive )
        pVar = NULL, pDecVar = Tas_ManFindActive( p );
    else if ( p->Pars.fUseHighest )
        pVar = Tas_ManDecideHighest( p );
    else if ( p->Pars.fUseLowest )
        pVar = Tas_ManDecideLowest( p );
    else if ( p->Pars.fUseMaxFF )
        pVar = Tas_ManDecideMaxFF( p );
    else
        assert( 0 );
    // chose decision variable using fanout count
    if ( pVar != NULL )
    {
        assert( Tas_VarIsJust( pVar ) );
        if ( Gia_ObjRefNum(p->pAig, Gia_ObjFanin0(pVar)) > Gia_ObjRefNum(p->pAig, Gia_ObjFanin1(pVar)) )
            pDecVar = Gia_Not(Gia_ObjChild0(pVar));
        else
            pDecVar = Gia_Not(Gia_ObjChild1(pVar));
    }
    // decide on first fanin
    Tas_ManAssign( p, pDecVar, Level+1, NULL, NULL );
    hLearn0 = My_Tas_ManSolve_rec( p, Level+1 );
    // status = 4
    Tas_ManCancelUntil( p, iPropHead );
    Tas_QueRestore( &p->pJust, iJustHead, iJustTail );
    // decide on second fanin
    Tas_ManAssign( p, Gia_Not(pDecVar), Level+1, NULL, NULL );
    hLearn1 = My_Tas_ManSolve_rec( p, Level+1 );
    // status = 5;
    if ( (&(p->pClauses))->pData[hLearn0] != Gia_Regular(pDecVar) )
        return hLearn0;
    // status = 6;
    if ( (&(p->pClauses))->pData[hLearn1] != Gia_Regular(pDecVar) )
        return hLearn1;
    // status = 7;
    hClause = Tas_ManResolve( p, Level, hLearn0, hLearn1 );
    Tas_ManCreateCls( p, hClause );
    p->Pars.nBTThis++;
    return hClause;
}


struct SolverData {
    Gia_Obj_t * pVar, * pDecVar;
    int hClause, hLearn0, hLearn1;
    int iPropHead, iJustHead, iJustTail;
    int status, Level, ret;
    SolverData():
        pVar(nullptr), pDecVar(nullptr), hClause(0), hLearn0(0), hLearn1(0), iPropHead(0), iJustHead(0), iJustTail(0), status(0), Level(0), ret(0) {}
};
int My_Tas_ManSolve_loop( Tas_Man_t * p ) {
    stack <SolverData> _stack;
    _stack.emplace(SolverData());
    while (!_stack.empty()) {
        SolverData & d = _stack.top();
        // cout << d.Level << "," << d.status << endl;
        if (d.status == 0) { // before "propagate assignments"
            assert( !Tas_QueIsEmpty(&p->pProp) );
            if ( (d.hClause = Tas_ManPropagate( p, d.Level )) )
            {
                Tas_ManCreateCls( p, d.hClause );
                _stack.pop(); if (!_stack.empty()) _stack.top().ret = d.hClause;
            }
            d.status = 1;
        }
        else if (d.status == 1) { // before "check for satisfying assignment"
            assert( Tas_QueIsEmpty(&p->pProp) );
            if ( Tas_QueIsEmpty(&p->pJust) ) {
                p->nModels += (1ll << GetUnssignedCI(p));
                // cout << p->nModels << endl;
                _stack.pop(); if (!_stack.empty()) _stack.top().ret = 0;
            }
            d.status = 2;
        }
        else if (d.status == 2) { // before "quit using resource limits"
            p->Pars.nJustThis = abc::Abc_MaxInt( p->Pars.nJustThis, p->pJust.iTail - p->pJust.iHead );
            if ( Tas_ManCheckLimits( p ) ) {
                cout << "out of limit" << endl;
                _stack.pop(); if (!_stack.empty()) _stack.top().ret = 0;
            }
            d.status = 3;
        }
        else if (d.status == 3) { // before "remember the state before branching"
            d.iPropHead = p->pProp.iHead;
            Tas_QueStore( &p->pJust, &d.iJustHead, &d.iJustTail );
            // find the decision variable
            if ( p->Pars.fUseActive )
                d.pVar = NULL, d.pDecVar = Tas_ManFindActive( p );
            else if ( p->Pars.fUseHighest )
                d.pVar = Tas_ManDecideHighest( p );
            else if ( p->Pars.fUseLowest )
                d.pVar = Tas_ManDecideLowest( p );
            else if ( p->Pars.fUseMaxFF )
                d.pVar = Tas_ManDecideMaxFF( p );
            else
                assert( 0 );
            // chose decision variable using fanout count
            if ( d.pVar != NULL )
            {
                assert( Tas_VarIsJust( d.pVar ) );
                if ( Gia_ObjRefNum(p->pAig, Gia_ObjFanin0(d.pVar)) > Gia_ObjRefNum(p->pAig, Gia_ObjFanin1(d.pVar)) )
                    d.pDecVar = Gia_Not(Gia_ObjChild0(d.pVar));
                else
                    d.pDecVar = Gia_Not(Gia_ObjChild1(d.pVar));
            }
            // decide on first fanin
            Tas_ManAssign( p, d.pDecVar, d.Level+1, NULL, NULL );
            d.status = 4; _stack.push(SolverData()); _stack.top().Level = d.Level + 1;
        }
        else if (d.status == 4) { // before "decide on second fanin"
            d.hLearn0 = d.ret;
            Tas_ManCancelUntil( p, d.iPropHead );
            Tas_QueRestore( &p->pJust, d.iJustHead, d.iJustTail );
            // decide on second fanin
            Tas_ManAssign( p, Gia_Not(d.pDecVar), d.Level+1, NULL, NULL );
            d.status = 5; _stack.push(SolverData()); _stack.top().Level = d.Level + 1;
        }
        else if (d.status == 5) {
            d.hLearn1 = d.ret;
            if ( (&(p->pClauses))->pData[d.hLearn0] != Gia_Regular(d.pDecVar) ) {
                _stack.pop(); if (!_stack.empty()) _stack.top().ret = d.hLearn0;
            }
            d.status = 6;
        }
        else if (d.status == 6) {
            if ( (&(p->pClauses))->pData[d.hLearn1] != Gia_Regular(d.pDecVar) ) {
                _stack.pop(); if (!_stack.empty()) _stack.top().ret = d.hLearn1;
            }
            d.status = 7;
        }
        else if (d.status == 7) {
            d.hClause = Tas_ManResolve( p, d.Level, d.hLearn0, d.hLearn1 );
            Tas_ManCreateCls( p, d.hClause );
            p->Pars.nBTThis++;
            d.status = 8; _stack.pop(); if (!_stack.empty()) _stack.top().ret = d.hClause;
        }
        else
            assert(0);
    }
    return 0;
}


int My_Tas_ManSolve( Tas_Man_t * p, Gia_Obj_t * pObj, Gia_Obj_t * pObj2 ) {
    int i, Entry, RetValue = 0;
    s_Counter2 = 0;
    Vec_IntClear( p->vModel );
    if ( pObj == Gia_ManConst0(p->pAig) || pObj2 == Gia_ManConst0(p->pAig) || pObj == Gia_Not(pObj2) )
        return 1;
    if ( pObj == Gia_ManConst1(p->pAig) && (pObj2 == NULL || pObj2 == Gia_ManConst1(p->pAig)) )
        return 0;
    assert( !p->pProp.iHead && !p->pProp.iTail );
    assert( !p->pJust.iHead && !p->pJust.iTail );
    assert( p->pClauses.iHead == 1 && p->pClauses.iTail == 1 );
    p->Pars.nBTThis = p->Pars.nJustThis = p->Pars.nBTThisNc = 0;
    Tas_ManAssign( p, pObj, 0, NULL, NULL );
    if ( pObj2 && !Tas_VarIsAssigned(Gia_Regular(pObj2)) )
        Tas_ManAssign( p, pObj2, 0, NULL, NULL );
    if ( My_Tas_ManSolve_rec(p, 0) )
    // if (My_Tas_ManSolve_loop(p))
        RetValue = 1;
    Tas_ManCancelUntil( p, 0 );
    p->pJust.iHead = p->pJust.iTail = 0;
    p->pClauses.iHead = p->pClauses.iTail = 1;
    // clauses
    if ( p->nClauses > 0 )
    {
        p->pStore.iCur = 16;
        Vec_IntForEachEntry( p->vWatchLits, Entry, i )
            p->pWatches[Entry] = 0;
        Vec_IntClear( p->vWatchLits );
        p->nClauses = 0;
    }
    // activity
    Vec_IntForEachEntry( p->vActiveVars, Entry, i )
        p->pActivity[Entry] = 0.0;
    Vec_IntClear( p->vActiveVars );
    // statistics
    p->Pars.nBTTotal += p->Pars.nBTThis;
    p->Pars.nJustTotal = abc::Abc_MaxInt( p->Pars.nJustTotal, p->Pars.nJustThis );
    if ( Tas_ManCheckLimits( p ) )
        RetValue = -1;
    return RetValue;
}


#define MY_ABC_PRT(a,t)    (Abc_Print(1, "%s =", (a)), Abc_Print(1, "%9.6f sec\n", 1.0*((double)(t))/((double)CLOCKS_PER_SEC)))
#define MY_ABC_PRTP(a,t,T) (Abc_Print(1, "%s =", (a)), Abc_Print(1, "%9.6f sec (%6.6f %%)\n", 1.0*((double)(t))/((double)CLOCKS_PER_SEC), ((double)(T))? 100.0*((double)(t))/((double)(T)) : 0.0))
namespace abc{
void My_Tas_ManSatPrintStats( Tas_Man_t * p )
{
    printf( "CO = %8d  ", Gia_ManCoNum(p->pAig) );
    printf( "AND = %8d  ", Gia_ManAndNum(p->pAig) );
    printf( "Conf = %6d  ", p->Pars.nBTLimit );
    printf( "JustMax = %5d  ", p->Pars.nJustLimit );
    printf( "\n" );
    printf( "Unsat calls %6d  (%6.2f %%)   Ave conf = %8.1f   ", 
        p->nSatUnsat, p->nSatTotal? 100.0*p->nSatUnsat/p->nSatTotal :0.0, p->nSatUnsat? 1.0*p->nConfUnsat/p->nSatUnsat :0.0 );
    MY_ABC_PRTP( "Time", p->timeSatUnsat, p->timeTotal );
    printf( "Sat   calls %6d  (%6.2f %%)   Ave conf = %8.1f   ", 
        p->nSatSat,   p->nSatTotal? 100.0*p->nSatSat/p->nSatTotal :0.0, p->nSatSat? 1.0*p->nConfSat/p->nSatSat : 0.0 );
    MY_ABC_PRTP( "Time", p->timeSatSat,   p->timeTotal );
    printf( "Undef calls %6d  (%6.2f %%)   Ave conf = %8.1f   ", 
        p->nSatUndec, p->nSatTotal? 100.0*p->nSatUndec/p->nSatTotal :0.0, p->nSatUndec? 1.0*p->nConfUndec/p->nSatUndec : 0.0 );
    MY_ABC_PRTP( "Time", p->timeSatUndec, p->timeTotal );
    MY_ABC_PRT( "Total time", p->timeTotal );
}
}


extern "C" {
void Gia_ManCollectTest( Gia_Man_t * pAig );
void Cec_ManSatAddToStore( Vec_Int_t * vCexStore, Vec_Int_t * vCex, int Out );
}
Vec_Int_t * My_Tas_ManSolveMiterNc( Gia_Man_t * pAig, int nConfs, Vec_Str_t ** pvStatus, int fVerbose ) {
    abc::Tas_Man_t * p; 
    Vec_Int_t * vCex, * vVisit, * vCexStore;
    Vec_Str_t * vStatus;
    Gia_Obj_t * pRoot;//, * pRootCopy; 
//    Gia_Man_t * pAigCopy = Gia_ManDup( pAig ), * pAigTemp;

    int i, status;
    abctime clk, clkTotal = abc::Abc_Clock();
    assert( Gia_ManRegNum(pAig) == 0 );
//    Gia_ManCollectTest( pAig );
    // prepare AIG
    Gia_ManCreateRefs( pAig );
    Gia_ManCleanMark0( pAig );
    Gia_ManCleanMark1( pAig );
    Gia_ManFillValue( pAig ); // maps nodes into trail ids
    Gia_ManCleanPhase( pAig ); // maps nodes into trail ids
    // create logic network
    p = Tas_ManAlloc( pAig, nConfs );
    p->pAig   = pAig;
    // create resulting data-structures
    vStatus   = abc::Vec_StrAlloc( Gia_ManPoNum(pAig) );
    vCexStore = abc::Vec_IntAlloc( 10000 );
    vVisit    = abc::Vec_IntAlloc( 100 );
    vCex      = abc::Tas_ReadModel( p );
    // solve for each output
    Gia_ManForEachCo( pAig, pRoot, i )
    {
//        printf( "%d=", i );

        Vec_IntClear( vCex );
        if ( Gia_ObjIsConst0(Gia_ObjFanin0(pRoot)) )
        {
            if ( Gia_ObjFaninC0(pRoot) )
            {
//                printf( "Constant 1 output of SRM!!!\n" );
                Cec_ManSatAddToStore( vCexStore, vCex, i ); // trivial counter-example
                Vec_StrPush( vStatus, 0 );
            }
            else
            {
//                printf( "Constant 0 output of SRM!!!\n" );
                Vec_StrPush( vStatus, 1 );
            }
            continue;
        } 
        clk = abc::Abc_Clock();
//        p->Pars.fUseActive  = 1;
        p->Pars.fUseHighest = 1;
        p->Pars.fUseLowest  = 0;
        p->nModels = 0;
        status = My_Tas_ManSolve( p, Gia_ObjChild0(pRoot), NULL );
        cout << "#models = " << p->nModels << endl;
//        printf( "\n" );
/*
        if ( status == -1 )
        {
            p->Pars.fUseHighest = 0;
            p->Pars.fUseLowest  = 1;
            status = Tas_ManSolve( p, Gia_ObjChild0(pRoot) );
        }
*/
        Vec_StrPush( vStatus, (char)status );
        if ( status == -1 )
        {
//            printf( "Unsolved %d.\n", i );

            p->nSatUndec++;
            p->nConfUndec += p->Pars.nBTThis;
            Cec_ManSatAddToStore( vCexStore, NULL, i ); // timeout
            p->timeSatUndec += abc::Abc_Clock() - clk;
            continue;
        }

//        pRootCopy = Gia_ManCo( pAigCopy, i );
//        pRootCopy->iDiff0 = Gia_ObjId( pAigCopy, pRootCopy );
//        pRootCopy->fCompl0 = 0;

        if ( status == 1 )
        {
            p->nSatUnsat++;
            p->nConfUnsat += p->Pars.nBTThis;
            p->timeSatUnsat += abc::Abc_Clock() - clk;
            continue;
        }
        p->nSatSat++;
        p->nConfSat += p->Pars.nBTThis;
//        Gia_SatVerifyPattern( pAig, pRoot, vCex, vVisit );
        Cec_ManSatAddToStore( vCexStore, vCex, i );
        p->timeSatSat += abc::Abc_Clock() - clk;

//        printf( "%d ", Vec_IntSize(vCex) );
    }
//    pAigCopy = Gia_ManCleanup( pAigTemp = pAigCopy );
//    Gia_ManStop( pAigTemp );
//    Gia_DumpAiger( pAigCopy, "test", 0, 2 );
//    Gia_ManStop( pAigCopy );

    Vec_IntFree( vVisit );
    p->nSatTotal = Gia_ManPoNum(pAig);
    p->timeTotal = abc::Abc_Clock() - clkTotal;
    if ( fVerbose )
        My_Tas_ManSatPrintStats( p );
//    printf( "RecCalls = %8d.  RecClause = %8d.  RecNonChro = %8d.\n", p->nRecCall, p->nRecClause, p->nRecNonChro );
    Tas_ManStop( p );
    *pvStatus = vStatus;

//    printf( "Total number of cex literals = %d. (Ave = %d)\n", 
//         Vec_IntSize(vCexStore)-2*p->nSatUndec-2*p->nSatSat, 
//        (Vec_IntSize(vCexStore)-2*p->nSatUndec-2*p->nSatSat)/p->nSatSat );
    return vCexStore;
}


void CircSatTest(cmdline::parser& opt) {
    AbcMan abc;
    abc.Comm("miter " + opt.get<string>("exact") + " " + opt.get<string>("appr"));
    // abc.Synth(ORIENT::AREA, true);
    abc.Synth(ORIENT::AREA, true);
    abc.Comm("logic; aig;");
    // auto pNtk = abc.GetNet();
    // auto pPo = abc::Abc_NtkPo(pNtk, 0);
    // auto pDriv = abc::Abc_ObjFanin0(pPo);
    // abc::Abc_NtkDeleteObj(pPo);
    // auto pInv = abc::Abc_NtkCreateNodeInv(pNtk, pDriv);
    // auto pNewPo = abc::Abc_NtkCreatePo(pNtk);
    // abc::Abc_ObjAddFanin(pNewPo, pInv);
    abc.Comm("&get");

    const int nBTLimit = 1000000;
    const int fVerbose = 1;
    Vec_Int_t * vCounters;
    Vec_Str_t * vStatus;
    vCounters = My_Tas_ManSolveMiterNc(abc.GetAbcFrame()->pGia, nBTLimit, &vStatus, fVerbose);
    Vec_IntFree( vCounters );
    Vec_StrFree( vStatus );
}