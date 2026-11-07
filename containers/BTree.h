// btree.h

#ifndef BTREE_H
#define BTREE_H

/*template <typename keyType, typename ObjIDType>
class BTree;*/


/************************
#include "btpage.h"
*************************/

#include "CBTreePage.h"

//#include "recfile.h"
//#include "fixfld.h"
//#include "indbuff.h"

#define DEFAULT_BTREE_ORDER 3

//		btree needs to be able to pack, unpack, read and
//      write index records
//      also, handle the insert, delete, split, merge,
//      growth of the tree
//      a pool of nodes will be managed
//      keep at least a branch of the tree in memory
//


template <typename keyType, typename ObjIDType = long>
class BTree 
// this is the full version of the BTree
{
       typedef CBTreePage <keyType, ObjIDType> BTreePage;// useful shorthand
       /*struct ObjectInfo
       {
               keyType first;
               long    second;
               ObjectInfo *&operator->() { return this; }
       };*/

public:
       //typedef ObjectInfo iterator;
       // Eliminarlo y utilizar la tecnica moderna
       typedef BTreePage::lpfnForEach2    lpfnForEach2;
       typedef BTreePage::lpfnForEach3    lpfnForEach3;
       typedef BTreePage::lpfnFirstThat2  lpfnFirstThat2;
       typedef BTreePage::lpfnFirstThat3  lpfnFirstThat3;
       typedef BTreePage::ObjectInfo      ObjectInfo;

public:
       BTree(int order = DEFAULT_BTREE_ORDER, bool unique = true);
       ~BTree();
       //int           Open (char * name, int mode);
       //int           Create (char * name, int mode);
       //int           Close ();
       bool            Insert (const keyType key, const int ObjID);
       bool            Remove (const keyType key, const int ObjID);
       ObjIDType       Search (const keyType key);
       long            size()  { return m_NumKeys; }
       long            height() { return m_Height;      }
       long            GetOrder() { return m_Order;     }

       void            Print (ostream &os);
       void            ForEach( lpfnForEach2 lpfn, void *pExtra1 );
       void            ForEach( lpfnForEach3 lpfn, void *pExtra1, void *pExtra2);
       ObjectInfo*     FirstThat( lpfnFirstThat2 lpfn, void *pExtra1 );
       ObjectInfo*     FirstThat( lpfnFirstThat3 lpfn, void *pExtra1, void *pExtra2);
       //typedef               ObjectInfo iterator;

protected:
       //BTreePage * FindLeaf (const keyType key);
       // load a branch into memory down to the leaf with key

       //BTreePage * Fetch(const int ObjID);
       //int Store (BTreePage *);
       BTreePage          m_Root;
       int             m_Height;  // height of tree
       int             m_Order;   // order of tree
       long            m_NumKeys; // number of keys
       bool            m_Unique;  // Accept the elements only once ?
       //int PoolSize;
       ///BTreePage ** Nodes; // pool of available nodes
       // Nodes[1] is level 1, etc. (see FindLeaf)
       // Nodes[Height-1] is leaf
       //FixedFieldBuffer Buffer;
       //RecordFile<BTreePage> BTreeFile;
};

/****************************
#include "btree.hxx"
#endif
*****************************/

//btree.tc
#include <iostream>
//#include "mybuffer.h"

const int MaxHeight = 5;
template <typename keyType, typename ObjIDType>
BTree<keyType, ObjIDType>::BTree(int order, bool unique)
                               : m_Unique(unique),
                                 m_Order(order),
                                 m_Root(2 * order  + 1, m_Unique),
                                 m_NumKeys(0)
{
       m_Root.SetMaxKeysForChilds(order);
       m_Height = 1;
}

template <typename keyType, typename ObjIDType>
BTree<keyType, ObjIDType>::~BTree()
{
}

template <typename keyType, typename ObjIDType>
bool BTree<keyType, ObjIDType>::Insert(const keyType key, const int ObjID)
{
       bt_ErrorCode error = m_Root.Insert(key, ObjID);
       if( error == bt_duplicate )
               return false;
       m_NumKeys++;
       if( error == bt_overflow )
       {
               m_Root.SplitRoot();
               m_Height++;
       }
       return true;
}

template <typename keyType, typename ObjIDType>
bool BTree<keyType, ObjIDType>::Remove (const keyType key, const int ObjID)
{
       bt_ErrorCode error = m_Root.Remove(key, ObjID);
       if( error == bt_duplicate || error == bt_nofound )
               return false;
       m_NumKeys--;

       if( error == bt_rootmerged )
               m_Height--;
       return true;
}

template <typename keyType, typename ObjIDType>
ObjIDType BTree<keyType, ObjIDType>::Search (const keyType key)
{
       ObjIDType ObjID = -1;
       m_Root.Search(key, ObjID);
       return ObjID;
}


template <typename keyType, typename ObjIDType>
void BTree<keyType, ObjIDType>::ForEach(lpfnForEach2 lpfn, void *pExtra1)
{
       m_Root.ForEach(lpfn, 0, pExtra1);
}

template <typename keyType, typename ObjIDType>
void BTree<keyType, ObjIDType>::ForEach(lpfnForEach3 lpfn, void *pExtra1, void *pExtra2)
{
       m_Root.ForEach(lpfn, 0, pExtra1, pExtra2);
}

template <typename keyType, typename ObjIDType>
BTree<keyType, ObjIDType>::ObjectInfo *
BTree<keyType, ObjIDType>::FirstThat(lpfnFirstThat2 lpfn, void *pExtra1)
{
       return m_Root.FirstThat(lpfn, 0, pExtra1);
}

template <typename keyType, typename ObjIDType>
BTree<keyType, ObjIDType>::ObjectInfo *
BTree<keyType, ObjIDType>::FirstThat(lpfnFirstThat3 lpfn, void *pExtra1, void *pExtra2)
{
       return m_Root.FirstThat(lpfn, 0, pExtra1, pExtra2);
}

template <typename keyType, typename ObjIDType>
void BTree<keyType, ObjIDType>::Print (ostream &os)
{
       m_Root.Print(os);
}






#endif