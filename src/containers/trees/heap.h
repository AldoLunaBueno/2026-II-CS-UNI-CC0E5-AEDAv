/**
 * @file heap.h
 * @ingroup trees
 * @brief Montículo binario genérico (`BaseHeap` / `Heap`, `MinHeap`, `MaxHeap`) con restauración de invariante.
 */

#ifndef __HEAP_H__
#define __HEAP_H__
#include "../core/general_container.h"
#include "../vector.h"

/** @brief Rasgos base de nodo e iterador para `BaseHeap`. */
template <typename T>
struct HeapTraits {
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;
};

/** @brief Rasgos para montículo mínimo (`std::less<T>`). */
template <typename T>
struct HeapAscTraits : public AscendingTraits<T>, public HeapTraits<T> {};

/** @brief Rasgos para montículo máximo (`std::greater<T>`). */
template <typename T>
struct HeapDescTraits : public DescendingTraits<T>, public HeapTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseHeap;

/** @brief Alias estándar de montículo con mixins de consulta, mutación y serialización. */
template <typename Traits>
using Heap = BaseHeap<Traits, Callable, Applicable, Printable, Readable>;

/**
 * @class BaseHeap
 * @brief Montículo binario thread-safe almacenado sobre un `Vector` interno.
 *
 * @tparam Traits Rasgos del montículo (`HeapAscTraits` o `HeapDescTraits`).
 * @tparam Mixins Lista de mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseHeap : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseHeap;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using Compare = typename Traits::Compare;
protected:
    using Parent::Parent;
    using Parent::m_mutex;
    Vector<VectorAscTraits<value_type>> m_data; /**< Arreglo contiguo que almacena los nodos del montículo. */
public:
    BaseHeap() = default;
    BaseHeap(const BaseHeap& other) { *this = other; }
    BaseHeap(BaseHeap&& other) noexcept { *this = std::move(other); }
    virtual ~BaseHeap() { clear(); }

    BaseHeap& operator=(const BaseHeap& other) {
        this->copy_from(other);
        return *this;
    }
    BaseHeap& operator=(BaseHeap&& other) noexcept {
        this->move_from(other, [&]{ m_data = std::move(other.m_data); });
        return *this;
    }

    /** @brief Inserta un elemento delegando a `push()`. */
    void push_back(const value_type& value, Ref ref) override { push(value, ref); }

    /** @brief Vacía el montículo. */
    void clear() override {
        std::lock_guard lock(m_mutex);
        m_data.clear();
    }

    /** @brief Retorna el número de elementos en el montículo. */
    size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_data.size();
    }

    /** @brief Inserta `(value, ref)` y restaura la propiedad de montículo hacia arriba. */
    void push(const value_type &value, Ref ref);
    /** @brief Flota el elemento en `index` hacia la raíz mientras supere a su padre según `Compare`. */
    void HeapifyUp(size_t index);
    /** @brief Extrae y retorna la raíz del montículo reordenando el resto con `HeapifyDown(0)`. */
    Node pop();
    /** @brief Hunde el elemento en `index` hacia las hojas restaurando la invariante de montículo. */
    void HeapifyDown(size_t index);

    /**
     * @brief Restaura la invariante de montículo tras modificaciones externas (por ejemplo vía `ApplyFunction`).
     *
     * @image html heap_rebuild.svg "Ciclo de invariante de reconstrucción en Heap"
     */
    void rebuild() override;

    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }
};

template <typename Traits, template <typename> typename... Mixins>
void BaseHeap<Traits, Mixins...>::push(const value_type &value, Ref ref) {
    std::lock_guard lock(m_mutex);
    m_data.push_back(value, ref);
    HeapifyUp(m_data.size() - 1);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHeap<Traits, Mixins...>::HeapifyUp(size_t index) {
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

template <typename Traits, template <typename> typename... Mixins>
auto BaseHeap<Traits, Mixins...>::pop() -> Node {
    std::lock_guard lock(m_mutex);
    static Node nullNode;
    if (m_data.empty()) return nullNode;
    Node root = m_data[0];
    std::swap(m_data[0], m_data[m_data.size() - 1]);
    m_data.pop_back();

    if (!m_data.empty()) HeapifyDown(0);
    return root;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseHeap<Traits, Mixins...>::HeapifyDown(size_t index) {
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

template <typename Traits, template <typename> typename... Mixins>
void BaseHeap<Traits, Mixins...>::rebuild() {
    std::lock_guard lock(m_mutex);
    for (size_t i=1; i<size(); ++i) HeapifyUp(i);
}

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