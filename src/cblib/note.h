//
// Created by reza on 5/6/18.
//

#ifndef SERVER_NOTE_H
#define SERVER_NOTE_H

#include <zcash/IncrementalMerkleTree.hpp>
#include <primitives/transaction.h>
#include <sstream>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/writer.h>
#include <jsoncpp/json/reader.h>
#include <src/plog/include/plog/Log.h>
#include "zcash/NoteEncryption.hpp"
#include "zcash/Note.hpp"


class NoteCiphertext
{

public:
    NoteCiphertext(std::string ciphertext);
    NoteCiphertext(JSDescription joinsplit, int ith);

    std::string serialize(uint256 commitment, uint256 nullifier, uint256 root, unsigned int height);

    ZCNoteDecryption::Ciphertext get_ciphertext(void){return text;};

    uint256 get_root()
    {
        return note_root;
    }

    uint256 get_commitment()
    {
        return note_commitment;
    }

    std::string get_ciphertext_string(void)
    {
        std::string temp;
        std::copy(text.begin(), text.end(), std::back_inserter(temp));
        return temp;
    }

    const unsigned int get_height(void) const
    {
        return note_height;
    }

    uint256 get_ephemeral_key()
    {
        return ephemeral_key;
    }

    uint256 get_h_sig()
    {
        return h_sig;
    }

    uint256 get_nullifier()
    {
        return correspondent_nullifier;
    }

    int get_ith()
    {
        return ith;
    }

private:
    ZCNoteDecryption::Ciphertext text;
    unsigned int note_height;
    uint256 note_commitment;
    uint256 note_root;
    uint256 ephemeral_key;
    uint256 h_sig;

    // belongs to ith note of requested transaction
    // needed for decryption of cipher note by clients
    int ith;

    //TODO: modify the structure of nullifier in class
    // corresponding nullifier of some note: it's equivalent of nullifiers set in zcash
    uint256 correspondent_nullifier;
};


class NotePlaintextDB : public libzcash::NotePlaintext
{
public:

    std::string string_memo;

    NotePlaintextDB(libzcash::NotePlaintext note):NotePlaintext(note)
    {
        string_memo = reinterpret_cast<char*>(this->memo.data());
    };
    NotePlaintextDB(std::string json_plaintext);

    std::string serialize(bool is_spent, NoteCiphertext correspondent_cipher);

    std::string serialize(bool is_spent, unsigned int height)
    {
        std::ostringstream s;
        Json::Value temp;
        Json::FastWriter fst;

        temp["memo"] = string_memo;
        temp["r"] = this->r.ToString();
        temp["rho"] = this->rho.ToString();
        temp["value"] = int(this->value);
        temp["status"] = is_spent;
        temp["note_height"] = height;

        return fst.write(temp);
    }


    void set_status(bool new_status)
    {
        status = new_status;
    }

    bool get_status(void)
    {
        return status;
    }

    unsigned int get_height(void)
    {
        return note_height;
    }

private:

    bool status; // equivalent of is_spent
    unsigned int note_height;
};


class MerkleTree
{
public:

    MerkleTree():merkletree(){};
    MerkleTree(std::string merkle);
    MerkleTree(ZCIncrementalMerkleTree merkle);

    std::string serialize(void);
    ZCIncrementalMerkleTree get_merkle(void){return merkletree;}
    bool update_merkle(uint256 commitment);

private:
    ZCIncrementalMerkleTree merkletree;

};


std::string UnicodeToUTF8( const std::wstring& ws );
std::wstring UTF8toUnicode(const std::string& s);
bool is_valid_utf8(const char * string);

// note addition and query methods TODO: get it into proper classes

// received notes
bool add_note(JSDescription joinsplit, bool is_test);
bool query_note(std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name); // the note with highest height would return as a result
bool query_note(uint256 root, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name);  // specific root
bool query_note(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name); //notes with height greater than provided height
bool query_note_one(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name); //one note with the very smae height as what is provided
bool query_nullifier(uint256 nullifier, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name); // specific nullifier
bool note_witness_merkle(uint256 commitment, uint256 root, ZCIncrementalWitness& witness, bool is_test, std::string db_name); // recover merkle of correspondent note for creating witness
bool query_note_commitment(uint256 commitment, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name);


// user notes
bool add_user_note(NoteCiphertext current_note, bool is_test, std::string db_name);
bool update_user_notes_status(bool is_test, std::string db_name);
bool purge_user_notes(bool is_test, std::string db_name);  // purge all user notes which is spent. memory usage thing!
unsigned int user_notes_balance(bool is_test, std::string db_name);
bool query_user_note(unsigned int height, std::vector<NotePlaintextDB> &result_vector, bool is_test, std::string db_name); //notes with height greater than provided height



// sorting algorithm
bool comp(const NoteCiphertext& a, const NoteCiphertext& b);
bool comp_value(const NotePlaintextDB& a, const NotePlaintextDB& b);
#endif //SERVER_NOTE_H
