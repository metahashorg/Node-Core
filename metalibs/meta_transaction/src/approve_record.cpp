#include <meta_transaction.h>
#include <algorithm>

namespace metahash::transaction {

bool ApproveRecord::make(
    const sha256_2& approving_block_hash,
    crypto::Signer& signer)
{
    std::vector<char> PubKey = signer.get_pub_key();
    std::vector<char> sign_buff = signer.sign(approving_block_hash);

    std::vector<char> record_raw;

    record_raw.insert(record_raw.end(), approving_block_hash.begin(), approving_block_hash.end());

    crypto::append_varint(record_raw, sign_buff.size());
    record_raw.insert(record_raw.end(), sign_buff.begin(), sign_buff.end());

    crypto::append_varint(record_raw, PubKey.size());
    record_raw.insert(record_raw.end(), PubKey.begin(), PubKey.end());

    std::string_view record_raw_sw(record_raw.data(), record_raw.size());
    return parse(record_raw_sw);
}

sha256_2 ApproveRecord::get_block_hash() const
{
    sha256_2 hash;
    std::copy_n(block_hash.begin(), 32, hash.begin());
    return hash;
}

}