/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 */

#ifndef epicsTimerPrivate_h
#define epicsTimerPrivate_h

#include "tsFreeList.h"
#include "tsDLList.h"
#include "epicsTimer.h"

#ifdef DEBUG
#   define debugPrintf(ARGSINPAREN) printf ARGSINPAREN
#else
#   define debugPrintf(ARGSINPAREN)
#endif

class timer : public epicsTimer, public tsDLNode < timer > {
public:
    void destroy ();
    void start ( class epicsTimerNotify &, const epicsTime & );
    void start ( class epicsTimerNotify &, double delaySeconds );
    void cancel ();
    expireInfo getExpireInfo () const;
    void show ( unsigned int level ) const;
    class timerQueue & getPrivTimerQueue ();
protected:
    timer ( class timerQueue & );
    ~timer (); 
private:
    enum state { statePending = 45, stateLimbo = 78 };
    epicsTime exp; // experation time 
    state curState; // current state 
    epicsTimerNotify *pNotify; // callback
    timerQueue &queue;
    void privateStart ( epicsTimerNotify & notify, const epicsTime & );
    void privateCancel ();
    timer & operator = ( const timer & );
    friend class timerQueue;
};

struct epicsTimerForC : public epicsTimerNotify, public timer {
public:
    void destroy ();
protected:
    epicsTimerForC ( timerQueue &, epicsTimerCallback, void *pPrivateIn );
    ~epicsTimerForC (); 
private:
    epicsTimerCallback pCallBack;
    void * pPrivate;
    expireStatus expire ( const epicsTime & currentTime );
    epicsTimerForC & operator = ( const epicsTimerForC & );
    friend class timerQueue;
};

class timerQueue : public epicsTimerQueue {
public:
    timerQueue ( epicsTimerQueueNotify &notify );
    virtual ~timerQueue ();
    double process ( const epicsTime & currentTime );
    epicsTimer & createTimer ();
    void show ( unsigned int level ) const;
    epicsTimerForC & createTimerForC ( epicsTimerCallback, void *pPrivateIn );
    void destroyTimerForC ( epicsTimerForC & );
private:
    mutable epicsMutex mutex;
    tsFreeList < class timer, 0x20 > timerFreeList;
    tsFreeList < epicsTimerForC, 0x20 > cTimerfreeList;
    epicsEvent cancelBlockingEvent;
    tsDLList < timer > timerList;
    epicsTimerQueueNotify &notify;
    timer *pExpireTmr;
    epicsThreadId processThread;
    bool cancelPending;
	timerQueue ( const timerQueue & );
    timerQueue & operator = ( const timerQueue & );
    friend class timer;
};

class timerQueueActiveMgrPrivate { // X aCC 655
public:
    timerQueueActiveMgrPrivate ();
protected:
    virtual ~timerQueueActiveMgrPrivate () = 0;
private:
    unsigned referenceCount;
    friend class timerQueueActiveMgr;
};

class timerQueueActive : public epicsTimerQueueActive, 
    public epicsThreadRunable, public epicsTimerQueueNotify,
    public timerQueueActiveMgrPrivate {
public:
    timerQueueActive ( bool okToShare, unsigned priority );
    virtual ~timerQueueActive () = 0;
    epicsTimer & createTimer ();
    void show ( unsigned int level ) const;
    bool sharingOK () const;
    unsigned threadPriority () const;
    epicsTimerForC & createTimerForC ( epicsTimerCallback, void *pPrivateIn );
    void destroyTimerForC ( epicsTimerForC & );
private:
    timerQueue queue;
    epicsEvent rescheduleEvent;
    epicsEvent exitEvent;
    epicsThread thread;
    bool okToShare;
    bool exitFlag;
    bool terminateFlag;
    void run ();
    void reschedule ();
    epicsTimerQueue & getEpicsTimerQueue ();
	timerQueueActive ( const timerQueueActive & );
    timerQueueActive & operator = ( const timerQueueActive & );
};

struct epicsTimerQueueActiveForC : public timerQueueActive, 
    public tsDLNode < epicsTimerQueueActiveForC > {
public:
    epicsTimerQueueActiveForC ( bool okToShare, unsigned priority );
    void release ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    virtual ~epicsTimerQueueActiveForC ();
private:
    static tsFreeList < epicsTimerQueueActiveForC > freeList;
    static epicsMutex freeListMutex;
	epicsTimerQueueActiveForC ( const epicsTimerQueueActiveForC & );
    epicsTimerQueueActiveForC & operator = ( const epicsTimerQueueActiveForC & );
};

class timerQueueActiveMgr {
public:
	timerQueueActiveMgr ();
    ~timerQueueActiveMgr ();
    epicsTimerQueueActiveForC & allocate ( bool okToShare, 
        unsigned threadPriority = epicsThreadPriorityMin + 10 );
    void release ( epicsTimerQueueActiveForC & );
private:
    epicsMutex mutex;
    tsDLList < epicsTimerQueueActiveForC > sharedQueueList;
	timerQueueActiveMgr ( const timerQueueActiveMgr & );
    timerQueueActiveMgr & operator = ( const timerQueueActiveMgr & );
};

extern timerQueueActiveMgr queueMgr;

class timerQueuePassive : public epicsTimerQueuePassive {
public:
    timerQueuePassive ( epicsTimerQueueNotify & );
    epicsTimer & createTimer ();
    void show ( unsigned int level ) const;
    double process ( const epicsTime & currentTime );
    epicsTimerForC & createTimerForC ( epicsTimerCallback, void *pPrivateIn );
    void destroyTimerForC ( epicsTimerForC & );
protected:
    timerQueue queue;
    ~timerQueuePassive ();
    epicsTimerQueue & getEpicsTimerQueue ();
	timerQueuePassive ( const timerQueuePassive & );
    timerQueuePassive & operator = ( const timerQueuePassive & );
};

inline epicsTimerForC & timerQueue::createTimerForC 
    ( epicsTimerCallback pCB, void *pPriv )
{
    epicsAutoMutex autoLock ( this->mutex );
    void *pBuf = this->cTimerfreeList.allocate ( sizeof (epicsTimerForC) );
    if ( ! pBuf ) {
        throw std::bad_alloc();
    }
    return * new ( pBuf ) epicsTimerForC ( *this, pCB, pPriv );
}

inline void timerQueue::destroyTimerForC ( epicsTimerForC & tmr )
{
    tmr.~epicsTimerForC ();
    this->cTimerfreeList.release ( &tmr, sizeof ( tmr ) );
}

inline bool timerQueueActive::sharingOK () const
{
    return this->okToShare;
}

inline unsigned timerQueueActive::threadPriority () const
{
    return thread.getPriority ();
}

inline void * epicsTimerQueueActiveForC::operator new ( size_t size )
{ 
    epicsAutoMutex locker ( epicsTimerQueueActiveForC::freeListMutex );
    return epicsTimerQueueActiveForC::freeList.allocate ( size );
}

inline void epicsTimerQueueActiveForC::operator delete ( void *pCadaver, size_t size )
{ 
    epicsAutoMutex locker ( epicsTimerQueueActiveForC::freeListMutex );
    epicsTimerQueueActiveForC::freeList.release ( pCadaver, size );
}

#endif // epicsTimerPrivate_h

