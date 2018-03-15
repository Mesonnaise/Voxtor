#pragma once
#include"Index.h"
#include"Vector.h"
class Tree;
class Accessor{
private:
  Index  mIndex;
  Tree  *mTree;
protected:
  Accessor(Tree *tree);
public:
  ~Accessor();

  void UpdateIndex(Index index);
  void UpdateIndex(Vector vec);

  bool Valid()const;
  void Clean();

  size_t Depth()const;
  bool   IsVoxel()const;

};

