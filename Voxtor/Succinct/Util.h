#pragma once
#include<cinttypes>
#include<algorithm>
#include<intrin.h>

inline uint64_t le(uint64_t x,uint64_t y){
  uint64_t t=(y|0x8080808080808080ULL)-(x&~0x8080808080808080ULL);
  return (t^x^y)&0x8080808080808080ULL;
}

inline uint64_t lt(uint64_t x,uint64_t y){
  uint64_t t=(x|0x8080808080808080ULL)-(y&~0x8080808080808080ULL);
  return (t^x^~y)&0x8080808080808080ULL;
}

inline uint64_t BwordSelect(uint64_t x,uint64_t k){
  //Implementation is based off paper 
  //Broadword Implementation of Rank/Select Queries, Nov 19,2014
  //by Sebastiano Vigna

  uint64_t s,b,l;
  //Individual bit count per byte. 
  s=x-((x>>1)&0x5555555555555555ULL);
  s=(s&0x3333333333333333ULL)+((s>>2)&0x3333333333333333ULL);
  s=(s+(s>>4))&0x0F0F0F0F0F0F0F0FULL;
  s*=0x0101010101010101ULL;


  //Compaire one's count k to the bytes s and
  //use the results to find the byte where the k'th bit is. 
  //b=k*0x0101010101010101ULL;
  //b=__popcnt64(le(s,b))*8;

  //Fixed a flaw in the papers implementation, where leading zeros
  //in x would results in most significant byte being asigned to b
  //instead of the actual byte because sideways addition would copy
  //the same value for each zero filled byte. 
  b=k*0x0101010101010101ULL;
  b=(8-__popcnt64(le(b,s)))*8;

  //Old method for popcount
  //b=le(s,k*0x0101010101010101ULL)>>7;
  //b=((b*0x0101010101010101ULL)>>53)&~0x07;

  l=(s<<8)>>b;
  l=k-(l&0xFF);
  //Move the byte where k'th bit resides to the LSB
  s=(x>>b)&0xFF;


  //Spread the byte with target bit a cross a full 64bit word
  //Then a less than operation is used to move any set bits to the 
  //most significant bit for each 8 bit word.
  //The spread operation is fallowed by bitshift to LSB, so 
  //a sideways addition can be performed. 

  //BMI2's PDEP instruction can be used as an 
  //alterative for spread operation
  s=_pdep_u64(s,0x0101010101010101ULL)*0x0101010101010101ULL;
  //s=(s*0x0101010101010101ULL)&0x8040201008040201;
  //s=(lt(0,s)>>7)*0x0101010101010101ULL;

  s=__popcnt64(lt(s,l*0x0101010101010101ULL));

  return b+s;
}


inline uint64_t ShiftLeft64(uint64_t *baseAddr,const uint64_t *endAddr,const uint64_t shift,uint64_t carry){
  //Implementation of logical shift left for buffers/arrays
  //Will shift entire buffer left up to 63bits
  const uint64_t invShift=64-shift;
  const uint64_t upperMask=0xFFFFFFFFFFFFFFFFULL<<invShift;
  const uint64_t lowerMask=0xFFFFFFFFFFFFFFFFULL>>invShift;

  while(baseAddr<endAddr){
    carry|=*baseAddr&upperMask;
    *baseAddr<<=shift;
    *baseAddr++|=carry&lowerMask;
    carry>>=invShift;
  }

  return carry;
}

inline uint64_t ShiftRight64(uint64_t *baseAddr,const uint64_t *endAddr,const uint64_t shift,uint64_t carry){
  //Implementation of logical shift right for buffers/arrays
  //Will shift entire buffer right up to 63bits
  const uint64_t invShift=64-shift;
  const uint64_t upperMask=0xFFFFFFFFFFFFFFFFULL<<invShift;
  const uint64_t lowerMask=0xFFFFFFFFFFFFFFFFULL>>invShift;

  while(baseAddr>endAddr){
    carry|=*--baseAddr&lowerMask;
    *baseAddr>>=shift;
    *baseAddr|=carry&upperMask;
    carry<<=invShift;
  }

  return carry;
}

inline uint64_t ShiftLeft256(uint64_t *baseAddr,const uint64_t *endAddr,uint64_t shift,uint64_t carry){
  //Implementation of logical shift left for large buffers/arrays that are 256bit aligned. 
  //Will shift entire buffer left up to 63bits
  const uint64_t invShift=64-shift;
  const uint64_t upperMask=0xFFFFFFFFFFFFFFFFULL<<invShift;
  const uint64_t lowerMask=0xFFFFFFFFFFFFFFFFULL>>invShift;

  __m256i laneCarryMask=     _mm256_set1_epi64x(upperMask);

  __m256i laneToRolloverMask=_mm256_setr_epi64x(lowerMask            ,0x0000000000000000ULL,
                                                0x0000000000000000ULL,0x0000000000000000ULL);

  __m256i clearRolloverMask= _mm256_setr_epi64x(0x0000000000000000ULL,lowerMask            ,
                                                lowerMask            ,lowerMask            );

  __m256i laneCarry=         _mm256_setr_epi64x(0x0000000000000000ULL,0x0000000000000000ULL,
                                                0x0000000000000000ULL,0x0000000000000000ULL);

  __m256i rolloverCarry=     _mm256_setr_epi64x(carry                ,0x0000000000000000ULL,
                                                0x0000000000000000ULL,0x0000000000000000ULL);


  __m256i intermediate=      _mm256_set1_epi64x(0);

  __m256i       *addrAVX=   reinterpret_cast<__m256i*>(baseAddr);
  const __m256i *addrEndAVX=reinterpret_cast<const __m256i*>(endAddr);

  while(addrAVX<addrEndAVX){
    //Copy the higher order bits that will be placed into the next 64bit word. 
    laneCarry=_mm256_and_si256(*addrAVX,laneCarryMask);
    //Rotates the 4 64bit words right by one. [0,1,2,3]=>[3,0,1,2]
    laneCarry=_mm256_permute4x64_epi64(laneCarry,0x93);

    //Perform the left bitshift.
    intermediate=_mm256_slli_epi64(*addrAVX,shift);
    //Move the higher order carried bits to fill in the holes created by left bitshift.
    laneCarry=_mm256_srli_epi64(laneCarry,invShift);

    //Place the higher order bits left over from last pass into the lower order hole. 
    intermediate=_mm256_or_si256(intermediate,rolloverCarry);
    //Copy the higher order bits need for the next pass from register used by laneCarry 
    rolloverCarry=_mm256_and_si256(laneCarry,laneToRolloverMask);

    //Finish off by placing the lower order left over from the rotation
    intermediate=_mm256_or_si256(intermediate,_mm256_and_si256(laneCarry,clearRolloverMask));
    _mm256_stream_si256(addrAVX++,intermediate);
  }

  //return the bits left over in rollover carry
  return _mm256_extract_epi64(rolloverCarry,0);
 // return _mm_extract_epi64(_mm256_extractf128_si256(rolloverCarry,0),0);
}

inline uint64_t ShiftRight256(uint64_t *baseAddr,const uint64_t *endAddr,uint64_t shift,uint64_t carry){
  //Implementation of logical shift right for large buffers/arrays that are 256bit aligned. 
  //Will shift entire buffer right up to 63bits
  const uint64_t invShift=64-shift;
  const uint64_t upperMask=0xFFFFFFFFFFFFFFFFULL<<invShift;
  const uint64_t lowerMask=0xFFFFFFFFFFFFFFFFULL>>invShift;

  __m256i laneCarryMask=     _mm256_set1_epi64x(lowerMask);

  __m256i laneToRolloverMask=_mm256_setr_epi64x(0x0000000000000000ULL,0x0000000000000000ULL,
                                                0x0000000000000000ULL,            upperMask);

  __m256i clearRolloverMask= _mm256_setr_epi64x(upperMask            ,            upperMask,
                                                upperMask            ,0x0000000000000000ULL);

  __m256i laneCarry=         _mm256_setr_epi64x(0x0000000000000000ULL,0x0000000000000000ULL,
                                                0x0000000000000000ULL,0x0000000000000000ULL);

  __m256i rolloverCarry=     _mm256_setr_epi64x(0x0000000000000000ULL,0x0000000000000000ULL,
                                                0x0000000000000000ULL,                carry);

  __m256i intermediate=      _mm256_set1_epi64x(0);

  __m256i       *addrAVX=   reinterpret_cast<__m256i*>(baseAddr);
  const __m256i *addrEndAVX=reinterpret_cast<const __m256i*>(endAddr);

  while(addrAVX>addrEndAVX){
    laneCarry=_mm256_and_si256(*--addrAVX,laneCarryMask);
    laneCarry=_mm256_permute4x64_epi64(laneCarry,0x39);

    intermediate=_mm256_srli_epi64(*addrAVX,shift);
    laneCarry=_mm256_slli_epi64(laneCarry,invShift);

    intermediate=_mm256_or_si256(intermediate,rolloverCarry);
    rolloverCarry=_mm256_and_si256(laneCarry,laneToRolloverMask);

    intermediate=_mm256_or_si256(intermediate,_mm256_and_si256(laneCarry,clearRolloverMask));
    _mm256_stream_si256(addrAVX,intermediate);
  }

  return _mm256_extract_epi64(rolloverCarry,3);
  //return _mm_extract_epi64(_mm256_extractf128_si256(rolloverCarry,1),1);
}

inline uint64_t ShiftLeft(uint64_t *baseAddr,uint64_t *endAddr,uint64_t shift,uint64_t carry){
  uint64_t *AVXEnd=(uint64_t*)((uint64_t)endAddr&0xFFFFFFFFFFFFFFE0ULL);
  carry=ShiftLeft256(baseAddr,AVXEnd,shift,carry);

  return ShiftLeft64(AVXEnd,endAddr,shift,carry);
}

inline uint64_t ShiftRight(uint64_t *baseAddr,const uint64_t *endAddr,uint64_t shift,uint64_t carry){
  uint64_t *AVXStart=(uint64_t*)((uint64_t)baseAddr&0xFFFFFFFFFFFFFFE0ULL);
  carry=ShiftRight64(baseAddr,AVXStart,shift,carry);

  return ShiftRight256(AVXStart,endAddr,shift,carry);
}

//Was going to be used for something, not any more
inline __m256i MaskRight(uint64_t position){
  //Creates a from the LSB to words the MSB in a 256Bit register
  //This treats a 256Bit register as a single scalar
  uint64_t subPos=0xFFFFFFFFFFFFFF01ULL<<((position&0x00000000000000E0ULL)>>2);
  __m256i permutePos=_mm256_cvtepi8_epi32(_mm_cvtsi64_si128(subPos));

  uint32_t subMask=0xFFFFFFFFUL<<(position&0x000000000000001FULL);

  __m256i mask=_mm256_setr_epi32(0x00000000UL,subMask,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xFFFFFFFFUL);
  return _mm256_permutevar8x32_epi32(mask,permutePos);
}

uint64_t ZeroByteCompact(uint64_t *baseAddr,const uint64_t *addrEnd){
  //Removes any zero bytes from a buffer/array and 
  //compacts the remaining bytes to wards lower address (baseAddr)
  //starting and ending address need to be 256bit aligned
  __m256i       *addrExtract=   reinterpret_cast<__m256i*>(baseAddr);
  const __m256i *addrExtractEnd=reinterpret_cast<const __m256i*>(addrEnd);

  uint64_t packed;
  uint64_t packedSize;

  uint64_t offset=0;
  uint64_t used=0;

  while(addrExtract<addrExtractEnd){
    __m256i extract=*addrExtract++;
    //Create the masks needed for pext (Parallel bit extract)
    //0x00 for zero values and 0xFF for any other value
    __m256i mask=_mm256_cmpeq_epi8(extract,_mm256_setzero_si256());
    mask=_mm256_xor_si256(mask,_mm256_set1_epi64x(0xFFFFFFFFFFFFFFFFULL));

    for(int i=0;i<4;i++){
      //Extract only set bytes from the first quadword
      packed=_pext_u64(_mm256_extract_epi64(extract,0),
                       _mm256_extract_epi64(mask   ,0));

      //Rotate the quadwords left by one lane
      extract=_mm256_permute4x64_epi64(extract,0x39);
      mask   =_mm256_permute4x64_epi64(mask   ,0x39);

      //Count the number of set byte on the quadword
      packedSize=_lzcnt_u64(packed);
      packedSize&=0xFFFFFFFFFFFFFFF8ULL;
      packedSize=64-packedSize;

      //Insert the now packed quadword into the depositing address and
      //take care of any miss alignment where depositing would take place across quadword
      while(packedSize){
        *baseAddr&=~(0xFFFFFFFFFFFFFFFFULL<<offset);
        *baseAddr|=packed<<offset;

        used=std::min(packedSize,64-offset);

        packedSize-=used;
        offset=offset+used;

        baseAddr+=offset>>6;

        offset&=0x000000000000003FULL;
        packed>>=used;
      }
    }
  }
  
  //count how many zero exist at the end of the buffer/array
  uint64_t zeroCount=8-(offset>>3);

  *baseAddr++&=~(0xFFFFFFFFFFFFFFFFULL<<offset);

  zeroCount+=reinterpret_cast<uint64_t>(addrEnd)-
             reinterpret_cast<uint64_t>(baseAddr);

  //Fill the remaining quadwords with zeros
  while(baseAddr<addrEnd)
    *baseAddr++=0x0000000000000000ULL;
  return zeroCount;
}

//It does magic
uint64_t ExtractByte(uint64_t *baseAddr,const uint64_t *addrEnd,
                     uint64_t *maskAddr,const uint64_t *maskEnd){
  uint64_t *extAddr=baseAddr;

  uint64_t mask;
  uint64_t maskedOffset=0;

  uint64_t packed;
  uint64_t packedSize;

  uint64_t offset=0;
  uint64_t used=0;

  while(extAddr<addrEnd&&maskAddr<maskEnd){
    mask=_pdep_u64(*maskAddr>>maskedOffset,0x0101010101010101ULL);
    mask*=0x00000000000000FFULL;

    maskedOffset+=8;
    maskAddr+=maskedOffset>>6;
    maskedOffset&=0x000000000000003FULL;

    packedSize=__popcnt64(mask);
    packed=_pext_u64(*extAddr++,mask);

    //Insert the now packed quadword into the depositing address and
    //take care of any miss alignment where depositing would take place across quadword
    while(packedSize){
      *baseAddr&=~(0xFFFFFFFFFFFFFFFFULL<<offset);
      *baseAddr|=packed<<offset;

      used=std::min(packedSize,64-offset);

      packedSize-=used;
      offset=offset+used;

      baseAddr+=offset>>6;

      offset&=0x000000000000003FULL;
      packed>>=used;
    }
  }

  //count how many zero exist at the end of the buffer/array
  uint64_t zeroCount=8-(offset>>3);

  *baseAddr++&=~(0xFFFFFFFFFFFFFFFFULL<<offset);

  zeroCount+=reinterpret_cast<uint64_t>(addrEnd)-
    reinterpret_cast<uint64_t>(baseAddr);

  //Fill the remaining quadwords with zeros
//  while(baseAddr<addrEnd)
//    *baseAddr++=0x0000000000000000ULL;

  return zeroCount;
}

//Not worth it for popcounts as short as 64Bytes
inline uint64_t PopCnt2Pass(uint64_t *baseAddr) {

  __m256i lookupTable=_mm256_setr_epi8(0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
                                       0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4);

  __m256i magicNumber=_mm256_set1_epi64x(0x0101010101010101ULL);

  __m256i lowerNibbleMask=_mm256_set1_epi8(0x0fU);

  __m256i lowerNibble=_mm256_set1_epi64x(0x0000000000000000ULL);
  __m256i upperNibble=_mm256_set1_epi64x(0x0000000000000000ULL);

  __m256i lowerNibblePopCnt=_mm256_set1_epi64x(0x0000000000000000ULL);
  __m256i upperNibblePopCnt=_mm256_set1_epi64x(0x0000000000000000ULL);

  __m256i accumulator=_mm256_set1_epi64x(0x0000000000000000ULL);
  __m256i total=_mm256_set1_epi64x(0x0000000000000000ULL);

  __m256i       *addrAVX=reinterpret_cast<__m256i*>(baseAddr);

  for(int i=0;i<2;i++){
    lowerNibble=_mm256_and_si256(*addrAVX,lowerNibbleMask);
    upperNibble=_mm256_srli_epi64(*addrAVX++,4);
    upperNibble=_mm256_and_si256(upperNibble,lowerNibbleMask);

    lowerNibblePopCnt=_mm256_shuffle_epi8(lookupTable,lowerNibble);
    upperNibblePopCnt=_mm256_shuffle_epi8(lookupTable,upperNibble);

    accumulator=_mm256_add_epi8(lowerNibblePopCnt,upperNibblePopCnt);

    
    total=_mm256_add_epi64(total,accumulator);
  }

  total=_mm256_sad_epu8(total,_mm256_setzero_si256());
  return _mm256_extract_epi64(total,0)+_mm256_extract_epi64(total,1)+
         _mm256_extract_epi64(total,2)+_mm256_extract_epi64(total,3);
}