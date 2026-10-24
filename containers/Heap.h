#ifndef __HEAP_H__
#define __HEAP_H__
#include "GeneralContainer.h"
#include "vector.h"

template <typename T>
struct HeapAscTraits : public AscendingTraits<T> {
    using value_type        = T;
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;
};

template <typename T>
struct HeapDescTraits : public DescendingTraits<T> {
    using value_type        = T;
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;
};

template <typename Traits>
class Heap : public GeneralContainer<Traits, Callable, Applicable> {
public:
    using Parent            = GeneralContainer<Traits, Callable, Applicable>;
    using value_type        = typename Parent::value_type;
    using Node              = typename Parent::Node;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using NodePtr           = Node *;
    using Compare           = typename Traits::Compare;
protected:
    Heap() = default;
    using Parent::Parent;
    Vector<VectorAscTraits<value_type>> m_data;
public:
    void push_back(const value_type& value, Ref ref) override { push(value, ref); }

    void clear() override { return m_data.clear(); }

    size_t size() { return m_data.size(); }

    void push(const value_type &value, Ref ref) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (Compare()(m_data[index].getValue(), m_data[parentIndex].getValue())) {
                std::swap(m_data[index], m_data[parentIndex]);
                index = parentIndex;
            } else {
                break;
            }
        }
    }

    Node pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        static Node nullNode;
        if (m_data.empty()) return nullNode;
        Node root = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();

        if (!m_data.empty()) HeapifyDown(0);
        return root;
    }

    void HeapifyDown(size_t index) {
        while (true) {
            size_t l = (index * 2) + 1;
            size_t r = l + 1;
            size_t best = index;
            if (l < size() && Compare()(m_data[l].getValue(), m_data[best].getValue())) best = l;
            if (r < size() && Compare()(m_data[r].getValue(), m_data[best].getValue())) best = r;
            if (best == index) break;
            std::swap(m_data[best], m_data[index]);
            index = best;
        }
    }

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        Parent::ApplyFunction(func, std::forward<Args>(args)...);
        for (size_t i=1; i<size(); ++i) HeapifyUp(i);
    }

    
    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }
};

class MinHeap : public Heap<HeapAscTraits<TX>> {
public:
    using Parent = Heap<HeapAscTraits<TX>>;
    using Parent::Parent;
    MinHeap() = default;
};
class MaxHeap : public Heap<HeapDescTraits<TX>> {
public:
    using Parent = Heap<HeapDescTraits<TX>>;
    using Parent::Parent;
    MaxHeap() = default;
};

#endif // __HEAP_H__