/*****************************************************************************
*    Open LiteSpeed is an open source HTTP server.                           *
*    Copyright (C) 2013 - 2015  LiteSpeed Technologies, Inc.                 *
*                                                                            *
*    This program is free software: you can redistribute it and/or modify    *
*    it under the terms of the GNU General Public License as published by    *
*    the Free Software Foundation, either version 3 of the License, or       *
*    (at your option) any later version.                                     *
*                                                                            *
*    This program is distributed in the hope that it will be useful,         *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of          *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the            *
*    GNU General Public License for more details.                            *
*                                                                            *
*    You should have received a copy of the GNU General Public License       *
*    along with this program. If not, see http://www.gnu.org/licenses/.      *
*****************************************************************************/
#include <memcache/lsmchashmulti.h>
#include <memcache/lsmemcache.h>
#include <shm/lsshmtidmgr.h>
#include <memcacheconn.h>
#include <log4cxx/logger.h>


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

LsMcHashMulti::LsMcHashMulti()
    : m_iCnt(0)
    , m_fnHashKey(NULL)
    , m_iLastHashKey(0)
    , m_pSlices(NULL)
    , m_pLastSlice(NULL)
    , m_pMultiplexer(NULL)
{
}


LsMcHashMulti::~LsMcHashMulti()
{
    if (m_pSlices != NULL)
    {
        LsMcHashSlice *pSlice = m_pSlices;
        while (m_iCnt > 0)
        {
            pSlice->m_hashByUser.del();
            delete pSlice->m_pConnSlaveToMaster;
            ++pSlice;
            --m_iCnt;
        }
        delete[] m_pSlices;
        m_pSlices = NULL;
    }
}


int  LsMcHashMulti::key2hashNum(LsShmHKey hkey, MemcacheConn *pConn)
{
    if (m_iCnt <= 1)
        return 0;
    if ((LsMemcache::getConfigMultiUser()) &&
        (pConn->getUser()))
    {
        char *pUser = pConn->getUser();
        unsigned int hashNum = XXH32(pUser, strlen(pUser), 0) % m_iCnt;
        LS_DBG_M("key2hashNum return %d for user: %s\n", hashNum, pUser); 
        
        return (int)hashNum;
    }
    unsigned int hashNum = (m_iLastHashKey = hkey) % m_iCnt;
    LS_DBG_M("key2hashNum return %d for hkey: 0x%x\n", hashNum, hkey);
    return (int)hashNum;
}


int LsMcHashMulti::init(int iCnt, const char **ppPathName, 
                        const char *pHashName, LsShmHasher_fn fnHashKey, 
                        LsShmValComp_fn fnValComp, int mode)
{
    char *pDirName;
    char *pShmName;
    LsShm *pShm;
    LsShmPool *pGPool;
    LsMcHashSlice *pSlice;
    char buf[4096];

    if ((iCnt < 1) || (ppPathName == NULL) || (pHashName == NULL))
        return LS_FAIL;
    if (fnHashKey == NULL)
        fnHashKey = LsShmHash::hashXXH32;
    if (fnValComp == NULL)
        fnValComp = memcmp;
    m_fnHashKey = fnHashKey;

    int lstIdx = iCnt - 1 ;
    m_pSlices = new LsMcHashSlice [iCnt];
    pSlice = m_pSlices;
    int ret = LS_OK;
    while (--iCnt >= 0)
    {
        if ((pShmName = strrchr((char *)*ppPathName, '/')) != NULL)
        {
            int cnt;
            if ((cnt = pShmName - *ppPathName) > (int)(sizeof(buf) - 1))
            {
                LS_ERROR("LsShmHashMulti::init Name too big! [%.64s...]!\n",
                    *ppPathName);
                ret = LS_FAIL;
                break;
            }
            memcpy(buf, *ppPathName, cnt);
            buf[cnt] = '\0';
            pDirName = buf;
            ++pShmName;
        }
        else
        {
            pDirName = NULL;
            pShmName = (char *)*ppPathName;
        }
        LS_DBG_M("McHashMulti init, ShmName: %s, HashName: %s, slice: %d\n", 
                 pShmName, pHashName, lstIdx - iCnt);
        if ((pShm = LsShm::open(pShmName, 0, pDirName)) == NULL)
        {
            LS_ERROR("LsShm::open [%s] failed!\n", *ppPathName);
            ret = LS_FAIL;
            break;
        }

        if ((pGPool = pShm->getGlobalPool()) == NULL)
        {
            LS_ERROR("getGlobalPool failed! [%s]\n", *ppPathName);
            ret = LS_FAIL;
            break;
        }

        if (!(pSlice->m_hashByUser.init(pShm, pGPool, pHashName, 
                                        fnHashKey, fnValComp, mode)))
        {
            ret = LS_FAIL;
            break;
        }
        pSlice->m_idx = lstIdx - iCnt;
        LS_DBG_M(" LsMcHashMulti::init m_idx:%d", pSlice->m_idx);
        pSlice->m_iHdrOff = 0;
        pSlice->m_pConnSlaveToMaster = NULL;

        m_pLastSlice = pSlice;
        ++ppPathName;
        ++pSlice;
        ++m_iCnt;
    }

    if (ret != LS_OK)
    {
        while (m_iCnt > 0)
        {
            (--pSlice)->m_hashByUser.del();
            --m_iCnt;
        }
        delete[] m_pSlices;
        m_pSlices = NULL;
    }
    return ret;
}


int LsMcHashMulti::foreach(
    int (*func)(LsMcHashSlice *pSlice, MemcacheConn *pConn, void *pArg), 
    MemcacheConn *pConn, void *pArg)
{
    int cnt = m_iCnt;
    LsMcHashSlice *pSlice = &m_pSlices[0];
    while (--cnt >= 0)
    {
        if ((*func)(pSlice, pConn, pArg) == LS_FAIL)
            return LS_FAIL;
        ++pSlice;
    }
    return m_iCnt;
}
