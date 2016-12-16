#include "util.h"
#include "zcash/JoinSplit.hpp"
#include "primitives/transaction.h"

using namespace libzcash;

int main(int argc, char **argv)
{
    auto p = ZCJoinSplit::Unopened();
    p->loadVerifyingKey((ZC_GetParamsDir() / "sprout-verifying.key").string());
    p->setProvingKeyPath((ZC_GetParamsDir() / "sprout-proving.key").string());
    p->loadProvingKey();

    // construct a proof.

    for (int i = 0; i < 5; i++) {
        uint256 anchor = ZCIncrementalMerkleTree().root();
        uint256 pubKeyHash;

        JSDescription jsdesc(*p,
                             pubKeyHash,
                             anchor,
                             {JSInput(), JSInput()},
                             {JSOutput(), JSOutput()},
                             0,
                             0);
    }
}
