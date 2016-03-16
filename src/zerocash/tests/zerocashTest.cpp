/** @file
 *****************************************************************************

 A test for Zerocash.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <stdlib.h>
#include <iostream>

#define BOOST_TEST_MODULE zerocashTest
#include <boost/test/included/unit_test.hpp>

#include "timer.h"

#include "zerocash/Zerocash.h"
#include "zerocash/ZerocashParams.h"
#include "zerocash/Address.h"
#include "zerocash/CoinCommitment.h"
#include "zerocash/Coin.h"
#include "zerocash/IncrementalMerkleTree.h"
#include "zerocash/MintTransaction.h"
#include "zerocash/PourTransaction.h"
#include "zerocash/PourInput.h"
#include "zerocash/PourOutput.h"
#include "zerocash/utils/util.h"

using namespace std;
using namespace libsnark;

#define TEST_TREE_DEPTH 4

// testing with general situational setup
void test_pour(libzerocash::ZerocashParams& p,
          uint64_t vpub_in,
          uint64_t vpub_out,
          std::vector<uint64_t> inputs, // values of the inputs (max 2)
          std::vector<uint64_t> outputs) // values of the outputs (max 2)
{
    using pour_input_state = std::tuple<libzerocash::Address, libzerocash::Coin, std::vector<bool>>;

    // Construct incremental merkle tree
    libzerocash::IncrementalMerkleTree merkleTree(TEST_TREE_DEPTH);

    // Dummy sig_pk
    vector<unsigned char> as(ZC_SIG_PK_SIZE, 'a');

    vector<libzerocash::PourInput> pour_inputs;
    vector<libzerocash::PourOutput> pour_outputs;

    vector<pour_input_state> input_state;

    for(std::vector<uint64_t>::iterator it = inputs.begin(); it != inputs.end(); ++it) {
        libzerocash::Address addr = libzerocash::Address::CreateNewRandomAddress();
        libzerocash::Coin coin(addr.getPublicAddress(), *it);

        // commitment from coin
        std::vector<bool> commitment(ZC_CM_SIZE * 8);
        libzerocash::convertBytesVectorToVector(coin.getCoinCommitment().getCommitmentValue(), commitment);

        // insert commitment into the merkle tree
        std::vector<bool> index;
        merkleTree.insertElement(commitment, index);

        // store the state temporarily
        input_state.push_back(std::make_tuple(addr, coin, index));
    }

    // compute the merkle root we will be working with
    vector<unsigned char> rt(ZC_ROOT_SIZE);
    {
        vector<bool> root_bv(ZC_ROOT_SIZE * 8);
        merkleTree.getRootValue(root_bv);
        libzerocash::convertVectorToBytesVector(root_bv, rt);
    }

    // get witnesses for all the input coins and construct the pours
    for(vector<pour_input_state>::iterator it = input_state.begin(); it != input_state.end(); ++it) {
        merkle_authentication_path path(TEST_TREE_DEPTH);

        auto index = std::get<2>(*it);
        merkleTree.getWitness(index, path);

        pour_inputs.push_back(libzerocash::PourInput(std::get<1>(*it), std::get<0>(*it), libzerocash::convertVectorToInt(index), path));
    }

    // construct dummy outputs with the given values
    for(vector<uint64_t>::iterator it = outputs.begin(); it != outputs.end(); ++it) {
        pour_outputs.push_back(libzerocash::PourOutput(*it));
    }

    libzerocash::PourTransaction pourtx(p, as, rt, pour_inputs, pour_outputs, vpub_in, vpub_out);

    //BOOST_CHECK(pourtx.verify(p, as, rt));
}

/*
BOOST_AUTO_TEST_CASE( GenerateKeypair ) {
    libzerocash::ZerocashParams::zerocash_pp::init_public_params();

    auto keypair = libzerocash::ZerocashParams::GenerateNewKeyPair(TEST_TREE_DEPTH);
    libzerocash::ZerocashParams p(
        TEST_TREE_DEPTH,
        &keypair
    );

    std::string vk_path = "./zerocashTest-verification-key";
    std::string pk_path = "./zerocashTest-proving-key";

    libzerocash::ZerocashParams::SaveProvingKeyToFile(
        &p.getProvingKey(),
        pk_path
    );

    libzerocash::ZerocashParams::SaveVerificationKeyToFile(
        &p.getVerificationKey(),
        vk_path
    );
}
*/


BOOST_AUTO_TEST_CASE( TenPours )
{
    libzerocash::ZerocashParams::zerocash_pp::init_public_params();

    std::string vk_path = "./zerocashTest-verification-key";
    std::string pk_path = "./zerocashTest-proving-key";

    auto pk_loaded = libzerocash::ZerocashParams::LoadProvingKeyFromFile(pk_path, TEST_TREE_DEPTH);
    auto vk_loaded = libzerocash::ZerocashParams::LoadVerificationKeyFromFile(vk_path, TEST_TREE_DEPTH);

    libzerocash::ZerocashParams p(
        TEST_TREE_DEPTH,
        &pk_loaded,
        &vk_loaded
    );

    for (int i = 0; i < 10; i++)
    {
        test_pour(p, 1, 0, {2, 2}, {2, 3});
    }
}
