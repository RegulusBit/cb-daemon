add_library(libzcash Address.cpp CreateJoinSplit.cpp GenerateParams.cpp IncrementalMerkleTree.cpp JoinSplit.cpp Note.cpp
                     NoteEncryption.cpp prf.cpp Proof.cpp util.cpp circuit/commitment.tcc circuit/gadget.tcc circuit/prfs.tcc
                     circuit/utils.tcc circuit/merkle.tcc circuit/note.tcc Zcash.h)



target_include_directories(
        libzcash

        PUBLIC
        ${DEPENDS_DIR}
        ${DEPENDS_DIR}/snark
        ${DEPENDS_DIR}/crypto
)