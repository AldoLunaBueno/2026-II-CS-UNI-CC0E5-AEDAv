#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "containers/doublelinkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/circulardoublelinkedlist.h"
#include "containers/Heap.h"
#include "Demos.h"
using namespace std;

void AddOne(GeneralNode<TX> &node) {
    node.value() += 1;
}

template <typename T>
void AddX(GeneralNode<T> &node, T x) {
    node.value() += x;
}

void Square(GeneralNode<TX> &node) {
    node.value() *= node.value();
}

template <typename T>
void ModX(GeneralNode<T> &node, T x) {
    node.value() %= x;
}

// Predicado generico para FirstThat/call: true si el valor del nodo es
// mayor que 'threshold'. Generico en Node y T para servir tanto a
// Vector<VectorAscTraits<TX>> (int) como a Vector<VectorAscTraits<string>>.
template <typename Node, typename T>
bool IsGreaterThan(Node &node, T threshold) {
    return node.getValue() > threshold;
}

template <typename Node>
void PrintNode(Node &node, ostream &os) {
    os << node << " ";
}
const TX NThreads = 5;

// Inserta secuencialmente cada pareja (valor, ref) de 'values' en el
// Container, una por una. La insercion concurrente queda aislada en
// DemoRaceCondition(), que es donde se estudia esa problematica.
template <typename Container>
void InsertElements(Container &container,
                     const vector<pair<typename Container::value_type, Ref>> &values) {
    for (const auto &v : values)
        container.push_back(v.first, v.second);
}

template <typename Container>
void TestContainer(Container &container,
                    const vector<pair<typename Container::value_type, Ref>> &values,
                    const string &filename) {
    InsertElements(container, values);

    // Impresion usando write()
    cout << "Container using write(): ";
    container.write(cout);
    cout << endl;

    // Escritura hacia un archivo, en modo append para acumular cada estado
    // (el archivo se deja vacio una vez al inicio, ver DemoVector)
    ofstream of(filename, ios::app);
    container.write(of);
    of << endl;
    of.close();

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

// Prueba los recorridos del Container hacia adelante (begin/end) y hacia
// atras (rbegin/rend) solamente, imprimiendo los elementos en cada sentido.
template <typename Container>
void TestTraversal(Container &container) {
    using Node = typename Container::Node;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    // Prueba de call()/FirstThat() con valor de retorno: busca el primer
    // nodo cuyo valor sea mayor que el del primer elemento del container.
    if (!container.empty()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

// Prueba los recorridos hacia únicamente hacia adelante, especialmente para una LinkedList
template <typename Container>
void TestForwardTraversal(Container &container){
    using Node = typename Container::Node;

    cout << "Prueba de función call con std::cout para LinkedList [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Prueba de uso de iteradores para impresión: [";
    for (auto &&node : container)
        cout << node << " ";
    cout << "]" << endl;

    // Prueba de FirstThat(): primer nodo cuyo valor sea mayor que el primero
    if (container.begin() != container.end()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

void DemoVector() {
    cout << "Pruebas del contenedor Vector" << endl;
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    // Cada elemento es una pareja (valor, ref) que se guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec);
}

// Insertamos muchos elementos (generados en un loop, no a mano)
// concurrentemente y comparamos cuantos deberian haber entrado contra
// cuantos entraron realmente. Si push_back no estuviera sincronizado
// (sin el mutex/lock_guard actual) esto perderia inserciones o crashearia;
// con el mutex protegiendo push_back/resize, deberia dar siempre 0 perdidas.
void DemoRaceCondition() {
    const size_t N = 200000;

    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    Vector<VectorAscTraits<TX>> vec;

    // Insercion concurrente: NThreads workers insertando en paralelo sobre
    // el mismo Vector, cada uno con un subconjunto entrelazado (stride)
    size_t n = values.size();
    vector<thread> workers;
    for (size_t t = 0; t < NThreads; ++t) {
        workers.emplace_back([&vec, &values, n, t](){
            for (size_t i = t; i < n; i += NThreads)
                vec.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    TXX expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    TXX actualSum = 0;
    for (size_t i = 0; i < vec.size(); ++i) actualSum += vec[i].getValue();

    cout << "DemoRaceCondition: se esperaban " << N << " elementos, "
         << "el Vector quedo con " << vec.size() << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (vec.size() != N || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back / resize sin sincronizar) ***" << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back/resize "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

template <typename Container>
void TestLinkedList(Container &list, const string &filename) {
    ofstream(filename, ios::trunc).close();

    // Pruebas de push_back
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, filename);
    TestForwardTraversal(list);

    InsertElements(list, {{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});

    // Pruebas de insert
    list.insert(4, 12);
    list.insert(11, 20);
    list.insert(7, 6);
    cout << "Lista luego de hacer 3 inserts: " << list << endl;

    // Prueba de ApplyFunction
    list.ApplyFunction(AddOne);
    cout << "Lista tras usar AddOne a sus elementos: " << list << endl;
    list.ApplyFunction(AddX<TX>, TX(10));
    cout << "Lista tras usar AddX(10) a sus elementos: " << list << endl;

    // Prueba de clear
    list.clear();
    cout << "Lista luego de usar clear: " << list << endl;
    list.push_back(10, 2);
    cout << "Lista luego de un nuevo push_back: " << list << endl;

    // Prueba de lectura y asignacion (operator=)
    Container new_list;
    ifstream in(filename);
    in >> new_list;
    in.close();
    cout << "Lista leida desde archivo: " << new_list << endl;
    TestForwardTraversal(new_list);

    list = new_list;
}

template <typename DoubleContainer>
void TestDoubleLinkedListSpecifics(DoubleContainer &list) {
    TestTraversal(list);

    list.clear();
    list.insert(5, 50);
    list.insert(1, 10);
    list.insert(9, 90);
    list.insert(6, 60);
    cout << "Lista tras inserts en cabeza, cola y medio: " << list << endl;
    TestTraversal(list);
}

void DemoLinkedList()
{
    cout << "\nPruebas para LinkedList" << endl;
    LinkedList<LinkedListAscTraits<TX>> list;
    TestLinkedList(list, "linkedlist.txt");

    cout << "\nPruebas para DoubleLinkedList" << endl;
    DoubleLinkedList<DoubleLinkedListAscTraits<TX>> dlist;
    TestLinkedList(dlist, "doublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(dlist);

    cout << "\nPruebas para CircularLinkedList" << endl;
    CircularLinkedList<LinkedListAscTraits<TX>> clist;
    TestLinkedList(clist, "circularlinkedlist.txt");

    cout << "\nPruebas para CircularDoubleLinkedList" << endl;
    CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>> cdlist;
    TestLinkedList(cdlist, "circulardoublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(cdlist);
}

template <typename Heap>
void InitHeap(Heap &heap) {
    heap.push(50, 5);
    heap.push(20, 2);
    heap.push(80, 8);
    heap.push(10, 1);
    heap.push(30, 3);
    heap.push(70, 7);
    cout << "Tras inserciones: " << heap << endl;
}

template <typename Heap>
void ExtractHeap(Heap &heap) {
    cout << "Tras extracción: ";
    while (!heap.empty()) {
        auto node = heap.pop();
        cout << node.getValue() << " ";
    }
    cout << endl;
}

template <typename Heap>
void TestHeap(Heap& heap) {
    InitHeap(heap);
    ExtractHeap(heap);
    InitHeap(heap);
    heap.ApplyFunction(AddOne);
    cout << "Tras aplicar AddOne: " << heap << endl;
    TX m = 13;
    heap.ApplyFunction(ModX<TX>, TX(m));
    cout << "Tras aplicar mod " << m << ": " << heap << endl;
    ExtractHeap(heap);
    cout << endl;
}

void DemoHeap() {
    cout << "\nPruebas para MinHeap" << endl;
    MinHeap minHeap;
    TestHeap(minHeap);

    cout << "\nPruebas para MaxHeap" << endl;
    MaxHeap maxHeap;
    TestHeap(maxHeap);
}