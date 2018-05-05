#pragma once
#include<stdexcept>
#include<cinttypes>
#include<intrin.h> 
#include<tuple>

//Implementation is based off paper 
//Space-Efficient, High-Performance Rank & Select Structures on Uncompressed Bit Sequences, June 2013
//by Dong Zhou, David G. Andersen, Michael Kaminsky

namespace Succinct{
  class  BitArray{
  protected:
    uint64_t *mBaseAddr;
    uint64_t  mMaxBitCount;//total bits
    uint64_t  mBitCount;

  protected:
#pragma optimize("",off)
    inline bool BackOff(int ammount){
      while(ammount-->0){}
      return true;
    }
#pragma optimize("",on)

    static uint64_t MemAlign(uint64_t size,uint64_t align);
  public:
    static const uint64_t mBlockSize=64;

    static uint64_t RequiredSize(uint64_t count);
    //BitArray works on 512bit block size
    BitArray(uint64_t *baseAddr,uint64_t MaxBits,uint64_t initalBitCount);
    ~BitArray();

    constexpr size_t Size()const{
      return mBitCount;
    }

    //skip is used by derived succinct levels for quicker scanning
    uint64_t Rank(uint64_t pos,uint64_t skip=0)const;
    uint64_t Select(uint64_t count,uint64_t skip=0)const;
    uint64_t Range(uint64_t pos,uint64_t limit)const;

    void Set(uint64_t pos);
    void Clear(uint64_t pos);
    bool Get(uint64_t pos)const;

    uint64_t Insert(uint64_t pos,size_t ammount,uint64_t value=0);
    uint64_t Remove(uint64_t pos,size_t ammount,uint64_t carry=0);

    //Moves null bytes to the end of the array, 
    //then recalculates the bit count
    void Compact();

    void Slice(BitArray &destination,uint64_t from,uint64_t to);
//    void Copy(const Succinct *b);
    
//    Succinct* operator=(const Succinct *b);
//    bool operator==(const Succinct *b)const;

//    static std::tuple<uint64_t,uint64_t*,BitArray*> Allocate(uint64_t bitCount);
  };
}