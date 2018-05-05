#pragma once
#include<optional>
#include"L0Block.h"
namespace Succinct{

  class Sparse{
  public:
    struct Cache{
      uint64_t  mOffset=0;//Offset in the whole Sparse Array
      L0Block  *mBlock=nullptr;
    };
  protected:
    uint64_t   mRevision=0;
    L0Block   *mRoot=nullptr;
    size_t     mSize=0;
    size_t     mBitCount;

  protected:
    L0Block*  FindBlock(L0Block *curBlock,uint64_t &subCount,uint64_t &pos)const;

    inline void CacheAssign(Sparse::Cache *c,uint64_t global,uint64_t pos,L0Block *block)const{
      if(c){
        c->mOffset=global;
//        c->mSegmentOffset=pos;
        c->mBlock=block;
      }
    }
    inline void CacheVars(Cache *c,uint64_t &global,uint64_t &pos,L0Block *&block)const{
      global=0;
      block=mRoot;

      if(c&&pos<c->mOffset){
        global=c->mOffset;
        block=c->mBlock?c->mBlock:block;
        pos-=global;
      }
    }

  public:
    Sparse(size_t initalSize);
    ~Sparse();

    constexpr size_t Size()const{
      return mSize;
    }
    constexpr size_t BitCount()const{
      return mBitCount;
    }
    constexpr uint64_t Revision()const{
      return mRevision;
    }
    constexpr uint64_t PopCount()const{
      L0Block *curBlock=mRoot;

      while(curBlock->Next())
        curBlock=curBlock->Next();

      return curBlock->PopCount();
    }

    uint64_t Rank(uint64_t pos,Cache *c=nullptr)const;
    uint64_t Select(uint64_t count)const;
   // uint64_t Range(uint64_t pos,uint64_t limit)const;

    void Set(uint64_t pos,Cache *c=nullptr);
    void Clear(uint64_t pos,Cache *c=nullptr);
    bool Get(uint64_t pos,Cache *c=nullptr)const;

    void Insert(uint64_t pos,size_t ammount,uint64_t value,Cache *c=nullptr);
    uint64_t Remove(uint64_t pos,size_t ammount);


    void Filter(Sparse *mask,uint64_t maskScale);
    //Removes the gaps out of the array
    void Compact();


    size_t MemUsage()const;
  };
}