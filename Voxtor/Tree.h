#pragma once
#include<cinttypes>
#include<vector>
#include<tuple>
#include"Succinct\Array.h"
#include"Vector.h"
#include"Index.h"

class Tree{
public:
  using MapValues=std::vector<uint64_t>;

private:
  struct StackPosition{
    uint64_t mOffset;
    uint64_t mIndex;

    inline operator uint64_t() const{ return mOffset+mIndex; };
  };

  using CursorValues=std::vector<StackPosition>;
  using CursorPair=std::tuple<StackPosition,size_t>;

private:
  bool mIsDense;
  std::vector<Succinct::Array> mStack;

  mutable MapValues            mCursorStack;
  mutable Index                mCursorIndex;
  mutable uint64_t             mCursorPostion;

protected:
  CursorPair                   Cursor      ()const;
  inline void                  CursorRewind()const{ mCursorPostion=0; };
  void                         CursorClear ()const;
  inline size_t                CursorSize  ()const{ return mCursorStack.size(); }
  bool                         CursorPop   ()const;

  CursorPair                   CursorUpdate(Vector vec)const;

  CursorPair                   CursorPrev  ()const;
  CursorPair                   CursorNext  ()const;
  
  CursorPair                   CursorWalk  ()const;

public:
  Tree(size_t depth,bool dense);
  ~Tree();

  inline size_t                Depth(){ return mStack.size();}

  //In sparse mode the size values are not fix
  //and can change with inserts

  //Bytes allocated for this tree (bitArray and counters)
  //Does not include memory needed for classes
  size_t                       AllocatedSize()const;
  //Total number of bits available
  size_t                       ReserveSize()const;
  //Total number of bits used in the tree
  size_t                       Size()const;

  Vector                       Dimensions()const;

  bool                         Empty()const;

  bool                         Get(Vector vec)const;
  void                         Set(Vector vec);
  void                         Clear(Vector vec);

  uint64_t                     NodeCount()const;
  uint64_t                     LeafCount()const;

  MapValues                    Map(Vector vec)const;

  void                         ToSparse();
  //Removes the extra unused space from the tree
  //Usefull for sparse trees
  void                         Compact();

  //TODO: Next milestone
  //Prune
  //Slice
};

