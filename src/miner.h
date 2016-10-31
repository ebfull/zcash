// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "chainparams.h"
#include "primitives/block.h"
#include "txmempool.h"

#include <stdint.h>

class CBlockIndex;
class CReserveKey;
class CScript;
class CWallet;
namespace Consensus { struct Params; };

struct CBlockTemplate
{
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

/** Run the miner threads */
void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads);
/** Fill a new block with transactions */
void FillNewBlock(CBlock* pblock,
                  CBlockIndex* pindexPrev,
                  const CScript& scriptPubKeyIn,
                  const CChainParams& chainparams,
                  std::vector<CAmount>& vTxFees,
                  std::vector<int64_t>& vTxSigOps,
                  std::map<uint256, CTxMemPoolEntry>& mapTx,
                  std::function<void(const uint256, double&, CAmount&)> mempoolApplyDeltas,
                  unsigned int nBlockMaxSize,
                  unsigned int nBlockPrioritySize,
                  unsigned int nBlockMinSize);
/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey);
/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
void UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev);

#endif // BITCOIN_MINER_H
