
//
// Created by reza on 4/5/18.
//

#include <iostream>
#include <primitives/transaction.h>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "zcash/Proof.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "streams.h"
#include "util.h"
#include "tinyformat.h"



int main(int argc, char* argv[])

{
    // pp: public parameters used in the network
    // construct a merkle tree
    ZCIncrementalMerkleTree merkleTree;
    ZCJoinSplit* pzcashParams = NULL;
    pzcashParams = ZCJoinSplit::Prepared("vkfile" , "pkfile");

    //Address
    libzcash::SpendingKey k = libzcash::SpendingKey::random();
    libzcash::PaymentAddress addr = k.address();

    // note is the basecoin definition. value of this coin is 100
    // including:
    //              value
    //              commitment
    //              serial number
    //              owner address
    libzcash::Note note(addr.a_pk, 100, uint256(), uint256());
    // commitment from coin
    uint256 commitment = note.cm();


    // insert commitment into the merkle tree
    merkleTree.append(commitment);

    // compute the merkle root we will be working with
    uint256 rt = merkleTree.root();

    auto witness = merkleTree.witness();

    uint256 pubKeyHash;

    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
            libzcash::JSInput(witness, note, k),
            libzcash::JSInput() // dummy input of zero value
    };
    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
            libzcash::JSOutput(addr, 50),
            libzcash::JSOutput(addr, 50)
    };

    auto verifier = libzcash::ProofVerifier::Strict();

    JSDescription jsdesc(*pzcashParams, pubKeyHash, rt, inputs, outputs, 0, 0);

    std::cout << "I: the verification result is: " << jsdesc.Verify(*pzcashParams, verifier, pubKeyHash) << std::endl;

    return 0;
}