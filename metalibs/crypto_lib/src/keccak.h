
#ifndef CRYPTOPP_KECCAK_H
#define CRYPTOPP_KECCAK_H

#include <cryptopp/cryptlib.h>
#include <cryptopp/secblock.h>

namespace CryptoPP {

class Keccak : public HashTransformation {
public:
    Keccak(unsigned int digestSize)
        : m_digestSize(digestSize)
    {
        Restart();
    }
    unsigned int DigestSize() const { return m_digestSize; }
    std::string AlgorithmName() const { return "Keccak-" + IntToString(m_digestSize * 8); }
    unsigned int OptimalDataAlignment() const { return GetAlignmentOf<word64>(); }

    void Update(const byte* input, size_t length);
    void Restart();
    void TruncatedFinal(byte* hash, size_t size);

    //unsigned int BlockSize() const { return r(); } // that's the idea behind it

protected:
    inline unsigned int r() const { return 200 - 2 * m_digestSize; }

    FixedSizeSecBlock<word64, 25> m_state;
    unsigned int m_digestSize, m_counter;
};

//! \class Keccak_224
//! \tparam DigestSize controls the digest size as a template parameter instead of a per-class constant
//! \brief Keccak-X message digest, template for more fine-grained typedefs
//! \since Crypto++ 5.7.0
template <unsigned int T_DigestSize>
class Keccak_Final : public Keccak {
public:
    CRYPTOPP_CONSTANT(DIGESTSIZE = T_DigestSize)
    CRYPTOPP_CONSTANT(BLOCKSIZE = 200 - 2 * DIGESTSIZE)

    //! \brief Construct a Keccak-X message digest
    Keccak_Final()
        : Keccak(DIGESTSIZE)
    {
    }
    static std::string StaticAlgorithmName() { return "Keccak-" + IntToString(DIGESTSIZE * 8); }
    unsigned int BlockSize() const { return BLOCKSIZE; }

private:
    CRYPTOPP_COMPILE_ASSERT(BLOCKSIZE < 200); // ensure there was no underflow in the math
    CRYPTOPP_COMPILE_ASSERT(BLOCKSIZE > (int)T_DigestSize); // this is a general expectation by HMAC
};

typedef Keccak_Final<28> Keccak_224;
typedef Keccak_Final<32> Keccak_256;
typedef Keccak_Final<48> Keccak_384;
typedef Keccak_Final<64> Keccak_512;

}

#endif
