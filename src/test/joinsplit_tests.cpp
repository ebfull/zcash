#include "test/test_bitcoin.h"
#include <boost/test/unit_test.hpp>

#include "utilstrencodings.h"

#include "zcash/prf.h"

#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"
#include "zcash/IncrementalMerkleTree.hpp"

using namespace libzcash;

void test_full_api(ZCJoinSplit* js)
{
    // The sender's information.
    SpendingKey sender_key(random_uint256());
    PaymentAddress sender_addr = sender_key.address();

    // The recipient's information.
    SpendingKey recipient_key(random_uint256());
    PaymentAddress recipient_addr = recipient_key.address();

    // Create the commitment tree
    ZCIncrementalMerkleTree tree;

    // Place some money in the tree spendable by the sender.
    Note first_note(sender_addr.a_pk,
                    100,
                    random_uint256(),
                    random_uint256());
    tree.append(first_note.cm());
    auto witness_sender = tree.witness();

    // Set up a JoinSplit description
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint64_t vpub_old = 0;
    uint64_t vpub_new = 0;
    uint256 pubKeyHash = random_uint256();
    boost::array<uint256, 2> hmacs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    uint256 rt = tree.root();

    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
    std::string proof;
    {
        boost::array<JSInput, 2> inputs = {
            JSInput(witness_sender, first_note, sender_key),
            JSInput() // dummy input
        };

        boost::array<JSOutput, 2> outputs = {
            JSOutput(recipient_addr, 90),
            JSOutput(sender_addr, 10)
        };

        boost::array<Note, 2> output_notes;

        // Perform the proof
        auto prepared_proof = js->prepare(
            inputs,
            outputs,
            output_notes,
            ciphertexts,
            ephemeralKey,
            pubKeyHash,
            randomSeed,
            hmacs,
            nullifiers,
            commitments,
            vpub_old,
            vpub_new,
            rt
        );

        proof = js->prove(prepared_proof);
    }

    // Verify the transaction:
    BOOST_CHECK(js->verify(
        proof,
        pubKeyHash,
        randomSeed,
        hmacs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt
    ));

    // Now the recipient should send all of the money back!
    // First, `Receive` it
    auto h_sig = js->h_sig(randomSeed, nullifiers, pubKeyHash);
    ZCNoteDecryption decryptor(recipient_key.viewing_key());

    auto note_pt = NotePlaintext::decrypt(
        decryptor,
        ciphertexts[0],
        ephemeralKey,
        h_sig,
        0
    );

    auto decrypted_note = note_pt.note(recipient_addr);

    BOOST_CHECK(decrypted_note.value == 90);

    // Insert the commitments from the last tx into the tree
    tree.append(commitments[0]);
    auto witness_recipient = tree.witness();
    tree.append(commitments[1]);
    witness_recipient.append(commitments[1]);

    rt = tree.root();

    pubKeyHash = random_uint256();

    {
        boost::array<JSInput, 2> inputs = {
            JSInput(witness_recipient, decrypted_note, recipient_key),
            JSInput() // dummy input
        };

        boost::array<JSOutput, 2> outputs = {
            JSOutput(sender_addr, 90),
            JSOutput() // dummy output
        };

        boost::array<Note, 2> output_notes;

        // Perform the proof
        auto prepared_proof = js->prepare(
            inputs,
            outputs,
            output_notes,
            ciphertexts,
            ephemeralKey,
            pubKeyHash,
            randomSeed,
            hmacs,
            nullifiers,
            commitments,
            vpub_old,
            vpub_new,
            rt
        );

        proof = js->prove(prepared_proof);
    }

    // Verify the transaction:
    BOOST_CHECK(js->verify(
        proof,
        pubKeyHash,
        randomSeed,
        hmacs,
        nullifiers,
        commitments,
        vpub_old,
        vpub_new,
        rt
    ));
}

BOOST_FIXTURE_TEST_SUITE(joinsplit_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(saveAndLoad)
{
    auto js = ZCJoinSplit::Generate();

    test_full_api(js);

    js->saveProvingKey("./zcashTest.pk");
    js->saveVerifyingKey("./zcashTest.vk");

    delete js;

    js = ZCJoinSplit::Unopened();

    js->preloadProvingKey("./zcashTest.pk");
    js->loadProvingKey();
    js->loadVerifyingKey("./zcashTest.vk");

    test_full_api(js);

    delete js;
}

BOOST_AUTO_TEST_CASE(note_plaintexts)
{
    uint256 a_sk = uint256S("f6da8716682d600f74fc16bd0187faad6a26b4aa4c24d5c055b216d94516847e");
    uint256 a_pk = PRF_addr_a_pk(a_sk);
    uint256 sk_enc = ZCNoteEncryption::generate_privkey(a_sk);
    uint256 pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);
    PaymentAddress addr_pk(a_pk, pk_enc);

    uint256 h_sig;

    ZCNoteEncryption encryptor(h_sig);
    uint256 epk = encryptor.get_epk();

    Note note(a_pk,
              1945813,
              random_uint256(),
              random_uint256()
             );

    boost::array<unsigned char, ZCASH_MEMO_SIZE> memo;

    NotePlaintext note_pt(note, memo);

    ZCNoteEncryption::Ciphertext ct = note_pt.encrypt(encryptor, pk_enc);

    ZCNoteDecryption decryptor(sk_enc);

    auto decrypted = NotePlaintext::decrypt(decryptor, ct, epk, h_sig, 0);
    auto decrypted_note = decrypted.note(addr_pk);

    BOOST_CHECK(decrypted_note.a_pk == note.a_pk);
    BOOST_CHECK(decrypted_note.rho == note.rho);
    BOOST_CHECK(decrypted_note.r == note.r);
    BOOST_CHECK(decrypted_note.value == note.value);

    BOOST_CHECK(decrypted.memo == note_pt.memo);
}

BOOST_AUTO_TEST_SUITE_END()
