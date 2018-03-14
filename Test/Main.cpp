#include<intrin.h>
#include<cinttypes>
#include<algorithm>
#include<random>
#include"Succinct\Util.h"

uint64_t Compact(uint64_t *addrStart,const uint64_t *addrEnd){
  __m256i       *addrExtract   =reinterpret_cast<__m256i*>(addrStart);
  const __m256i *addrExtractEnd=reinterpret_cast<const __m256i*>(addrEnd);

  uint64_t *deposit=addrStart;

  uint64_t packed;
  uint64_t packedSize;

  uint64_t offset=0;
  uint64_t tmp=0;

  while(addrExtract<addrExtractEnd){
    __m256i extract=*addrExtract++;
    __m256i mask=_mm256_cmpeq_epi8(extract,_mm256_setzero_si256());
    mask=_mm256_xor_si256(mask,_mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFULL));
    
    for(int i=0;i<4;i++){

      packed=_pext_u64(_mm256_extract_epi64(extract,0),
                       _mm256_extract_epi64(mask   ,0));

      extract=_mm256_permute4x64_epi64(extract,0x39);
      mask=_mm256_permute4x64_epi64(mask,0x39);

      packedSize=_lzcnt_u64(packed);
      packedSize&=0xFFFFFFFFFFFFFFF8ULL;
      packedSize=64-packedSize;
      
      while(packedSize){
        *deposit&=~(0xFFFFFFFFFFFFFFFFULL<<offset);
        *deposit|=packed<<offset;

        tmp=std::min(packedSize,64-offset);
        
        packedSize-=tmp;
        offset=offset+tmp;

        deposit+=offset>>6;

        offset&=0x000000000000003FULL;
        packed>>=tmp;
      }
    }
  }

  uint64_t zeroCount=8-(offset>>3);
 
  *deposit++&=~(0xFFFFFFFFFFFFFFFFULL<<offset);

  zeroCount+=reinterpret_cast<uint64_t>(addrEnd)-
             reinterpret_cast<uint64_t>(deposit);

  while(deposit<addrEnd)
    *deposit++=0x0000000000000000ULL;
  
  return zeroCount;
}

int main(){
  std::mt19937_64 generator;
  std::uniform_int_distribution<uint64_t> distribution(0,0xFFFFFFFFFFFFFFFFULL);
  uint64_t deposit[1024]={0};
  uint64_t posDeposit=0;
  
  for(int i=0;i<1024;i++){
    deposit[i]=distribution(generator);
  }

  ExtractByte(deposit,deposit+1024,deposit,deposit+1024);
  
  return 0;
}