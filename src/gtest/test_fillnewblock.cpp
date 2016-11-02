#include <gtest/gtest.h>

#include "chain.h"
#include "main.h"
#include "miner.h"
#include "primitives/block.h"
#include "script/script.h"

std::function<void(const uint256, double&, CAmount&)> noopApplyDeltas = []
        (const uint256 hash, double &dPriorityDelta, CAmount &nFeeDelta) {
    // No-op
};

#include "test/common_coins.cpp"

TEST(FillNewBlock, EmptyMemPool) {
    SelectParams(CBaseChainParams::MAIN);
    const CChainParams& chainparams = Params();
    CScript scriptDummy = CScript() << OP_TRUE;

    auto prevHash = GetRandHash();
    CBlockIndex indexPrev;
    indexPrev.nHeight = 50;
    indexPrev.phashBlock = &prevHash;

    std::map<uint256, CTxMemPoolEntry> mapTx;

    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
    
    CCoinsViewTest backingtest;
    CCoinsViewCache view(&backingtest);

    FillNewBlock(&block, &indexPrev,
                 scriptDummy, chainparams,
                 vTxFees, vTxSigOps,
                 view,
                 mapTx, noopApplyDeltas,
                 DEFAULT_BLOCK_MAX_SIZE,
                 DEFAULT_BLOCK_PRIORITY_SIZE,
                 DEFAULT_BLOCK_MIN_SIZE);

    EXPECT_EQ(1, block.vtx.size());
    EXPECT_EQ(1, vTxFees.size());
    EXPECT_EQ(1, vTxSigOps.size());

    const CTransaction& cb = block.vtx[0];
    EXPECT_EQ(scriptDummy, cb.vout[0].scriptPubKey);
    EXPECT_EQ(chainparams.GetFoundersRewardScriptAtHeight(51), cb.vout[1].scriptPubKey);

    CAmount subsidy = GetBlockSubsidy(51, chainparams.GetConsensus());
    CAmount fr = subsidy / 5;
    subsidy -= fr;
    EXPECT_EQ(subsidy, cb.vout[0].nValue);
    EXPECT_EQ(fr, cb.vout[1].nValue);

    EXPECT_EQ(0, vTxFees[0]);
    EXPECT_EQ(0, vTxSigOps[0]);
}

TEST(FillNewBlock, PureJoinSplit) {
    SelectParams(CBaseChainParams::MAIN);
    const CChainParams& chainparams = Params();
    CScript scriptDummy = CScript() << OP_TRUE;

    auto prevHash = GetRandHash();
    CBlockIndex indexPrev;
    indexPrev.nHeight = 50;
    indexPrev.phashBlock = &prevHash;

    std::map<uint256, CTxMemPoolEntry> mapTx;

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    JSDescription js;
    js.vpub_new = 10000;
    js.anchor = ZCIncrementalMerkleTree::empty_root();
    js.nullifiers[0] = libzcash::random_uint256();
    js.nullifiers[1] = libzcash::random_uint256();
    mtx.vjoinsplit.push_back(js);
    CTransaction tx(mtx);

    mapTx[tx.GetHash()] = CTxMemPoolEntry(
        tx,
        10000,
        1477741360,
        1,
        50,
        true
    );

    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;

    CCoinsViewTest backingtest;
    CCoinsViewCache view(&backingtest);

    FillNewBlock(&block, &indexPrev,
                 scriptDummy, chainparams,
                 vTxFees, vTxSigOps,
                 view,
                 mapTx, noopApplyDeltas,
                 DEFAULT_BLOCK_MAX_SIZE,
                 DEFAULT_BLOCK_PRIORITY_SIZE,
                 DEFAULT_BLOCK_MIN_SIZE);

    EXPECT_EQ(2, block.vtx.size());
}
