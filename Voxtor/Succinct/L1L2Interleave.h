#pragma once
#include<tuple>
#include"BitArray.h"
namespace Succinct{
  class L1L2Interleave:public BitArray{
  public:
    using CTPair=std::tuple<uint64_t,uint64_t>;
    static const uint64_t mBlockSize=512;//2048;

  protected:
    uint64_t *mBaseAddr=nullptr;
    uint64_t  mL1BlockCount=0;

  protected:
    void L2Inc(uint64_t *addr,int pos);
    void L2Dec(uint64_t *val,int pos);
    void L2Set(uint64_t *val,int pos,uint64_t count);

    uint64_t Rebuild(uint64_t pos);

  public:
    L1L2Interleave(uint64_t MaxBits,uint64_t initalBitCount,bool immediate);
    ~L1L2Interleave();

    uint64_t Rank(uint64_t pos)const;
    uint64_t Select(uint64_t count)const;

    void Set(uint64_t pos);
    void Clear(uint64_t pos);

    CTPair Insert(uint64_t pos,size_t ammount,uint64_t value=0);
    CTPair Remove(uint64_t pos,size_t ammount);
  };
}