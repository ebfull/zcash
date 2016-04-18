#include "JoinSplit.hpp"
#include "prf.h"
#include "sodium.h"

#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp"
#include "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp"

#include "sync.h"

using namespace libsnark;

namespace libzcash {

#include "zcash/circuit/gadget.tcc"

CCriticalSection cs_ParamsIO;
CCriticalSection cs_InitializeParams;

template<typename FieldT>
class IncompleteJoinSplitProof : public IncompleteProof {
public:
    protoboard<FieldT> pb;

    bool is_valid() {
        return pb.is_satisfied();
    }

    IncompleteJoinSplitProof(protoboard<FieldT> pb) : pb(pb) {}
};

template<typename T>
void saveToFile(std::string path, T& obj) {
    LOCK(cs_ParamsIO);

    std::stringstream ss;
    ss << obj;
    std::ofstream fh;
    fh.open(path, std::ios::binary);
    ss.rdbuf()->pubseekpos(0, std::ios_base::out);
    fh << ss.rdbuf();
    fh.flush();
    fh.close();
}

template<typename T>
void loadFromFile(std::string path, boost::optional<T>& objIn) {
    LOCK(cs_ParamsIO);

    std::stringstream ss;
    std::ifstream fh(path, std::ios::binary);

    if(!fh.is_open()) {
        throw std::runtime_error((boost::format("could not load param file at %s") % path).str());
    }

    ss << fh.rdbuf();
    fh.close();

    ss.rdbuf()->pubseekpos(0, std::ios_base::in);

    T obj;
    ss >> obj;

    objIn = std::move(obj);
}

template<size_t NumInputs, size_t NumOutputs>
class JoinSplitCircuit : public JoinSplit<NumInputs, NumOutputs> {
public:
    typedef default_r1cs_ppzksnark_pp ppzksnark_ppT;
    typedef Fr<ppzksnark_ppT> FieldT;

    boost::optional<r1cs_ppzksnark_proving_key<ppzksnark_ppT>> pk;
    boost::optional<r1cs_ppzksnark_verification_key<ppzksnark_ppT>> vk;
    boost::optional<std::string> pkPath;

    static void initialize() {
        LOCK(cs_InitializeParams);

        ppzksnark_ppT::init_public_params();
    }

    void preloadProvingKey(std::string path) {
        pkPath = path;
    }

    void loadProvingKey() {
        if (!pkPath) {
            throw std::runtime_error("proving key path unknown");
        }
        loadFromFile(*pkPath, pk);
    }

    void saveProvingKey(std::string path) {
        if (pk) {
            saveToFile(path, *pk);
        } else {
            throw std::runtime_error("cannot save proving key; key doesn't exist");
        }
    }
    void loadVerifyingKey(std::string path) {
        loadFromFile(path, vk);
    }
    void saveVerifyingKey(std::string path) {
        if (vk) {
            saveToFile(path, *vk);
        } else {
            throw std::runtime_error("cannot save verifying key; key doesn't exist");
        }
    }

    void generate() {
        protoboard<FieldT> pb;

        joinsplit_gadget<FieldT, NumInputs, NumOutputs> g(pb);
        g.generate_r1cs_constraints();

        const r1cs_constraint_system<FieldT> constraint_system = pb.get_constraint_system();
        r1cs_ppzksnark_keypair<ppzksnark_ppT> keypair = r1cs_ppzksnark_generator<ppzksnark_ppT>(constraint_system);

        pk = keypair.pk;
        vk = keypair.vk;
    }

    JoinSplitCircuit() {}

    std::string prove(
        IncompleteProof* proof
    ) {
        // Downcast the IncompleteProof to a IncompleteJoinSplitProof,
        // allowing us to avoid exposing the concrete IncompleteProof in
        // the public API. (It contains a libsnark protoboard, which would
        // leak libsnark's headers all throughout the project and invert
        // the API design, as it is generic over the field parameters.)
        if (auto incomplete_proof = dynamic_cast<IncompleteJoinSplitProof<FieldT>*>(proof)) {
            if (!pk) {
                throw std::runtime_error("JoinSplit proving key not loaded");
            }

            if (!incomplete_proof->is_valid()) {
                throw std::runtime_error("Constraint system not satisfied by inputs");
            }

            auto proof = r1cs_ppzksnark_prover<ppzksnark_ppT>(
                *pk,
                incomplete_proof->pb.primary_input(),
                incomplete_proof->pb.auxiliary_input()
            );

            std::stringstream ss;
            ss << proof;

            return ss.str();
        } else {
            throw std::logic_error("Supplied with incompatible proof construct");
        }
    }

    bool verify(
        const std::string& proof,
        const uint256& pubKeyHash,
        const uint256& randomSeed,
        const boost::array<uint256, NumInputs>& hmacs,
        const boost::array<uint256, NumInputs>& nullifiers,
        const boost::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt
    ) {
        if (!vk) {
            throw std::runtime_error("JoinSplit verifying key not loaded");
        }

        r1cs_ppzksnark_proof<ppzksnark_ppT> r1cs_proof;
        std::stringstream ss;
        ss.str(proof);
        ss >> r1cs_proof;

        uint256 h_sig = this->h_sig(randomSeed, nullifiers, pubKeyHash);

        auto witness = joinsplit_gadget<FieldT, NumInputs, NumOutputs>::witness_map(
            rt,
            h_sig,
            hmacs,
            nullifiers,
            commitments,
            vpub_old,
            vpub_new
        );

        return r1cs_ppzksnark_verifier_strong_IC<ppzksnark_ppT>(*vk, witness, r1cs_proof);
    }

    IncompleteJoinSplitProof<FieldT>* prepare(
        const boost::array<JSInput, NumInputs>& inputs,
        const boost::array<JSOutput, NumOutputs>& outputs,
        boost::array<Note, NumOutputs>& output_notes,
        boost::array<ZCNoteEncryption::Ciphertext, NumOutputs>& ciphertexts,
        uint256& ephemeralKey,
        const uint256& pubKeyHash,
        uint256& randomSeed,
        boost::array<uint256, NumInputs>& hmacs,
        boost::array<uint256, NumInputs>& nullifiers,
        boost::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new,
        const uint256& rt
    ) {
        // Compute nullifiers of inputs
        for (size_t i = 0; i < NumInputs; i++) {
            nullifiers[i] = inputs[i].nullifier();
        }

        // Sample randomSeed
        randomSeed = random_uint256();

        // Compute h_sig
        uint256 h_sig = this->h_sig(randomSeed, nullifiers, pubKeyHash);

        // Sample phi
        uint256 phi = random_uint256();

        // Compute notes for outputs
        for (size_t i = 0; i < NumOutputs; i++) {
            // Sample r
            uint256 r = random_uint256();

            output_notes[i] = outputs[i].note(phi, r, i, h_sig);
        }

        // Compute the output commitments
        for (size_t i = 0; i < NumOutputs; i++) {
            commitments[i] = output_notes[i].cm();
        }

        // Encrypt the ciphertexts containing the note
        // plaintexts to the recipients of the value.
        {
            ZCNoteEncryption encryptor(h_sig);

            for (size_t i = 0; i < NumOutputs; i++) {
                // TODO: expose memo in the public interface
                boost::array<unsigned char, ZCASH_MEMO_SIZE> memo;
                memo[0] = 0xF6; // invalid UTF8 as per spec

                NotePlaintext pt(output_notes[i], memo);

                ciphertexts[i] = pt.encrypt(encryptor, outputs[i].addr.pk_enc);
            }

            ephemeralKey = encryptor.get_epk();
        }

        // Authenticate h_sig with each of the input
        // spending keys, producing hmacs which protect
        // against malleability.
        for (size_t i = 0; i < NumInputs; i++) {
            hmacs[i] = PRF_pk(inputs[i].key, i, h_sig);
        }

        protoboard<FieldT> pb;
        joinsplit_gadget<FieldT, NumInputs, NumOutputs> g(pb);
        g.generate_r1cs_constraints();
        g.generate_r1cs_witness(
            phi,
            rt,
            h_sig,
            inputs,
            output_notes,
            vpub_old,
            vpub_new
        );

        return new IncompleteJoinSplitProof<FieldT>(pb);
    }
};

template<size_t NumInputs, size_t NumOutputs>
JoinSplit<NumInputs, NumOutputs>* JoinSplit<NumInputs, NumOutputs>::Generate()
{
    JoinSplitCircuit<NumInputs, NumOutputs>::initialize();
    auto js = new JoinSplitCircuit<NumInputs, NumOutputs>();
    js->generate();

    return js;
}

template<size_t NumInputs, size_t NumOutputs>
JoinSplit<NumInputs, NumOutputs>* JoinSplit<NumInputs, NumOutputs>::Unopened()
{
    JoinSplitCircuit<NumInputs, NumOutputs>::initialize();
    return new JoinSplitCircuit<NumInputs, NumOutputs>();
}

template<size_t NumInputs, size_t NumOutputs>
uint256 JoinSplit<NumInputs, NumOutputs>::h_sig(
    const uint256& randomSeed,
    const boost::array<uint256, NumInputs>& nullifiers,
    const uint256& pubKeyHash
) {
    unsigned char personalization[crypto_generichash_blake2b_PERSONALBYTES]
        = {'Z','c','a','s','h','C','o','m','p','u','t','e','h','S','i','g'};

    std::vector<unsigned char> block;
    block.insert(block.end(), randomSeed.begin(), randomSeed.end());

    for (size_t i = 0; i < NumInputs; i++) {
        block.insert(block.end(), nullifiers[i].begin(), nullifiers[i].end());
    }

    block.insert(block.end(), pubKeyHash.begin(), pubKeyHash.end());

    uint256 output;

    if (crypto_generichash_blake2b_salt_personal(output.begin(), 32,
                                                 &block[0], block.size(),
                                                 NULL, 0, // No key.
                                                 NULL,    // No salt.
                                                 personalization
                                                ) != 0)
    {
        throw std::logic_error("hash function failure");
    }

    return output;
}

Note JSOutput::note(const uint256& phi, const uint256& r, size_t i, const uint256& h_sig) const {
    uint256 rho = PRF_rho(phi, i, h_sig);

    return Note(addr.a_pk, value, rho, r);
}

JSOutput::JSOutput() : addr(uint256(), uint256()), value(0) {
    SpendingKey a_sk(random_uint256());
    addr = a_sk.address();
}

JSInput::JSInput() : witness(ZCIncrementalMerkleTree().witness()),
                     key(random_uint256()) {
    note = Note(key.address().a_pk, 0, random_uint256(), random_uint256());
    ZCIncrementalMerkleTree dummy_tree;
    dummy_tree.append(note.cm());
    witness = dummy_tree.witness();
}

template class JoinSplit<ZC_NUM_JS_INPUTS,
                         ZC_NUM_JS_OUTPUTS>;

}