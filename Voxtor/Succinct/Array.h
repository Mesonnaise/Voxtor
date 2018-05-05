#pragma once
#include"L0Block.h"
namespace Succinct{
  class Array{
  public:
    const static size_t mBlockAllocateSize=2351104;

  private:
    L0Block            *mRootBlock;
    size_t              mBlockSize;

    mutable L0Block    *mCursorBlock;
    mutable uint64_t    mCursorOffset=0;

    bool                mIsFixed=false;
    mutable bool        mIsValid=false;

  protected:
    constexpr L0Block* Cursor()const{ return mCursorBlock;}
    L0Block*           CursorReset()const;
    L0Block*           CursorSeek(uint64_t &pos)const;
    L0Block*           CursorPrev()const;
    L0Block*           CursorNext()const;
  public:
    Array(uint64_t level,bool dense);
    ~Array();

    constexpr bool     Fixed()const{ return mIsFixed; }
    inline    void     Fixed(bool fixed){ mIsFixed=fixed; }

    size_t             AllocatedSize()const;

    size_t             ReserveSize()const;

    size_t             Size()const;
    void               Size(size_t size);

    bool               Empty()const;

  //  constexpr bool     Full(){ return 0; }

    uint64_t           Rank(uint64_t pos)const;
    uint64_t           Select(uint64_t count)const;

    bool               Get(uint64_t pos)const;
    void               Set(uint64_t pos);
    void               Clear(uint64_t pos);

    uint64_t           GetGroup(uint64_t pos,size_t size)const;

    uint64_t           PopCount()const;

    uint64_t           Insert(uint64_t pos,size_t ammount,uint64_t value=0);
    uint64_t           Remove(uint64_t pos,size_t ammount,uint64_t carry=0);

    size_t             Filter(Array *mask,uint64_t maskOffset,uint64_t maskScale);

    void               Compact();

    size_t             Slice(Array *destination,uint64_t start,uint64_t end);
   // size_t             Clone(Array *destination);
  };
}
