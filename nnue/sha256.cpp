/* 9/30/2026 12:05: Verify the complete target network before parameter decoding. */
#include "sha256.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
constexpr uint32_t constants[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
uint32_t rotate(uint32_t x,unsigned n) { return (x>>n)|(x<<(32-n)); }
void compress(std::array<uint32_t,8>& h,const unsigned char *block) {
    uint32_t w[64];
    for(int i=0;i<16;++i) w[i]=(uint32_t(block[i*4])<<24)|(uint32_t(block[i*4+1])<<16)|(uint32_t(block[i*4+2])<<8)|block[i*4+3];
    for(int i=16;i<64;++i) {
        uint32_t x=w[i-15],y=w[i-2];
        w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));
    }
    auto v=h;
    for(int i=0;i<64;++i) {
        uint32_t t1=v[7]+(rotate(v[4],6)^rotate(v[4],11)^rotate(v[4],25))+((v[4]&v[5])^(~v[4]&v[6]))+constants[i]+w[i];
        uint32_t t2=(rotate(v[0],2)^rotate(v[0],13)^rotate(v[0],22))+((v[0]&v[1])^(v[0]&v[2])^(v[1]&v[2]));
        v={t1+t2,v[0],v[1],v[2],v[3]+t1,v[4],v[5],v[6]};
    }
    for(int i=0;i<8;++i) h[i]+=v[i];
}
}
std::string cce_file_sha256(const char *path) {
    std::ifstream input(path,std::ios::binary);
    if(!input) return {};
    std::array<uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::array<unsigned char,65536> buffer{};
    uint64_t length=0;size_t tail=0;
    while(input) {
        input.read(reinterpret_cast<char*>(buffer.data()),buffer.size());
        size_t got=size_t(input.gcount());length+=got;
        size_t full=got/64;
        for(size_t i=0;i<full;++i) compress(h,buffer.data()+i*64);
        tail=got%64;
        if(tail) { for(size_t i=0;i<tail;++i) buffer[i]=buffer[full*64+i]; }
    }
    if(!input.eof()) return {};
    buffer[tail++]=0x80;
    size_t padded=tail<=56?64:128;
    for(size_t i=tail;i<padded;++i) buffer[i]=0;
    uint64_t bits=length*8;
    for(int i=0;i<8;++i) buffer[padded-1-i]=static_cast<unsigned char>(bits>>(i*8));
    compress(h,buffer.data());if(padded==128) compress(h,buffer.data()+64);
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(auto word:h) out<<std::setw(8)<<word;
    return out.str();
}
