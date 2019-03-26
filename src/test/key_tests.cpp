// Copyright (c) 2012-2015 The Bitcoin Core developers
// Copyright (c) 2015-2018 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "key.h"

#include "base58.h"
#include "dstencode.h"
#include "script/script.h"
#include "test/test_bitcoin.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"

#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <secp256k1.h>
#include <secp256k1_recovery.h>
#include <secp256k1_schnorr.h>

using namespace std;

static const std::string strSecret1 = "5HxWvvfubhXpYYpS3tJkw6fq9jE9j18THftkZjHHfmFiWtmAbrj";
static const std::string strSecret2 = "5KC4ejrDjv152FGwP386VD1i2NYc5KkfSMyv1nGy1VGDxGHqVY3";
static const std::string strSecret1C = "Kwr371tjA9u2rFSMZjTNun2PXXP3WPZu2afRHTcta6KxEUdm1vEw";
static const std::string strSecret2C = "L3Hq7a8FEQwJkW1M2GNKDW28546Vp5miewcCzSqUD9kCAXrJdS3g";
static const std::string addr1 = "1QFqqMUD55ZV3PJEJZtaKCsQmjLT6JkjvJ";
static const std::string addr2 = "1F5y5E5FMc5YzdJtB9hLaUe43GDxEKXENJ";
static const std::string addr1C = "1NoJrossxPBKfCHuJXT4HadJrXRE9Fxiqs";
static const std::string addr2C = "1CRj2HyM1CXWzHAXLQtiGLyggNT9WQqsDs";

static const std::string strAddressBad = "1HV9Lc3sNHZxwj4Zk6fB38tEmBryq2cBiF";

#ifdef KEY_TESTS_DUMPINFO
void dumpKeyInfo(uint256 privkey)
{
    CKey key;
    key.resize(32);
    memcpy(&secret[0], &privkey, 32);
    vector<unsigned char> sec;
    sec.resize(32);
    memcpy(&sec[0], &secret[0], 32);
    printf("  * secret (hex): %s\n", HexStr(sec).c_str());

    for (int nCompressed = 0; nCompressed < 2; nCompressed++)
    {
        bool fCompressed = nCompressed == 1;
        printf("  * %s:\n", fCompressed ? "compressed" : "uncompressed");
        CBitcoinSecret bsecret;
        bsecret.SetSecret(secret, fCompressed);
        printf("    * secret (base58): %s\n", bsecret.ToString().c_str());
        CKey key;
        key.SetSecret(secret, fCompressed);
        vector<unsigned char> vchPubKey = key.GetPubKey();
        printf("    * pubkey (hex): %s\n", HexStr(vchPubKey).c_str());
        printf("    * address (base58): %s\n", CBitcoinAddress(vchPubKey).ToString().c_str());
    }
}
#endif

// get r value produced by ECDSA signing algorithm
// (assumes ECDSA r is encoded in the canonical manner)
std::vector<uint8_t> get_r_ECDSA(std::vector<uint8_t> sigECDSA)
{
    std::vector<uint8_t> ret(32, 0);

    assert(sigECDSA[2] == 2);
    int rlen = sigECDSA[3];
    assert(rlen <= 33);
    assert(sigECDSA[4 + rlen] == 2);
    if (rlen == 33)
    {
        assert(sigECDSA[4] == 0);
        assert(sigECDSA.size() >= 37);
        std::copy(sigECDSA.begin() + 5, sigECDSA.begin() + 37, ret.begin());
    }
    else
    {
        assert(rlen <= 32);
        assert(int(sigECDSA.size()) >= 4 + rlen);
        std::copy(sigECDSA.begin() + 4, sigECDSA.begin() + 4 + rlen, ret.begin() + (32 - rlen));
    }
    return ret;
}

BOOST_FIXTURE_TEST_SUITE(key_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(internal_test)
{
    // test get_r_ECDSA (defined above) to make sure it's working properly
    BOOST_CHECK(get_r_ECDSA(ParseHex("3045022100c6ab5f8acfccc114da39dd5ad0b1ef4d39df6a721e8"
                                     "24c22e00b7bc7944a1f7802206ff23df3802e241ee234a8b66c40"
                                     "c82e56a6cc37f9b50463111c9f9229b8f3b3")) ==
                ParseHex("c6ab5f8acfccc114da39dd5ad0b1ef4d39df6a721e824c22e00b7bc7944a1f78"));
    BOOST_CHECK(get_r_ECDSA(ParseHex("3045022046ab5f8acfccc114da39dd5ad0b1ef4d39df6a721e8"
                                     "24c22e00b7bc7944a1f7802206ff23df3802e241ee234a8b66c40"
                                     "c82e56a6cc37f9b50463111c9f9229b8f3b3")) ==
                ParseHex("46ab5f8acfccc114da39dd5ad0b1ef4d39df6a721e824c22e00b7bc7944a1f78"));
    BOOST_CHECK(get_r_ECDSA(ParseHex("3045021f4b5f8acfccc114da39dd5ad0b1ef4d39df6a721e8"
                                     "24c22e00b7bc7944a1f7802206ff23df3802e241ee234a8b66c40"
                                     "c82e56a6cc37f9b50463111c9f9229b8f3b3")) ==
                ParseHex("004b5f8acfccc114da39dd5ad0b1ef4d39df6a721e824c22e00b7bc7944a1f78"));
    BOOST_CHECK(get_r_ECDSA(ParseHex("3045021e5f8acfccc114da39dd5ad0b1ef4d39df6a721e8"
                                     "24c22e00b7bc7944a1f7802206ff23df3802e241ee234a8b66c40"
                                     "c82e56a6cc37f9b50463111c9f9229b8f3b3")) ==
                ParseHex("00005f8acfccc114da39dd5ad0b1ef4d39df6a721e824c22e00b7bc7944a1f78"));
}

BOOST_AUTO_TEST_CASE(key_test1)
{
    CBitcoinSecret bsecret1, bsecret2, bsecret1C, bsecret2C, baddress1;
    BOOST_CHECK(bsecret1.SetString(strSecret1));
    BOOST_CHECK(bsecret2.SetString(strSecret2));
    BOOST_CHECK(bsecret1C.SetString(strSecret1C));
    BOOST_CHECK(bsecret2C.SetString(strSecret2C));
    BOOST_CHECK(!baddress1.SetString(strAddressBad));

    CKey key1 = bsecret1.GetKey();
    BOOST_CHECK(key1.IsCompressed() == false);
    CKey key2 = bsecret2.GetKey();
    BOOST_CHECK(key2.IsCompressed() == false);
    CKey key1C = bsecret1C.GetKey();
    BOOST_CHECK(key1C.IsCompressed() == true);
    CKey key2C = bsecret2C.GetKey();
    BOOST_CHECK(key2C.IsCompressed() == true);

    CPubKey pubkey1 = key1.GetPubKey();
    CPubKey pubkey2 = key2.GetPubKey();
    CPubKey pubkey1C = key1C.GetPubKey();
    CPubKey pubkey2C = key2C.GetPubKey();

    BOOST_CHECK(key1.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key1C.VerifyPubKey(pubkey1));
    BOOST_CHECK(key1C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key1C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey1C));
    BOOST_CHECK(key2.VerifyPubKey(pubkey2));
    BOOST_CHECK(!key2.VerifyPubKey(pubkey2C));

    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey1C));
    BOOST_CHECK(!key2C.VerifyPubKey(pubkey2));
    BOOST_CHECK(key2C.VerifyPubKey(pubkey2C));

    BOOST_CHECK(DecodeDestination(addr1) == CTxDestination(pubkey1.GetID()));
    BOOST_CHECK(DecodeDestination(addr2) == CTxDestination(pubkey2.GetID()));
    BOOST_CHECK(DecodeDestination(addr1C) == CTxDestination(pubkey1C.GetID()));
    BOOST_CHECK(DecodeDestination(addr2C) == CTxDestination(pubkey2C.GetID()));

    for (int n = 0; n < 16; n++)
    {
        string strMsg = strprintf("Very secret message %i: 11", n);
        uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());

        // normal ECDSA signatures

        vector<unsigned char> sign1, sign2, sign1C, sign2C;

        BOOST_CHECK(key1.SignECDSA(hashMsg, sign1));
        BOOST_CHECK(key2.SignECDSA(hashMsg, sign2));
        BOOST_CHECK(key1C.SignECDSA(hashMsg, sign1C));
        BOOST_CHECK(key2C.SignECDSA(hashMsg, sign2C));

        BOOST_CHECK(pubkey1.VerifyECDSA(hashMsg, sign1));
        BOOST_CHECK(!pubkey1.VerifyECDSA(hashMsg, sign2));
        BOOST_CHECK(pubkey1.VerifyECDSA(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1.VerifyECDSA(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2.VerifyECDSA(hashMsg, sign1));
        BOOST_CHECK(pubkey2.VerifyECDSA(hashMsg, sign2));
        BOOST_CHECK(!pubkey2.VerifyECDSA(hashMsg, sign1C));
        BOOST_CHECK(pubkey2.VerifyECDSA(hashMsg, sign2C));

        BOOST_CHECK(pubkey1C.VerifyECDSA(hashMsg, sign1));
        BOOST_CHECK(!pubkey1C.VerifyECDSA(hashMsg, sign2));
        BOOST_CHECK(pubkey1C.VerifyECDSA(hashMsg, sign1C));
        BOOST_CHECK(!pubkey1C.VerifyECDSA(hashMsg, sign2C));

        BOOST_CHECK(!pubkey2C.VerifyECDSA(hashMsg, sign1));
        BOOST_CHECK(pubkey2C.VerifyECDSA(hashMsg, sign2));
        BOOST_CHECK(!pubkey2C.VerifyECDSA(hashMsg, sign1C));
        BOOST_CHECK(pubkey2C.VerifyECDSA(hashMsg, sign2C));

        // compact ECDSA signatures (with key recovery)

        vector<unsigned char> csign1, csign2, csign1C, csign2C;

        BOOST_CHECK(key1.SignCompact(hashMsg, csign1));
        BOOST_CHECK(key2.SignCompact(hashMsg, csign2));
        BOOST_CHECK(key1C.SignCompact(hashMsg, csign1C));
        BOOST_CHECK(key2C.SignCompact(hashMsg, csign2C));

        CPubKey rkey1, rkey2, rkey1C, rkey2C;

        BOOST_CHECK(rkey1.RecoverCompact(hashMsg, csign1));
        BOOST_CHECK(rkey2.RecoverCompact(hashMsg, csign2));
        BOOST_CHECK(rkey1C.RecoverCompact(hashMsg, csign1C));
        BOOST_CHECK(rkey2C.RecoverCompact(hashMsg, csign2C));

        BOOST_CHECK(rkey1 == pubkey1);
        BOOST_CHECK(rkey2 == pubkey2);
        BOOST_CHECK(rkey1C == pubkey1C);
        BOOST_CHECK(rkey2C == pubkey2C);

        // Schnorr signatures

        std::vector<uint8_t> ssign1, ssign2, ssign1C, ssign2C;

        BOOST_CHECK(key1.SignSchnorr(hashMsg, ssign1));
        BOOST_CHECK(key2.SignSchnorr(hashMsg, ssign2));
        BOOST_CHECK(key1C.SignSchnorr(hashMsg, ssign1C));
        BOOST_CHECK(key2C.SignSchnorr(hashMsg, ssign2C));

        BOOST_CHECK(pubkey1.VerifySchnorr(hashMsg, ssign1));
        BOOST_CHECK(!pubkey1.VerifySchnorr(hashMsg, ssign2));
        BOOST_CHECK(pubkey1.VerifySchnorr(hashMsg, ssign1C));
        BOOST_CHECK(!pubkey1.VerifySchnorr(hashMsg, ssign2C));

        BOOST_CHECK(!pubkey2.VerifySchnorr(hashMsg, ssign1));
        BOOST_CHECK(pubkey2.VerifySchnorr(hashMsg, ssign2));
        BOOST_CHECK(!pubkey2.VerifySchnorr(hashMsg, ssign1C));
        BOOST_CHECK(pubkey2.VerifySchnorr(hashMsg, ssign2C));

        BOOST_CHECK(pubkey1C.VerifySchnorr(hashMsg, ssign1));
        BOOST_CHECK(!pubkey1C.VerifySchnorr(hashMsg, ssign2));
        BOOST_CHECK(pubkey1C.VerifySchnorr(hashMsg, ssign1C));
        BOOST_CHECK(!pubkey1C.VerifySchnorr(hashMsg, ssign2C));

        BOOST_CHECK(!pubkey2C.VerifySchnorr(hashMsg, ssign1));
        BOOST_CHECK(pubkey2C.VerifySchnorr(hashMsg, ssign2));
        BOOST_CHECK(!pubkey2C.VerifySchnorr(hashMsg, ssign1C));
        BOOST_CHECK(pubkey2C.VerifySchnorr(hashMsg, ssign2C));

        // check deterministicity of ECDSA & Schnorr
        BOOST_CHECK(sign1 == sign1C);
        BOOST_CHECK(sign2 == sign2C);
        BOOST_CHECK(ssign1 == ssign1C);
        BOOST_CHECK(ssign2 == ssign2C);

        // Extract r value from ECDSA and Schnorr. Make sure they are
        // distinct (nonce reuse would be dangerous and can leak private key).
        std::vector<uint8_t> rE1 = get_r_ECDSA(sign1);
        BOOST_CHECK(ssign1.size() == 64);
        std::vector<uint8_t> rS1(ssign1.begin(), ssign1.begin() + 32);
        BOOST_CHECK(rE1.size() == 32);
        BOOST_CHECK(rS1.size() == 32);
        BOOST_CHECK(rE1 != rS1);

        std::vector<uint8_t> rE2 = get_r_ECDSA(sign2);
        BOOST_CHECK(ssign2.size() == 64);
        std::vector<uint8_t> rS2(ssign2.begin(), ssign2.begin() + 32);
        BOOST_CHECK(rE2.size() == 32);
        BOOST_CHECK(rS2.size() == 32);
        BOOST_CHECK(rE2 != rS2);
    }

    // test deterministic signing expected values

    std::vector<unsigned char> detsig, detsigc;
    string strMsg = "Very deterministic message";
    uint256 hashMsg = Hash(strMsg.begin(), strMsg.end());
    // ECDSA
    BOOST_CHECK(key1.SignECDSA(hashMsg, detsig));
    BOOST_CHECK(key1C.SignECDSA(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("304402205dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d022014ddda2"
                                   "1494a4e221f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    BOOST_CHECK(key2.SignECDSA(hashMsg, detsig));
    BOOST_CHECK(key2C.SignECDSA(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("3044022052d8a32079c11e79db95af63bb9600c5b04f21a9ca33dc129c2bfa8ac9dc1cd5022061d8ae5"
                                   "e0f6c1a16bde3719c64c2fd70e404b6428ab9a69566962e8771b5944d"));
    // Compact
    BOOST_CHECK(key1.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key1C.SignCompact(hashMsg, detsigc));
    BOOST_CHECK(detsig == ParseHex("1c5dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d14ddda21494a4e221"
                                   "f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    BOOST_CHECK(detsigc == ParseHex("205dbbddda71772d95ce91cd2d14b592cfbc1dd0aabd6a394b6c2d377bbe59d31d14ddda21494a4e22"
                                    "1f0824f0b8b924c43fa43c0ad57dccdaa11f81a6bd4582f6"));
    BOOST_CHECK(key2.SignCompact(hashMsg, detsig));
    BOOST_CHECK(key2C.SignCompact(hashMsg, detsigc));

    // Schnorr
    BOOST_CHECK(key1.SignSchnorr(hashMsg, detsig));
    BOOST_CHECK(key1C.SignSchnorr(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("2c56731ac2f7a7e7f11518fc7722a166b02438924ca9d8b4d1113"
                                   "47b81d0717571846de67ad3d913a8fdf9d8f3f73161a4c48ae81c"
                                   "b183b214765feb86e255ce"));
    BOOST_CHECK(key2.SignSchnorr(hashMsg, detsig));
    BOOST_CHECK(key2C.SignSchnorr(hashMsg, detsigc));
    BOOST_CHECK(detsig == detsigc);
    BOOST_CHECK(detsig == ParseHex("e7167ae0afbba6019b4c7fcfe6de79165d555e8295bd72da1b8aa"
                                   "1a5b54305880517cace1bcb0cb515e2eeaffd49f1e4dd49fd7282"
                                   "6b4b1573c84da49a38405d"));
}

BOOST_AUTO_TEST_SUITE_END()
