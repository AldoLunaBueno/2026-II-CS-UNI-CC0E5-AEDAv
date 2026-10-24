#ifndef __HEAP_H__
#define __HEAP_H__

#include "GeneralContainer.h";

template <typename Traits>
class Heap : public GeneralContainer<Traits, Callable, Applicable> {
    
public:
    using Parent              = GeneralContainer<Traits>;
    using value_type        = typename Parent::value_type;
    using Node              = typename Parent::Node;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using NodePtr           = Node *;
    using Compare           = typename Traits::Compare;
private:
    Vector<Node> m_data;
    decltype(auto) begin() const override { return m_data.begin(); }
    decltype(auto) end()   const override { return m_data.end();   }

public:
    void push(const value_type &value, Ref ref) {
        m_data.push_back(Node(value));
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
        if (m_data.empty()) return;
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        HeapifyDown(0);
    }

};

template <typename Traits>
class MinHeap : public Heap<Traits> {

};

template <typename Traits>
class MaxHeap : public Heap<Traits> {
    
};



#endif // __HEAP_H__