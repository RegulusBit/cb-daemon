//
// Created by reza on 5/6/18.
//

#include <bsoncxx/builder/basic/document.hpp>
#include <primitives/transaction.h>
#include <codecvt>
#include "zcash/IncrementalMerkleTree.hpp"
#include "note.h"
#include "zcash/NoteEncryption.hpp"
#include "src/utilities/utils.h"
#include "db/db.h"
#include "account.h"


std::wstring UTF8toUnicode(const std::string& s)
{
    std::wstring ws;
    wchar_t wc;
    for( int i = 0;i < s.length(); )
    {
        char c = s[i];
        if ( (c & 0x80) == 0 )
        {
            wc = c;
            ++i;
        }
        else if ( (c & 0xE0) == 0xC0 )
        {
            wc = (s[i] & 0x1F) << 6;
            wc |= (s[i+1] & 0x3F);
            i += 2;
        }
        else if ( (c & 0xF0) == 0xE0 )
        {
            wc = (s[i] & 0xF) << 12;
            wc |= (s[i+1] & 0x3F) << 6;
            wc |= (s[i+2] & 0x3F);
            i += 3;
        }
        else if ( (c & 0xF8) == 0xF0 )
        {
            wc = (s[i] & 0x7) << 18;
            wc |= (s[i+1] & 0x3F) << 12;
            wc |= (s[i+2] & 0x3F) << 6;
            wc |= (s[i+3] & 0x3F);
            i += 4;
        }
        else if ( (c & 0xFC) == 0xF8 )
        {
            wc = (s[i] & 0x3) << 24;
            wc |= (s[i] & 0x3F) << 18;
            wc |= (s[i] & 0x3F) << 12;
            wc |= (s[i] & 0x3F) << 6;
            wc |= (s[i] & 0x3F);
            i += 5;
        }
        else if ( (c & 0xFE) == 0xFC )
        {
            wc = (s[i] & 0x1) << 30;
            wc |= (s[i] & 0x3F) << 24;
            wc |= (s[i] & 0x3F) << 18;
            wc |= (s[i] & 0x3F) << 12;
            wc |= (s[i] & 0x3F) << 6;
            wc |= (s[i] & 0x3F);
            i += 6;
        }
        ws += wc;
    }
    return ws;
}

std::string UnicodeToUTF8( const std::wstring& ws )
{
    std::string s;
    for( int i = 0;i < ws.size(); ++i )
    {
        wchar_t wc = ws[i];
        if ( 0 <= wc && wc <= 0x7f )
        {
            s += (char)wc;
        }
        else if ( 0x80 <= wc && wc <= 0x7ff )
        {
            s += ( 0xc0 | (wc >> 6) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x800 <= wc && wc <= 0xffff )
        {
            s += ( 0xe0 | (wc >> 12) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x10000 <= wc && wc <= 0x1fffff )
        {
            s += ( 0xf0 | (wc >> 18) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x200000 <= wc && wc <= 0x3ffffff )
        {
            s += ( 0xf8 | (wc >> 24) );
            s += ( 0x80 | ((wc >> 18) & 0x3f) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x4000000 <= wc && wc <= 0x7fffffff )
        {
            s += ( 0xfc | (wc >> 30) );
            s += ( 0x80 | ((wc >> 24) & 0x3f) );
            s += ( 0x80 | ((wc >> 18) & 0x3f) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
    }
    return s;
}


bool is_valid_utf8(const char * string)
{
    if (!string)
        return true;

    const unsigned char * bytes = (const unsigned char *)string;
    unsigned int cp;
    int num;

    while (*bytes != 0x00)
    {
        if ((*bytes & 0x80) == 0x00)
        {
            // U+0000 to U+007F
            cp = (*bytes & 0x7F);
            num = 1;
        }
        else if ((*bytes & 0xE0) == 0xC0)
        {
            // U+0080 to U+07FF
            cp = (*bytes & 0x1F);
            num = 2;
        }
        else if ((*bytes & 0xF0) == 0xE0)
        {
            // U+0800 to U+FFFF
            cp = (*bytes & 0x0F);
            num = 3;
        }
        else if ((*bytes & 0xF8) == 0xF0)
        {
            // U+10000 to U+10FFFF
            cp = (*bytes & 0x07);
            num = 4;
        }
        else
            return false;

        bytes += 1;
        for (int i = 1; i < num; ++i)
        {
            if ((*bytes & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (*bytes & 0x3F);
            bytes += 1;
        }

        if ((cp > 0x10FFFF) ||
            ((cp >= 0xD800) && (cp <= 0xDFFF)) ||
            ((cp <= 0x007F) && (num != 1)) ||
            ((cp >= 0x0080) && (cp <= 0x07FF) && (num != 2)) ||
            ((cp >= 0x0800) && (cp <= 0xFFFF) && (num != 3)) ||
            ((cp >= 0x10000) && (cp <= 0x1FFFFF) && (num != 4)))
            return false;
    }

    return true;
}


NoteCiphertext::NoteCiphertext(std::string ciphertext)
{
    Json::Value temp;
    Json::Reader rdr;
    std::wstring dest_cipher;

    rdr.parse(ciphertext, temp);
    LOG_DEBUG << "recovered note cipher text height: " << temp["note_height"];
    ciphertext = temp["encrypted_note"].asString();
    dest_cipher = UTF8toUnicode(ciphertext);
    note_height = temp["note_height"].asUInt();
    ith = temp["ith"].asInt();
    note_commitment = uint256S(temp["commitment"].asString());
    note_root = uint256S(temp["root"].asString());
    ephemeral_key = uint256S(temp["ephemeral_key"].asString());
    h_sig = uint256S(temp["h_sig"].asString());
    correspondent_nullifier = uint256S(temp["nullifier"].asString());

    if(dest_cipher.size() == ZC_CLEN) {
        std::copy(dest_cipher.begin(), dest_cipher.end(), text.begin());
    }else {

        LOG_ERROR << "cipher text size to be created: " << dest_cipher.size();
        Exception("ciphertext is not well formed.");
    }
}

NoteCiphertext::NoteCiphertext(JSDescription joinsplit, int ith)
{
    this->ith = ith;
    text = joinsplit.ciphertexts[ith];
    ephemeral_key = joinsplit.ephemeralKey;
    correspondent_nullifier = joinsplit.nullifiers[ith];
}

std::string NoteCiphertext::serialize(uint256 commitment, uint256 nullifier, uint256 root, unsigned int height)
{
    std::wstring temp_string;
    Json::Value temp;
    Json::FastWriter fst;

    std::copy(text.begin(), text.end(), std::back_inserter(temp_string));

    temp["encrypted_note"] = UnicodeToUTF8(temp_string);
    temp["commitment"] = commitment.ToString();

    temp["root"] = root.ToString();
    temp["note_height"] = height;
    temp["ephemeral_key"] = ephemeral_key.ToString();
    temp["h_sig"] = h_sig.ToString();
    temp["nullifier"] = nullifier.ToString();
    temp["ith"] = ith;


    return fst.write(temp);
}

MerkleTree::MerkleTree(std::string merkle)
{
    Json::Value temp;
    Json::Reader rdr;
    std::wstring dest_merkle;
    rdr.parse(merkle, temp);

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;


    LOG_INFO << "is recovered merkle valid utf-8: " << is_valid_utf8(temp["merkletree"].asCString());
    dest_merkle = UTF8toUnicode(temp["merkletree"].asString());

    std::istringstream merkle_stream(converter.to_bytes(dest_merkle));

    merkletree.Unserialize(merkle_stream, 1, 1);
}

MerkleTree::MerkleTree(ZCIncrementalMerkleTree merkle):merkletree()
{
    merkletree = merkle;
}

std::string MerkleTree::serialize()
{
    std::wstring temp_string;
    Json::Value temp_tree;
    std::ostringstream temp;
    Json::FastWriter fst;

    merkletree.Serialize(temp, 1, 1);

    std::copy(temp.str().begin(), temp.str().end(), std::back_inserter(temp_string));

    LOG_INFO << "is merkle valid utf-8: " << is_valid_utf8(UnicodeToUTF8(temp_string).data());

    temp_tree["merkletree"] = UnicodeToUTF8(temp_string);

    return fst.write(temp_tree);

}

bool MerkleTree::update_merkle(uint256 commitment)
{
    try {
        merkletree.append(commitment);
    }catch (...){
        Exception("appending new commitment to merkletree falied.");
        return false;
    }

    return true;
}

bool query_note(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note height
    condition.append(bsoncxx::builder::basic::kvp("note_height",[&height](bsoncxx::builder::basic::sub_document temp){
        temp.append(bsoncxx::builder::basic::kvp("$gt", static_cast<int>(height)));
    }));


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

bool query_note(uint256 root, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("root", root.ToString()));


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}


bool query_note_commitment(uint256 commitment, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("commitment", commitment.ToString()));


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

bool query_note_one(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("note_height", int(height)));


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_WARNING << "query notes did not recovered.";

    return result;
}

//get the latest note
bool query_note(std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("note_height", -1));
    opt.sort(condition.view());
    opt.limit(1);


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, document, opt, "NOVALUE", false);

    if(!result_vector.size())
        LOG_WARNING << "query notes did not recovered.";

    return result;
}


bool query_nullifier(uint256 nullifier, std::vector<NoteCiphertext> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("nullifier", nullifier.ToString()));


    result = database::get_instance(db_name).query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_WARNING << "query notes did not recovered.";

    return result;
}

// sorting algorithm
bool comp(const NoteCiphertext& a, const NoteCiphertext& b)
{
    return (a.get_height() < b.get_height());
}

bool comp_value(const NotePlaintextDB& a, const NotePlaintextDB& b)
{
    return (a.value < b.value);
}



bool note_witness_merkle(uint256 commitment, uint256 root, ZCIncrementalWitness& witness, bool is_test, std::string db_name)
{


    // create or recover merkertree
    ZCIncrementalMerkleTree merkle_temp;
    std::vector<NoteCiphertext> note_temp;
    std::vector<NoteCiphertext> note_temp_reconstruct;
    std::vector<NoteCiphertext> note_temp_witness_append;
    std::vector<NoteCiphertext> note_temp_witness_append_in_range;

    if(!(query_note_commitment(commitment, note_temp, is_test, db_name)))
    {
        LOG_DEBUG << "note_witness_merkle: note not found.";
        return false;
    }
    else{
        LOG_DEBUG << "note_witness_merkle: merkle recovered from database with height: " << note_temp[0].get_height();

        //reconstruct merkle tree from commitment list
        //TODO: figuring out some efficient way to save merkle tree on database
        for(int i = 1; i <= note_temp[0].get_height(); i++)
        {
            if(!query_note_one(i, note_temp_reconstruct, is_test, db_name)) {
                LOG_ERROR << "note_witness_merkle: query note with height: " << i << " failed.";
                return false;
            }
            merkle_temp.append(note_temp_reconstruct[0].get_commitment());
            note_temp_reconstruct.clear();
        }

        // add witness path
        witness = merkle_temp.witness();

        query_note(note_temp[0].get_height(), note_temp_witness_append, is_test, db_name);

        note_temp.clear();
        if(!query_note(root, note_temp, is_test, db_name)) {
            LOG_ERROR << "note_witness_merkle: the requested root is not in database.";
            return false;
        }

        for(auto k : note_temp_witness_append)
        {
            if(k.get_height() <= note_temp[0].get_height())
            {
                note_temp_witness_append_in_range.push_back(k);
            }
        }

        // sorting based on height
        std::sort(note_temp_witness_append_in_range.begin(), note_temp_witness_append_in_range.end(), comp);
        for(auto k : note_temp_witness_append)
        {
            witness.append(k.get_commitment());
        }

    }

    return true;
}



bool add_user_note(NoteCiphertext current_note, bool is_test, std::string db_name)
{

    collection result_type;
    if(is_test)
        result_type = collection::user_notes_test;
    else
        result_type = collection::user_notes;

    LOG_DEBUG << "adding note to database " << collector[result_type] << ".";
    bool final_result = true;
    Postman postman(db_name);
    ZCNoteDecryption decrypter(postman.get_conf_keys().generate_privkey());

    std::string serialized_plain;

    int i = current_note.get_ith();
    // add note that belong to the user
    libzcash::NotePlaintext plainNote_result;
    ZCNoteDecryption::Ciphertext cipher_note = current_note.get_ciphertext();

    auto h_sig = current_note.get_h_sig();
    try
    {
        plainNote_result = libzcash::NotePlaintext::decrypt(decrypter, cipher_note, current_note.get_ephemeral_key(), h_sig, i);
        i++;
        LOG_DEBUG << i << "th note. decrypted note memo is: " << plainNote_result.memo.data();
        LOG_DEBUG << i << "th note. decrypted note value is: " << plainNote_result.value;

        // save incoming notes to database
        serialized_plain.clear();

        serialized_plain =  NotePlaintextDB(plainNote_result)
                .serialize(false, current_note);

        final_result = database::get_instance(db_name).add_request(
                serialized_plain, collector[result_type], "NOVALUE");


    }catch(...)
    {
        LOG_ERROR << "decryption or addition of note failed.";
    }

    return final_result;
}

bool update_user_notes_status(bool is_test, std::string db_name)
{
    LOG_DEBUG << "starting to update user notes status.";

    // first of all! grab the entire user note repository (which is not suitable for large scale usage)

    collection result_type;
    if(is_test)
        result_type = collection::user_notes_test;
    else
        result_type = collection::user_notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;
    bool result = true;

    std::vector<NotePlaintextDB> result_vector;
    std::vector<NotePlaintextDB> need_for_update_vector;
    std::vector<NoteCiphertext> dummy;
    try {

        database::get_instance(db_name).query<NotePlaintextDB>(result_vector, result_type, document, opt, "NOVALUE", false);
        LOG_DEBUG << "plaintext notes recovered for status check: " << result_vector.size();

    }catch(...)
    {
        LOG_ERROR << "updating user notes status failed.[query phase]";
        result = false;
    }

    // second of all! create nullifier for each plainnote and search for corresponding nullifier in notes database
    Postman postman(db_name);

    auto spending_key = postman.get_conf_keys().get_recipient_key();
    auto address = spending_key.address();

    for(auto current_plain_note : result_vector)
    {
        auto current_nullifier = current_plain_note.note(address).nullifier(spending_key);
        LOG_INFO << "Searching for nullifier in notes repository: " << current_nullifier.ToString();

        if(query_nullifier(current_nullifier, dummy, is_test, db_name))
        {
            current_plain_note.set_status(true); // which means it spent.
            LOG_INFO << "Status changed to spent[height]: " << current_plain_note.get_height();
            need_for_update_vector.push_back(current_plain_note);
        }
    }

    // thrid of all! update the changed plain notes in database

    bsoncxx::builder::basic::document update;
    condition.clear();

    update.append(bsoncxx::builder::basic::kvp("$set", [](bsoncxx::builder::basic::sub_document temp){
        temp.append(bsoncxx::builder::basic::kvp("status", true));
    }));

    for(auto current_plain_note : need_for_update_vector)
    {
        condition.clear();
        condition.append(bsoncxx::builder::basic::kvp("rho", current_plain_note.rho.ToString()));
        condition.append(bsoncxx::builder::basic::kvp("r", current_plain_note.r.ToString()));
        //condition.append(bsoncxx::builder::basic::kvp("value", current_plain_note.value));
        try
        {

            database::get_instance(db_name).update(result_type, condition.view(), update.view(), "NOVALUE");
            LOG_DEBUG << "note with memo: " << current_plain_note.memo.data() << " has spent.[status changed]";

        }catch(...)
        {
            LOG_ERROR << "updating user notes status failed.[update phase]";
            result = false;
        }

    }

    return result;
}

bool purge_user_notes(bool is_test, std::string db_name)
{
    LOG_DEBUG << "starting to purge user notes.";
    collection result_type;
    if(is_test)
        result_type = collection::user_notes_test;
    else
        result_type = collection::user_notes;

    bool final_result = true;
    bsoncxx::builder::basic::document condition;

    // purge condition is that the note should be spent.
    condition.append(bsoncxx::builder::basic::kvp("status", 1));

    try{
        int* deleted_numbers = new int;
        database::get_instance(db_name).delete_request(result_type, condition.view(), deleted_numbers);
        LOG_DEBUG << "number of user notes purged is: " << *deleted_numbers;

    }catch(...)
    {
        LOG_ERROR << "purging user notes failed.";
        final_result = false;
    }

    return final_result;
}


bool query_user_note(unsigned int height, std::vector<NotePlaintextDB> &result_vector, bool is_test, std::string db_name)
{
    collection result_type;
    if(is_test)
        result_type = collection::user_notes_test;
    else
        result_type = collection::user_notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note height
    condition.append(bsoncxx::builder::basic::kvp("note_height",[&height](bsoncxx::builder::basic::sub_document temp){
        temp.append(bsoncxx::builder::basic::kvp("$gt", static_cast<int>(height)));
    }));


    result = database::get_instance(db_name).query<NotePlaintextDB>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_WARNING << "query user_notes did not recovered.";

    return result;
}



unsigned int user_notes_balance(bool is_test, std::string db_name)
{
    unsigned int total_balance = 0;

    if(!purge_user_notes(is_test, db_name))
        throw Exception("getting user balance failed.[what() pre-process purging of notes failed.]");

    std::vector<NotePlaintextDB> result_vector;

    if(query_user_note(0, result_vector, is_test, db_name))
    {
        for(auto element : result_vector)
        {
            if(element.get_status())  // this will activate when ever the current status is true which means is_spent.
                continue;

            total_balance += element.value;
        }

    } else{
        LOG_WARNING << "user note balance seems to be empty.[returning zero value]";
    }


    return total_balance;
};

NotePlaintextDB::NotePlaintextDB(std::string json_plaintext):NotePlaintext()
{
    Json::Value temp;
    Json::Reader rdr;

    rdr.parse(json_plaintext, temp);
    LOG_DEBUG << "recovered note plain text height: " << temp["note_height"];

    this->value = temp["value"].asUInt();
    this->r = uint256S(temp["r"].asString());
    this->rho = uint256S(temp["rho"].asString());
    string_memo = temp["memo"].asString();


    note_height = temp["note_height"].asUInt();
    status = temp["status"].asBool();
}

std::string NotePlaintextDB::serialize(bool is_spent, NoteCiphertext correspondent_cipher)
{
    Json::Value temp;
    Json::FastWriter fst;


    temp["memo"] = this->memo.data();
    temp["r"] = this->r.ToString();
    temp["rho"] = this->rho.ToString();
    temp["value"] = int(this->value);
    temp["status"] = is_spent;
    temp["note_height"] = correspondent_cipher.get_height();

    return fst.write(temp);
}