/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2023 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Reference checksums for video decode validation
 *
 * See the <vulkan_data_dir>/vulkan/video/frame_checksums.py file for
 * instructions on generating the checksums for new tests.
 *--------------------------------------------------------------------*/

#include "vktVideoClipInfo.hpp"

namespace vkt
{
namespace video
{

static const char *clipA[30] = {
    "7fec73fbcd485785b0c5f14776628bd6", "4e635dfa94b770ee983540746f416270", "a8bbb2ae42f5edebe7891ff803be7c4f",
    "3389478c7a0011551274bb68ce6deb5f", "63aa1d861304729dfec950f486a15a0b", "04980dcedb06d21d0c7dc37653d3a458",
    "2a0162386ebbf3eea6f613042cb2a617", "a29121c412ae6bda174254039a5b6086", "bfdeb27a7a83b250a8311a4468c3e4bf",
    "cee21548a5235a8eea878dd695ef8f50", "3b8cc378c3843c7c5918e9213e9e4f81", "d11322bbaec122fcc7f2d8ed6b455110",
    "ffe3f13784f7689735d2b8c96bcd2d0b", "0253711c4523988d4b9736238f5e5d01", "d3968c4560c31815443f857ddb0082a3",
    "74b0ae5f72a6676e4679b30f06598277", "7d49ad952d2600aefe20e0a6611e91bf", "32d86ff0d1dfb493adb2d90e973025fb",
    "a0f52f968b800bd8b67c2a44b5394690", "0a54a47ed72ecead9f7a43f39a562fca", "c1effee5f75f5c59c71a3765e199861c",
    "75f020c1da96c2d5da31483c16a1e4ed", "66cfa534ced4f1b18b6b04cc0a41331d", "3648527cf7b9b7366759c5ab8f1aab20",
    "1c7c47a07c5bb38dfe2a4e9e2cf5d860", "2e2bc3c723a731b4df4eb32a0b36293d", "6e647c760a0304d97abf291a2b9e66be",
    "5a9fe2e1bdf31ad0d256aeeecbe35ae7", "3e825fb6fb5361151fb7c1347523d6d0", "2870bd63631f4b787a1b084974cd519c",
};

static const char *clipC[30] = {
    "2086be0ea6f35e68a9f34ea51a359ef0", // 352x288 reference checksums
    "9e045b8e14c7f635fff4a4e56a359ff7", "b9aace50035d92d2b2b0cbc5049ccea1", "eda08231bb604567d72d850b9aaa8658",
    "b5b9ec2c1a231f595c6c747a93926287", "81f77e981e65926f04ed9483f838ce1f", "8fb63cb865f410e5d003a6691ef6d998",
    "0417e3ce4d51670e8cf5e1a1a8b64b0d", "5c7cee8e1e72b3acb71dd09f51e3d4e4", "966368b1970e59dbd14d5e492691c25c",
    "8afd1bb08c05651dacc5bf1a72fb1001", "bd9fe310e70fc55fbc0b43895aa14eb1", "057435492eb70ebf1ffcec5b423828f7",
    "6df3ab9e8f590e8e21b88e5401361d49", "531730c9a1ce3076c5f1a30609efcf17",
    "87823f36c56ecd539c955cfe3e44d097", // 176x144 reference checksums
    "65399b5342b3ee7bba9f51074523ae16", "fd3c5efbc237e24c3242aaa35124b77c", "56dd0b57ba4b436605f236ae7ad8535c",
    "c1694a837158581a282493ffb7778d1a", "64417694aeccbe36abb77cd5689083ea", "08bba16075a5d1d36fadac3ca3a71d06",
    "adb982cd7240ab8d97924631455a5d88", "822bd35904dabe46e9238aae77c5e85f", "1da1fc85653b894fc6f4cf2ddd41b7f6",
    "80e283c058324a21cace4c8ed1f0a178", "b5169864eb21620a1a4a0f988c01fefa", "cf38cf96287f0cd13fdbdc2b25cfd0d1",
    "95b71dc4a6a7925cfb1ad6c76e547b95", "c2146da8b89f068e00d7de837a02b89c",
};

static const char *h264_4k_26_ibp_main[26] = {
    "2cd5ad413430098893a617ab46a7b885", "0b82fc7501897a834470f68e0647c96b", "5099dbd788ec90ad9010bbcc4ca3a813",
    "10ffaf41904c779627ab6ae9ebe5bf9f", "7d4aca0319da143a09d03b3b13d5ba23", "34f6ab5b1b676c189950f0b2f80d9431",
    "e812f06035d77030cf56eaa65632783c", "f8dbeb21b35a5fedcfd171a8207f836b", "b0c5a18f31c92e9778d1dcdca4b781d3",
    "bb89ff68261f0626eb8699dc3e000278", "aeb5064995db3a5cdae6283c67a2949e", "425dfb76d5961994a6dff267dc0fc5a6",
    "17800cb3d7598c6284f544069653c594", "bb193efca8e1a2f6bcb9f523c815468f", "a8eb65b204edc25a9dada32889717218",
    "715e7ec00e756ca3c85722f9464aeba3", "496e1e2c97a8cbfabb7b179838e3384f", "102445bf67c88ecb761f3263b8ab672b",
    "d9a3dbe8b05a4d7fd04a4564771b3dc0", "8fe9190578592d6a551f47f19926bb21", "854519810e75eb9501e896ec66b83d59",
    "ccd65246951a2b77dce66e11faa97811", "cde30e5d581267834ae91f4af77d7fac", "e144d9fe07470ae7be48362ab8817cc1",
    "4ef58adc2006d4456a0145102efe83b3", "ebfb7d5301c287e09a6473402fdcd010",
};

static const char *jellyfishHEVC[26] = {
    "131ecbc5c6ebac63c3b25721cb5cb2b2", "0a9da70fd9959874e74542c7768d9e08", "2f09ce3f5d243d84b12f53d2574b8e9c",
    "909d939a5d09650e49703b7e78beddfe", "483cf425d719b961173db2f1a1c7f282", "b3d1e44427723fb80d1229e335e2cacd",
    "5c5f38b8ab0f19602c0af8fc4a7b37a1", "7e3e579e923a0f5d07b75fa18e8be8ce", "ad2dc11df6f46a011d0dd923c0001dc5",
    "f490093b4e2dfee8fe13b2e41310607f", "982511b0c457e643b4f77bfd64db55eb", "f985dc3bd5b366edf0a7541af3fa3a72",
    "6493f4a899a3450fe8f2dfb77ab97812", "04dec3a5a5ce2bd480593a708885a0c2", "0afbf90e7e45c04f566eac2aecb19e52",
    "e1016a026c40490db627cdc51ef7b600", "869d25ef36cc75bc2729ab4a563cd1a0", "ba0f91e20161af480776c24790538153",
    "5896ccae5cdc7abbee7d064f11bc55bf", "80565c134c28c8413cb8ee6217af15fd", "e1acab18892d012cf14d6acc69f660b5",
    "c05cf136e5f464daa2e03484d855913e", "20f32820fa8deb9e971f56778bc7ba96", "e9bbe21340ed0c828a6f2eb5c2b204e6",
    "27c5e8099070dfcbae31ba7f9ddd97e5", "87be085bd498c3f97e9bc55beed79683",
};

static const char *slistAHEVC[65] = {
    "dba59afb916ccb6efed8d5fd815d2c1d", "01eddcf4049ddfba18d8d35864434464", "c54ef99eef04384e437a836828b54429",
    "2abf6b933b835c7ff998232e183461ee", "7fedd24815684b3dcdb8aa7bc437e37d", "c652de71628ad5cabdb32b929e3f90db",
    "3a07630caeb6e3c2f0c99ea8069fb023", "a9371041389a1c36d79c8348c0b73262", "14f66961d87a7bf40cd1f5489beac6f6",
    "ae97bcf48cdc5ff3f2d5e9c4f51e8d98", "030767b9404b2ea879a8ef76f8e44cca", "02e14ab304416d82021ea6cfad9c865c",
    "83b9ab620125c75136130c00ca159ffb", "d4639edac5e0ea0d19439444e65e37d0", "e180704c3a593bd4df1ff7f3168184a2",
    "7c6f8471b891abaac6b5e1f82974c1f8", "a8a2ed1a8843a6c2774f0f07e1baa381", "40328f317716871019b638aefe75d8fc",
    "d02a58c68ed46e7b02a13b0b6fb1b5e0", "589890c324c746c43ada7d48d79d050f", "0606deb22f430a73d6cdb509800fb63f",
    "f20b25d9c41b26293de821f9efd81f03", "fc69ab3e342f5da199d10fbfd230a091", "8bd5a832dec96cbd8c8089325f6dcdc3",
    "76356d2eb607a969ed80fdfcda39272a", "b9c39e728d0cdbf8867d3f677ee5cc63", "64694356f1138c7a85832dd04b1fc43d",
    "d3ad289494d67a07b1ecedfd9f8facee", "71447158718c78f905edebf3e00decd7", "1ff838c50cfbbf93ce42fa20c8d7985a",
    "25b041a3483a26e4ba10ce2b61f587f5", "9f30a6b5c9ec74a46f20407ad89d5036", "7c29ce2eed193cd85456a509e853f2b5",
    "89bc2240caec2b6a15d689760882c9b0", "74ca65d556993ef1073d54e0aa1e952c", "832365edf6962f06a4d4e3dec36a7d51",
    "d8237901c4df298a9feab9b405615945", "ddac17d0aec0a6a01f3047a422718be9", "82b5b78172bf8ebe313cecbebdbadc65",
    "57fa160e4783e6fdc269db1b0d27cb93", "2516a88c2bdfa961521495f8d52af830", "9799de4071746d65082517b0efa302a9",
    "ebb52f02ae5f1b9abc34842390d2a24a", "4113d5fdb02b53f725b6cedacce08374", "18248f5d1ff464727a8d264beb9d748c",
    "8a24a2201c0be36c10ecb678dc46aa6c", "6ee4d0aa9c22549dfca1d059f9655904", "1b171db883ccd47c4af57af74f52aef8",
    "dcfa06258f03c502f2c7210bacbda750", "0f13fedecc58e7c197b241bf63cda3a5", "c3cb4b85705890682c19bb614853be2c",
    "bb754ad707c2463f7f40bc699fccebb5", "a2676bb5e37d1e5c91cc0c78e9216e2a", "396c7b826bfe99752e60c43f8218da7f",
    "c567a70596fd15fb1e89a7887a8f2b5b", "1f3025ffd70e53df4be9f71f096e0e27", "d68073e211aea90d16a744fe19cdde7e",
    "ab23d39b501320e22e4ccff675caa495", "7ec0f756cba8d92e4a2e2013f056c3f5", "3f6a723c0e72345ca7c72de00ad5819d",
    "5e8c879314c9575b64206880afb6302b", "462fd7e60fdbf31769f77cdc7cf658cc", "8889d84206116d612e7eb9abaa3e8e73",
    "9bd67392254190f15464d6f04924f4dc", "f86b9d6da1b223bb3ac69682745278be",
};

static const char *slistBHEVC[65] = {
    "dba59afb916ccb6efed8d5fd815d2c1d", "b7e2af81be2c780fe104f8512e19d298", "6e386521a66175377e891e2efe24d389",
    "a6b75408a4567551adf63486cb81eb73", "3473ed3d042b86a941f9c7929748156c", "da535125bca7b0f07b9edcabf3c6e17a",
    "244c121f18511846ff370118f5ac856d", "79677fda76cd3993952f64be05eebcd3", "14f66961d87a7bf40cd1f5489beac6f6",
    "0eb51d7684bfa4c06ba82b56fc1a3e23", "75be1b9751b35eb1b9c8aaf1cfae9dfc", "408ace9990773b26a9e93f4fc3a4cd13",
    "228ff2fb6e618297ecabd731d764e258", "0b0965a30bedad0f4a66eb81eccff11a", "b26c0164d9da1833beaec3c52a28b34c",
    "c30d3a599a9256a1eb54fad6f50d828c", "b0effd63f31ae61242387697ae47e3a5", "a022024a9a7c830d7aba03909c02848b",
    "c706de03418a2f563628abfec358798b", "238040d9deabbc09ffa7db5b85025cd8", "52d9c53e2fc1398466f4eecf48c7519e",
    "b65690a004434de9a3a2105f42a0922c", "b864d224e952b787d9d8eac870a31164", "2308015257e07ebce493f6aa49862b62",
    "731a4b70437ebd245c5d2ca8d49f7a75", "8f0295c3ed1432c07137db95bd347276", "348a63f5738e345a846201388e53c91f",
    "d6b06f3616292d63c3f5c83e615df069", "1338828f268010f5f539792f19328d8e", "386c0f78dfc663978275539493f1cec2",
    "2269eb42d789ec411a54c9eccc086b6a", "d2c746c2000686497944fa9043e794e4", "ed5696d5f616ed06572636345b21110a",
    "0f96181b5f0368cfc529bfee9ec492c3", "9549c240ce8f0453cb327b98d1e3a62d", "3087704f92b3982a038053ce19653644",
    "c05f8b54f639d1bafb4920f9cf2cc507", "2828f9b4dc1f94ba69b5afdfbbea517c", "a45677eb696b3e8705980fd7c39d42b4",
    "3a8629e2bc9f3fe4edefa00683ea0d6a", "35f108753f4b002e7c247b65ad0ad0e8", "7cebe4dfb8575da5c68c2828bb9f34cc",
    "ccf8e94cebaff2fd8ffb53b9cc932b5e", "7f35e69f8e5fc384433c95634d08cff1", "0b63429aca064de21a257a51f55fa31d",
    "0ae8afab6e3b7d4a428498714da54d23", "11bb8a464172bbfb9b8f297cbcbdbd28", "114755eb742f91ab02aacb4fcbe490be",
    "8b7f38b6ca5cd17799c36ef33757566f", "acd0f22d58e1f54517d9cedbffb1c933", "7532f4d65da3d2114af1ac918a3add08",
    "c3d27eadfe76b2d96c3b447ac6beb9e8", "fb0a6e28754d9ecfa8f84f2003d10fb1", "e708fdfbe402a9f86a2872915fd58b65",
    "96b2bbd36e2b88d5d9c144aabe92a52b", "c5731882aa4127ccd2f07296d00919ca", "d71480b0b941231e18b7e82fa5be3377",
    "583eddd55b7c1e7b4f8842e968bfa473", "3c9f1134d811b13b4786cfa29e098769", "5454bb20d23f9998af6dfb6b38517e31",
    "c24afd9f9a4dd46854204d66911a6a89", "71cfb3ee29ed44cbb840f293e921ca62", "8dcf4325c7ca426273f05b2f8b6e43ed",
    "77530a8c08a5713f3a0f546025258748", "3adff0baa82a42a91bc1df312517ff8b",
};

static const char *basic8AV1[30] = {
    "7fec73fbcd485785b0c5f14776628bd6", "b65b9cf2f8194236330fd5e7e8350b16", "7c4cc029090f45796b43ff352b4cc679",
    "077561167a837df5a9a946f6a8c18a5c", "5f435c0ec053c75aab6233fce40a4125", "45a58a0ec4751e0e7205e5fbfef675b0",
    "9cd28856648ad33b8e3fa9514877fec4", "306d5ca2e91b557e4ce36474183f2234", "fe3e74c45a285e13001cfeb58f3338fb",
    "f1a22400b16f1c52c66eef9dd0840b74", "85ea43d8cf2ab462ee331e0e68f17a56", "2ec2d8f051dada172f6acf85d6b7b4b6",
    "438872ba513b608047e6375040252599", "bf1201624ce3e14b836d4e41de62f3ac", "07797e7c21f0ccc3409cd940d7fe9051",
    "855a29477f4193b144fa58448cccab15", "0c061a895e0ee6f88ef3db6605a0af29", "17d89d37c3c7084dbc6d7744265c415b",
    "00909ef87a99b5d42bf5f90f15225400", "b0e01dbe4b2484e099ccf40359864f84", "c1effee5f75f5c59c71a3765e199861c",
    "1ffbdb3fc6522b020a465d4d92538de1", "405ed627f04a44956bc211a8206cb774", "477d915b398fa54bfa08d7f6c712609d",
    "7926240a6ff3c690f419987f929cc558", "ced2e44e064ddded6f8403a5d29c3c4c", "3f7668e8a48d074eeb733cc158603873",
    "495538554c8063a4332dd00531e70f15", "e23c1dbe349d1f116fbe040efeac8931", "efa8967dbcf46dd114ac714f4216d2f3",
};

static const char *basic10AV1[30] = {
    "3da842f80be1583785df87a14d28f861", "3e629c6552343e3213569b160288908c", "e2e69fa47ba4346e7b425516cd3ad399",
    "5c4acfd736270ec614d4235d4b457c5f", "1bf85d94643a7edba6b1be0e273b1494", "db436d9f0556df36e7a61fe43b6ca8fa",
    "8653f145dff84ada80a06d73c89a8ade", "cd063b092bfe49152a4e532f6c47a366", "ed410923cb899ca9b985591f97db4eeb",
    "b10b3439906926e829531af9092d49c3", "4c0abb5321521506d6dd82cad743c5bb", "dad5abd30028edc9f2b58a82a2c62154",
    "385a53090a64ea638da73675e57af26e", "3381eab0147435ec47b6cceea43bc9f7", "66f3a5786ded03618e83cf0559ac07d5",
    "70cb63f503c0a480477abd4819fafa4b", "38785bcf08832cd952e8e03fd2e97fe3", "2a308da74bcf00e5443ea8d44711490b",
    "a4dda1c3cd1bbed78fe147fc96f3c449", "02e880b02320859564a3ee93af560bad", "ed2b06bd4fd31091a54b1739086423b9",
    "d6f30edc476ccdf8efde98c2093468fa", "0ca4576f5f1709aa282ff72b074808cf", "35ca496c0a8566a2f544101beb3c6b26",
    "90578ad37a02e59b9dbf3e0c278fa401", "169c4e28ce1e5c1954519ace57be64f3", "59a9840efb77ae92c0de834082c6fd05",
    "0826b6c29ed55fe7e985806e8d70a97a", "ce3e5f43e0fec03be8ab89be53ddd13e", "e53d96029124a706e330c1c0d6bd612e",
};

static const char *allintra8AV1[30] = {
    "f7d06010d13a06455848d5d394c7bea9", "f06f017fab7485af4a65eb38eabf440f", "5f826aa41f82d4e87575fae50a6a8ae1",
    "98f48f68b5cb6ff99be50524c7b89bca", "f4a695c3a38213e8e6e59af55badb099", "692d78e599f28282e0dcf177f37c0622",
    "f743e671cc74ab32ce97e3f0d48f3dd0", "67a15a5ce3cfe028c770eccf045b38f7", "39276c7212f3f35965cb8562e36030e2",
    "866da32385cce98d181fc513c4a1d009", "31d4e7b0b220b2f77824a208e48fccdc", "1f7c21dbb5fd7dcd7f62d395b52472d9",
    "2142a1394c221a1d6e28568bed8852a5", "f7cfaad4912c3b203646d6d64b070827", "08afa2ba43fda976ec51ea14fccb4d2d",
    "cdadbfb766bdfda88edb936427396e69", "821e17020cd07757dfac3e2d95cca497", "1ab1925f2a9ccd791e5dc19454289205",
    "f6a8eedd6184acf8cb633892ecc72c5e", "714837df1f3e3aab0de0c6529ce64859", "e10aa6398a388060809e4296dfc7ee18",
    "45ab7d592b05f3e756920eb266172f70", "bc251d000810605f2b576dc5333db1ac", "ce68f6d37e5d0a0eb202b90b43544110",
    "8f26ba6ed5f82f292470deab0fc78b9f", "996008ea7c34b31e9f8967b372251145", "6fa15a7b27070a0f445add8e70c2ed35",
    "f129769476db347af15b846c9e92299b", "9ec83b78b9b9d159e90c22428b7bd45d", "4dea2c2f152bee369d1d7c7150a1b06d",
};

static const char *allintrabc8AV1[2] = {
    "4013774063f11a76ceed39509652ac89",
    "0d015427f07d38debc639a5986978021",
};

static const char *cdfupdate8AV1[2] = {
    "acfd7c50b9b8e57b9c7ceff961d3206e",
    "794a8a3b813366319a5f0d0f12cde0e2",
};

static const char *filmgrain8AV1[10] = {
    "9fc2c754a8a3787b265b5c03b6187fa2", "a9a12ac5956f4dc932ebe183a2a9b32f", "1797729334584aa35ec31785e99dee35",
    "4b32cbea08b4f66ab106fe2d15cb0991", "2ee6937d0a35362a50e6fc1f52ab122e", "b47c737ac5b4369eea551e88adb2f5fc",
    "df4e3ac8f286c10a777a5c2003e1131b", "b5498a474837e063a59bd1f7958106f3", "350561922c88fdc37aade9494d4ec110",
    "c4941eea6e6faa6bbd5cccf83b1eacc9",
};

static const char *svcL1T28AV1[8] = {
    "f3874ea6c93fa8e345532e7273a00c78", "9fa72a857e695607338354107695176a", "a2eda89c577c97cc342d0ebc79f223c2",
    "abe1f2c18b65422df79f7e3fa3e62f2c", "d349496e56e18763cc43139d16822329", "4edcc8be524cddff3787f87af8b6a40c",
    "450f1a8a26c719f26fe74287974f64b0", "a3ac8d3056e8e1deee9c02cccdf2a96a",
};

static const char *superres8AV1[10] = {
    "c7c2ae16f306aaa67a0cd880b43bf08a", "663558dd0d39eb0e20339be1410342b1", "e2e7b5dab0267385ce07e78ed07e7ad8",
    "f27c21cd0efcfd5391a719443d57b219", "851c6772e4b55a081cfd3829d4b959a0", "ee4ded0f06d6adcec7025bb08d52b37c",
    "568fe76c1fbbd76415b449049834de6b", "0edd6223020fb9d15434d3fed080ff93", "0924ec198a4cf1c045ade94d7decb5f8",
    "5ffdd2f17169bfe7979d659a60f7e4e0",
};

static const char *orderhint10AV1[30] = {
    "3da842f80be1583785df87a14d28f861", "7e9b4cea67392bff367ee0a05a0133d8", "de3b49a00894a4a6528353cea5bb9855",
    "99814671f4e9eb6e488e0d958ef8bfdf", "3ce2507f38b8459f88adfeb57ddfcbb0", "1bc42ec76938aa019b8931b69a93e288",
    "cd2ea1d885dc921074674ad9035efc25", "f833bfa030347a1946329810eee65e85", "025ca7e6d55a7d1c8fab74bd7d2b010c",
    "b3d1f279fff264e836e0d11aa3af73d6", "c4709e5fea5648cf346241b10061a23f", "28d76e6b53941f575d661129165f5dd8",
    "c357c60c6f9e1316b2dabc905a0f529e", "3381eab0147435ec47b6cceea43bc9f7", "8a1c63b9ae1966c4a8117d497923cd97",
    "70cb63f503c0a480477abd4819fafa4b", "83feccd6ce008d66cd8409ce1ff62f4a", "dcf114e3b129dac0c17d4494c7ec01f3",
    "bd7973a0156726069f13717c4aa4fd77", "287b5a33747b4c2094c79ec150b24e34", "356f6d3bb31ee08bd3d131f6e3bf7db2",
    "6c9d5bcb07f1fa2a837267f95690755c", "a56fd3e10c143fa4f4959a59eae58b67", "fdfbeef91eef9019ce23666f8d3596ff",
    "0ef3ea92f36c386472265c9e3aa1edf8", "062b456b2f1f306937b9b0eb60999c7b", "e539d3e3b657b2c63c621f55009b55ba",
    "1ff04b727e26d3df747bae56f66687ab", "ce3e5f43e0fec03be8ab89be53ddd13e", "1b58c2fcbc035f33dbba51506cc79647",
};

static const char *globalmotion10AV1[30] = {
    "7fec73fbcd485785b0c5f14776628bd6", "af29d34591da3b99f0a2533825a2cddd", "95ecb98df35904848273b4979307ddfa",
    "bbb974b0ebb3cefc4104382c2dab8ea8", "b9f2e2d3bc6d4f6a61384f093676d9c5", "04a606f910bb0acf89384f49537ecd5f",
    "a0e35b7432abfc107eb398dc7bbfde34", "ac99fed61ca7c96095c7fd74a558379a", "afa91292ae6ae160f37c9c33038e7fe6",
    "83774993673ee5fa7ccce8539246bf4f", "85ea43d8cf2ab462ee331e0e68f17a56", "8fc3a67537d1808f16db709d739968c8",
    "c33aa68ce0474539bf5d3d2d1d7f538a", "7dd3e4cc769189becd2003c5c503cc43", "5fa9c38c794baf14a5c51cc627a240ac",
    "33555da5b23423110e230bcf18ffba66", "a386f53cf94524b5f3160c7b9828b0e0", "ff4b7b73a58a935d48931e6dafa700d5",
    "dc42efeac256efaa4a7beafa7bf4f587", "b8d8ff228f5f6fb2079eb698754ee204", "1e87014b9dfb03e5ffd397d7c87eec41",
    "e50bb0a799f4f1cd68a5c37762d07fff", "d90f54ce0d1cd17bfd86896c254397e3", "a923739505e724bfc7fcce9f36e45711",
    "2e98fd27f166f7ae3a959c791437563a", "8ebf3d3dc34b9ee9f7f95a4fa5411bb4", "65f910e17bc354e5889c6fad0ef5a299",
    "8774731cdcc3b86e5319ce1f20dc7dcb", "285d35ce961fbf6134ad300d33a314ad", "186f14908cfc20d0579166a9fca59cd7",
};

static const char *forwardKeyframe10AV1[30] = {
    "3da842f80be1583785df87a14d28f861", "3e629c6552343e3213569b160288908c", "e2e69fa47ba4346e7b425516cd3ad399",
    "5c4acfd736270ec614d4235d4b457c5f", "1bf85d94643a7edba6b1be0e273b1494", "db436d9f0556df36e7a61fe43b6ca8fa",
    "8653f145dff84ada80a06d73c89a8ade", "cd063b092bfe49152a4e532f6c47a366", "ed410923cb899ca9b985591f97db4eeb",
    "b10b3439906926e829531af9092d49c3", "4c0abb5321521506d6dd82cad743c5bb", "dad5abd30028edc9f2b58a82a2c62154",
    "385a53090a64ea638da73675e57af26e", "3381eab0147435ec47b6cceea43bc9f7", "66f3a5786ded03618e83cf0559ac07d5",
    "70cb63f503c0a480477abd4819fafa4b", "38785bcf08832cd952e8e03fd2e97fe3", "2a308da74bcf00e5443ea8d44711490b",
    "a4dda1c3cd1bbed78fe147fc96f3c449", "02e880b02320859564a3ee93af560bad", "ed2b06bd4fd31091a54b1739086423b9",
    "d6f30edc476ccdf8efde98c2093468fa", "0ca4576f5f1709aa282ff72b074808cf", "35ca496c0a8566a2f544101beb3c6b26",
    "90578ad37a02e59b9dbf3e0c278fa401", "169c4e28ce1e5c1954519ace57be64f3", "59a9840efb77ae92c0de834082c6fd05",
    "0826b6c29ed55fe7e985806e8d70a97a", "ce3e5f43e0fec03be8ab89be53ddd13e", "e53d96029124a706e330c1c0d6bd612e",
};

static const char *lossless10AV1[30] = {
    "ec6e4c1e736989a4ae3d38994555353d", "a1801b60ef450648bc11f1000dccbbb5", "7a8fb72b778d8ed76475d8993b6153a7",
    "1e775266e72f407f4eb48b04c2adb8b9", "760b854e841acb9e9fd47eaeae1e8f0a", "5e91d299a799babdb57f58eafe4d8141",
    "66df843e6de9381d7844381d19ae7470", "feaf085de6852f52f2d1063697d8e4f1", "d3c5e54027d7959ad5b7177d39a81564",
    "c22d20f79b7a676af66d6f5f6dd0be3f", "ec33a89d1eeaf232c5d162a6b33c1cd5", "63c4b1174b18bd3ef5a1096410a3dab9",
    "f1082a732e3df7d4a4f3205442b20f05", "cd170ca264ebb87a307e0c39143c4dcf", "8a254bacaee5ba951497ee21185828e5",
    "66dad625a67979e68cc4b7d46a4f3fd5", "ee6e93f50f4235fd7aa2d2e55075fdc1", "ff32548abff092545260b279fba43274",
    "444540e3519939b11e38198f6570bb32", "5c0b4b0bcc74da3c656209923561d34d", "21001a42d925b41ed8d14e5a74f64bc8",
    "35d033ec3c855cc6fe05939696e90afd", "74937bd021331243933deb3406df5b0d", "0a85bb965d2dc0ad689a433cb7e26b4a",
    "ece8749dd33ab26f761d3eeb4a20af3f", "892dab33e2c8dd6dfaaa878270330fc6", "59be22da25e38d322f3e35855725b570",
    "f0f386a9596b136b932d53a62b7c3d3e", "135ba4438ae0b5ea730ddb9f26bf2775", "a9ae505743ef0f7bd8be286b6b1d8afa",
};

static const char *loopfilter10AV1[30] = {
    "3da842f80be1583785df87a14d28f861", "3e629c6552343e3213569b160288908c", "e2e69fa47ba4346e7b425516cd3ad399",
    "5c4acfd736270ec614d4235d4b457c5f", "1bf85d94643a7edba6b1be0e273b1494", "db436d9f0556df36e7a61fe43b6ca8fa",
    "8653f145dff84ada80a06d73c89a8ade", "cd063b092bfe49152a4e532f6c47a366", "ed410923cb899ca9b985591f97db4eeb",
    "b10b3439906926e829531af9092d49c3", "9f4c48dfbe4c0db42150a99cf2047414", "a709a2a8640608a1801e1de3e46038ab",
    "385a53090a64ea638da73675e57af26e", "3381eab0147435ec47b6cceea43bc9f7", "66f3a5786ded03618e83cf0559ac07d5",
    "70cb63f503c0a480477abd4819fafa4b", "38785bcf08832cd952e8e03fd2e97fe3", "2a308da74bcf00e5443ea8d44711490b",
    "a4dda1c3cd1bbed78fe147fc96f3c449", "02e880b02320859564a3ee93af560bad", "ed2b06bd4fd31091a54b1739086423b9",
    "d6f30edc476ccdf8efde98c2093468fa", "0ca4576f5f1709aa282ff72b074808cf", "35ca496c0a8566a2f544101beb3c6b26",
    "90578ad37a02e59b9dbf3e0c278fa401", "169c4e28ce1e5c1954519ace57be64f3", "79ac385ed0f8fa599fb5132ebbac8db7",
    "0826b6c29ed55fe7e985806e8d70a97a", "ce3e5f43e0fec03be8ab89be53ddd13e", "e53d96029124a706e330c1c0d6bd612e",
};

static const char *cdef10AV1[30] = {
    "3da842f80be1583785df87a14d28f861", "8261a9e35cff8eabbf3e779bd78f5981", "dd1fa6eda334eac389b21f5682c9c4df",
    "5c4acfd736270ec614d4235d4b457c5f", "876c2faa39a73383b553a1811784cdcc", "3ce691eb5696b0fc9b034d4d71f15ae5",
    "8653f145dff84ada80a06d73c89a8ade", "df00929af0d158cae7d6cd33082d774f", "a6426ae7393cf89714e267c72dbd116a",
    "b10b3439906926e829531af9092d49c3", "445aa17e128abbacaf0d0aaefd321776", "fadedb1231f75c6dd9a1df4adeca67ef",
    "066eeaa0f54ae0081745f6ba4688173b", "3381eab0147435ec47b6cceea43bc9f7", "0c3204517ad123ab534e14e7ce7885f2",
    "70cb63f503c0a480477abd4819fafa4b", "110cde9ad14bcf0fa2dec80b66080100", "6a4e2740053104bc67aa06bdb5719bc6",
    "f4014867bb3906c384341abfddb1566c", "1347f9b4b7efc459d37d63355def45b8", "d5304e38fc41e8928315237290a05956",
    "3e23d946c34b58ef6ae14566cdbae096", "f28ce7d69c24218c1b67e1c700b3b6b4", "5f4772840b45f894c32c819c2fdf0be5",
    "82b1789a0244d0e16ae7b49ee78941de", "d2f65dc314cef5e431bcf2f7133e16d4", "d74e3617b5ded1e540e06036b3fdb366",
    "38c23b89c076ba027e18b93b9d7b439e", "79e2091067a4c3573c2aa2ad7664155f", "9e1a4353bab0f42459cab8f9dfe71845",
};

static const char *sizeupAV1[20] = {
    "3f20b55c5cfe25e4b5fec1b295abd383", // 352x288
    "fcc5539f98ed9c51280e46fa77f3c491", "d9b27a809788b6138774a665d76f8f20", "c14d7752f71479fcbdb173fa75d29dd5",
    "aac95c93e23f67c80dc7946d97ff1504", "3f3c9a7dcbc316933a68e7c9a0592e3b", "7ec5e34c09ec2a179ee22c04abad1183",
    "f271cda9802563b026274c37a339bade", "8b889fd48a9466b2dcda52985a5c3695", "037f485af956ffd97e00ec6c340d7677",

    "f3f3633e25c24e88c2057c4263f10875", // 1280x720
    "f7900a172b47abf17a14f2fb8d4462eb", "8e44be70dae5bcee3aa3902027be54f8", "4cf4f4eb58a1a0b1eaa27a4cb79330c4",
    "d61b38c688e72126a76812d40ebd982d", "0e9f6b0f797737994f4d37e4358d204b", "c51a2d765d4f56486c89c5c663aad4a6",
    "86b88efecf34dd7e4bb1d38346bc7a94", "02ec4d652fa265cfc928e0bc27ed783a", "120e9c7220f1803b796c487a28551382",
};

static const char *argonFilmgrain10[6] = {
    "cc05f657a3f6acaab634344e3181f149", "9bf462d19b14a2a4540d64f466f4b062", "9bf462d19b14a2a4540d64f466f4b062",
    "6c22eaa1a4537824bdd408dea694b5d5", "f2e8ea58b5a2e0645dd41ed1659e2c2e", "6c22eaa1a4537824bdd408dea694b5d5",
};

static const char *argonTest787[10] = {
    "63acc35f9a8ce92fb2b50a29b3dd64b5", "6a824e6e46aa9e7bbf93586beea35848", "fbf419f6edcb4ca94896ef62655e565a",
    "6a824e6e46aa9e7bbf93586beea35848", "0824f69abb11df1fd592b5f032f82a7f", "046b06462065ccc6b5015a560e2e28a1",
    "2f19e0f05601140cf9f03b001fec7494", "bcc5a7844a8dce5c38f553eca91a630b", "01f42dd70cb27bb804b792335ec4e6fa",
    "58cf033ef8015fc3a34d8e2f1213d160",
};

static auto H264_420_8BIT_HIGH_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_H264_PROFILE_IDC_HIGH};

static auto H264_420_8BIT_MAIN_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_H264_PROFILE_IDC_MAIN};

static auto H265_420_8BIT_MAIN_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_H265_PROFILE_IDC_MAIN};

static auto AV1_420_8BIT_MAIN_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN};

static auto AV1_420_10BIT_MAIN_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN};

static auto AV1_MONOCHROME_10BIT_MAIN_DECODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR, STD_VIDEO_AV1_PROFILE_MAIN};

static auto H264_420_8BIT_MAIN_ENCODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_H264_PROFILE_IDC_MAIN};
static auto H265_420_8BIT_MAIN_ENCODE_PROFILE = VideoProfileInfo{
    VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR, VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
    VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR, STD_VIDEO_H265_PROFILE_IDC_MAIN};

static ClipInfo Clips[] = {
    {
        CLIP_H264_DEC_A,
        "clip-a.h264",
        {H264_420_8BIT_HIGH_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        176,
        144,
        30,
        30,
        10,
        clipA,
    },
    {
        CLIP_H264_DEC_C,
        "clip-c.h264",
        {H264_420_8BIT_HIGH_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        352,
        288,
        30,
        30,
        10,
        clipC,
    },
    {
        CLIP_H265_DEC_D,
        "clip-d.h265",
        {H265_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        176,
        144,
        30,
        30,
        10,
        clipA, // same as clip A
    },
    {
        CLIP_H264_ENC_E,
        "vulkan/video/176x144_30_i420.yuv",
        {H264_420_8BIT_MAIN_ENCODE_PROFILE},
        ElementaryStreamFraming::UNKNOWN, // TODO: Encode tests shouldn't have been put in here
        176,
        144,
        24,
    },
    {
        CLIP_H265_ENC_F,
        "vulkan/video/176x144_30_i420.yuv",
        {H265_420_8BIT_MAIN_ENCODE_PROFILE},
        ElementaryStreamFraming::UNKNOWN,
        176,
        144,
        24,
    },
    {
        CLIP_H264_ENC_G,
        "vulkan/video/352x288_15_i420.yuv",
        {H264_420_8BIT_MAIN_ENCODE_PROFILE},
        ElementaryStreamFraming::UNKNOWN,
        352,
        288,
        15,
    },
    {
        CLIP_H265_ENC_H,
        "vulkan/video/352x288_15_i420.yuv",
        {H265_420_8BIT_MAIN_ENCODE_PROFILE},
        ElementaryStreamFraming::UNKNOWN,
        352,
        288,
        15,
    },
    {
        CLIP_H264_DEC_4K_26_IBP_MAIN,
        "4k_26_ibp_main.h264",
        {H264_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        3840,
        2160,
        30,
        26,
        13,
        h264_4k_26_ibp_main,
    },
    {
        CLIP_H265_DEC_JELLY,
        "jellyfish-250-mbps-4k-uhd-GOB-IPB13.h265",
        {H265_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        3840,
        2160,
        30,
        26,
        26,
        jellyfishHEVC,
    },
    {
        CLIP_H265_DEC_ITU_SLIST_A,
        "hevc-itu-slist-a.h265",
        {H265_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        832,
        480,
        65,
        65,
        65,
        slistAHEVC,
    },
    {
        CLIP_H265_DEC_ITU_SLIST_B,
        "hevc-itu-slist-b.h265",
        {H265_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::H26X_BYTE_STREAM,
        832,
        480,
        65,
        65,
        65,
        slistBHEVC,
    },
    {
        CLIP_AV1_DEC_BASIC_8,
        "av1-176x144-main-basic-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        176,
        144,
        0,
        30,
        0,
        basic8AV1,
    },
    {
        CLIP_AV1_DEC_ALLINTRA_8,
        "av1-352x288-main-allintra-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        352,
        288,
        0,
        30,
        0,
        allintra8AV1,
    },
    {
        CLIP_AV1_DEC_ALLINTRA_INTRABC_8,
        "av1-1920x1080-intrabc-extreme-dv-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        1920,
        1080,
        0,
        2,
        0,
        allintrabc8AV1,
    },
    {
        CLIP_AV1_DEC_CDFUPDATE_8,
        "av1-352x288-main-cdfupdate-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        352,
        288,
        0,
        2,
        0,
        cdfupdate8AV1,
    },
    {
        CLIP_AV1_DEC_GLOBALMOTION_8,
        "av1-176x144-main-globalmotion-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        176,
        144,
        0,
        30,
        0,
        globalmotion10AV1,
    },
    {
        CLIP_AV1_DEC_FILMGRAIN_8,
        "av1-352x288-main-filmgrain-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        352,
        288,
        0,
        10,
        0,
        filmgrain8AV1,
    },
    {
        CLIP_AV1_DEC_SVCL1T2_8,
        "av1-640x360-main-svc-L1T2-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        640,
        360,
        0,
        8,
        0,
        svcL1T28AV1,
    },
    {
        CLIP_AV1_DEC_SUPERRES_8,
        "av1-1920x1080-main-superres-8.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        1920,
        1080,
        0,
        10,
        0,
        superres8AV1,
    },
    {
        CLIP_AV1_DEC_SIZEUP_8,
        "av1-sizeup-fluster.ivf",
        {AV1_420_8BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        20,
        0,
        sizeupAV1,
    },
    // 10-bit AV1 test cases
    {
        CLIP_AV1_DEC_BASIC_10,
        "av1-176x144-main-basic-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        basic10AV1,
    },
    {
        CLIP_AV1_DEC_ORDERHINT_10,
        "av1-176x144-main-orderhint-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        orderhint10AV1,
    },
    {
        CLIP_AV1_DEC_FORWARDKEYFRAME_10,
        "av1-176x144-main-forward-key-frame-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        forwardKeyframe10AV1,
    },
    {
        CLIP_AV1_DEC_LOSSLESS_10,
        "av1-176x144-main-lossless-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        lossless10AV1,
    },
    {
        CLIP_AV1_DEC_LOOPFILTER_10,
        "av1-176x144-main-loop-filter-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        loopfilter10AV1,
    },
    {
        CLIP_AV1_DEC_CDEF_10,
        "av1-176x144-main-cdef-10.ivf",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::IVF,
        0,
        0,
        0,
        30,
        0,
        cdef10AV1,
    },
    {
        CLIP_AV1_DEC_ARGON_FILMGRAIN_10,
        "av1-argon_test1019.obu",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::AV1_ANNEXB,
        0,
        0,
        0,
        6,
        0,
        argonFilmgrain10,
    },
    {
        CLIP_AV1_DEC_ARGON_TEST_787,
        "av1-argon_test787.obu",
        {AV1_420_10BIT_MAIN_DECODE_PROFILE},
        ElementaryStreamFraming::AV1_ANNEXB,
        0,
        0,
        0,
        10,
        0,
        argonTest787,
    },
};

const ClipInfo *clipInfo(ClipName c)
{
    DE_ASSERT(c >= 0 && c < DE_LENGTH_OF_ARRAY(Clips));
    return &Clips[c];
}

const char *checksumForClipFrame(const ClipInfo *cinfo, int frameNumber)
{
    DE_ASSERT(frameNumber >= 0 && frameNumber < cinfo->totalFrames);
    return cinfo->frameChecksums[frameNumber];
}

} // namespace video
} // namespace vkt
