#pragma once
#include<cinttypes>
#include<vector>
#include"Succinct\Sparse.h"
#include"Index.h"
class Tree{
public:
  struct Level{
    uint64_t mPosition;
    uint64_t mSize;
  };
protected:
  bool mIsDense;
  std::vector<Succinct::Sparse> mStack;
public:
  Tree(size_t depth,bool dense);
  ~Tree();

  bool Empty()const;
  void Clear();

  void Insert(Index &idx);
  bool Test(Index &idx)const;

  std::vector<Level> FlattenPosition(Index &idx)const;

  //Used to convert dense to sparse or to remove gaps in a sprarse tree
  void Compact();

  uint64_t VoxelCount()const;
  uint64_t NodeCount()const;

  size_t MemUsage()const;

  //Iterators Begin and End
};

