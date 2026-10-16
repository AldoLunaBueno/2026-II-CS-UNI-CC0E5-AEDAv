#ifndef __HEAP_H__
#define __HEAP_H__

template <typename Traits>
class Heap {
    
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using Compare           = typename Traits::Compare;
private:
    Vector<Node> m_data;
    decltype(auto) begin() { return m_data.begin(); }
    decltype(auto) end()   { return m_data.end();   }

public:
    friend ostream &operator <<(ostream &os, const Heap<Traits> &heap) {
        os << "[";
        for (auto it = heap.begin(); it != heap.end(); ++it) {
            os << *it;
            if (std::next(it) != heap.end()) {
                os << ", ";
            }
        }
        return os << "]";
    }
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