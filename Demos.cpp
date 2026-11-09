#include <iostream>
#include <ostream>
#include <fstream> // ofstream para escribir en archivo
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "containers/doublelinkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/circulardoublelinkedlist.h"
#include "containers/Heap.h"
#include "containers/binarytree.h"
#include "containers/avltree.h"
#include "containers/hashtable.h"
#include "containers/merkletree.h"
#include "utilities/printcolored.h"
#include "utilities/render.h"
#include "utilities/layouts.h"
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

template <typename Node, typename T>
bool IsEqualTo(Node &node, T target) {
    return node.getValue() == target;
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

    // Impresion usando operador de inserción
    cout << "Container using operator<<: ";
    cout << container;
    cout << endl;

    // Escritura hacia un archivo, en modo append para acumular cada estado
    // (el archivo se deja vacio una vez al inicio, ver DemoVector)
    ofstream of(filename, ios::app);
    of << container;
    of << endl;
    of.close();

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

// Prueba los recorridos del Container hacia adelante (begin/end) y hacia
// atras (rbegin/rend) solamente, imprimiendo los elementos en cada sentido.
template <typename Container>
void TestTraversal(Container &container, const typename Container::value_type &threshold) {
    using Node      = typename Container::Node;
    using ValueType = typename Container::value_type;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Vista invertida (operator<<): " << container.reversed() << endl;

    Node &found = container.FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;

    Node &rev_found = container.reversed().FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "reversed().FirstThat (ultimo valor > " << threshold << "): " << rev_found << endl;
}

// Prueba los recorridos hacia únicamente hacia adelante, especialmente para una LinkedList
template <typename Container>
void TestForwardTraversal(Container &container, const typename Container::value_type &threshold) {
    using Node      = typename Container::Node;
    using ValueType = typename Container::value_type;

    cout << "Probar call con std::cout";
    container.call(PrintNode<Node>, cout);
    cout << endl;

    cout << "Probar iterador crudo para impresión:";
    for (auto &&node : container)
        cout << node << " ";
    cout << endl;

    Node &found = container.FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
}

void DemoVector() {
    cout << styled(" █ Pruebas del contenedor Vector ").black().bg_bright_yellow().bold() << "\n\n";
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    // Cada elemento es una pareja (valor, ref) que e guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec, 5);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec, "Hello");
}


// Callable
template <typename V, typename PrinterType>
concept IsCallableWith = requires(const V& v, PrinterType p) { v.call(p, std::cout); };
// Searchable
template <typename V, typename PredType, typename KeyType>
concept IsSearchableWith = requires(const V& v, PredType pred, const KeyType& key) { v.FirstThat(pred, key); };
// Applicable
template <typename V, typename MapperFunc>
concept IsApplicableWith = requires(V& v, MapperFunc f) { v.ApplyFunction(f); };
// Printable
template <typename V>
concept IsPrintable = requires(std::ostream& os, const V& v) { os << v; };

void TestCustomVector() {
    BaseVector<VectorAscTraits<TX>, Callable, Applicable, Searchable, Reversible> 
        vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    auto rev = vec.reversed();
    using RevView = decltype(rev);
    using Node    = typename RevView::Node;

    cout << "Si a continuación no hay mensajes de error, "
         << "el contenedor ajustó sus mixins a lo pedido por el cliente." << endl;

    // Validamos Callable
    static_assert(
        IsCallableWith<RevView, decltype(&PrintNode<Node>)>,
        "Test Fallido: RevView debería ser Callable."
    );

    // Validamos Searchable
    static_assert(
        IsSearchableWith<RevView, decltype(&IsGreaterThan<Node, TX>), TX>,
        "Test Fallido: RevView debería ser Searchable con claves TX."
    );

    // Verificamos que Applicable y Printable fallan exitosamente
    static_assert(
        !IsApplicableWith<RevView, decltype(&AddOne)>,
        "Test Fallido: RevView NO debería permitir ApplyFunction."
    );
    static_assert(
        !IsPrintable<RevView>,
        "Test Fallido: RevView NO debería conservar el comportamiento Printable."
    );
}


void DemoCustomContainer() {
    TestCustomVector();
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
    TestForwardTraversal(list, 5);

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
    TestForwardTraversal(new_list, 5);

    list = new_list;
}

template <typename DoubleContainer>
void TestDoubleLinkedListSpecifics(DoubleContainer &list) {
    TestTraversal(list, 5);

    list.clear();
    list.insert(5, 50);
    list.insert(1, 10);
    list.insert(9, 90);
    list.insert(6, 60);
    cout << "Lista tras inserts en cabeza, cola y medio: " << list << endl;
    TestTraversal(list, 5);
}

void DemoLinkedList()
{
    cout << "\n";
    cout << styled(" █ Pruebas para LinkedList ").black().bg_bright_yellow().bold() << "\n\n";
    LinkedList<LinkedListAscTraits<TX>> list;
    TestLinkedList(list, "linkedlist.txt");

    cout << "\n";
    cout << styled(" █ Pruebas para DoubleLinkedList ").black().bg_bright_yellow().bold() << "\n\n";
    DoubleLinkedList<DoubleLinkedListAscTraits<TX>> dlist;
    TestLinkedList(dlist, "doublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(dlist);

    cout << "\n";
    cout << styled(" █ Pruebas para CircularLinkedList ").black().bg_bright_yellow().bold() << "\n\n";
    CircularLinkedList<LinkedListAscTraits<TX>> clist;
    TestLinkedList(clist, "circularlinkedlist.txt");

    cout << styled(" ◉ Diagrama de Lista Enlazada Circular (Personalizado con Lambda y Color): ").black().bg_bright_cyan().bold() << "\n";
    istringstream ss("[(51,15),(1,11),(8,18),(3,13),(45,16),(71,1),(28,8),(32,3),(15,5),(51,15),(1,11),(8,18),(15,5),(10,21),(81,28),(13,23)]");
    ss >> clist;
    clist.draw(
        CircularListLayout{[](const auto& node) {
            auto val = node.getValue();
            if (val > 50) {
                return styled(val).bright_red().bold().str();
            } else if (val % 2 == 0) {
                return styled(val).bright_green().bold().str();
            } else {
                return styled(val).bright_yellow().str();
            }
        }},
        ConsoleGraphRenderer(cout)
    );
    cout << endl;

    cout << "\n";
    cout << styled(" █ Pruebas para CircularDoubleLinkedList ").black().bg_bright_yellow().bold() << "\n\n";
    CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>> cdlist;
    TestLinkedList(cdlist, "circulardoublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(cdlist);

    cout << styled(" ◉ Diagrama de Lista Circular Doblemente Enlazada (Truecolor RGB / Cyan): ").black().bg_bright_cyan().bold() << "\n";
    cdlist.draw(
        CircularListLayout{[](const auto& node) {
            auto val = node.getValue();
            return styled(val).rgb(0, 220, 255).bold().str();
        }},
        ConsoleGraphRenderer(cout)
    );
    cout << endl;
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
    cout << "\n";
    cout << styled(" █ Pruebas para MinHeap ").black().bg_bright_yellow().bold() << "\n\n";
    MinHeap minHeap;
    TestHeap(minHeap);

    cout << "\n";
    cout << styled(" █ Pruebas para MaxHeap ").black().bg_bright_yellow().bold() << "\n\n";
    MaxHeap maxHeap;
    TestHeap(maxHeap);
}

// Función que calcula un color pseudo-aleatorio consistente basado en operaciones modulares sobre el valor del nodo
template <typename T>
std::string get_modular_node_color(const T& val) {
    static const Color palette[] = {
        Color::BrightRed,
        Color::BrightGreen,
        Color::BrightYellow,
        Color::BrightBlue,
        Color::BrightMagenta,
        Color::BrightCyan,
        Color::Cyan,
        Color::Green
    };
    constexpr size_t N = sizeof(palette) / sizeof(palette[0]);
    size_t idx = static_cast<size_t>(((val * 5 + 3) % N + N) % N);
    return styled(val).add(palette[idx]).bold().str();
}

// contenedor instanciado como tipo concreto
template < typename TreeContainer >
void TestBinaryTreeGeneral(TreeContainer &tree, const string &filename) {
    using ValueType = typename TreeContainer::value_type;

    ofstream(filename, ios::trunc).close();

    // Pruebas de insercion y persistencia
    TestContainer(tree, 
        {
            {50, 5},  {20, 2},  {80, 8},  {10, 1},   {30, 3},  {70, 7},  {90, 9},
            {55, 10}, {25, 12}, {85, 18}, {100, 14}, {35, 13}, {75, 17}, {95, 19},
            {43, 20}, {33, 22}, {73, 28}, {93, 24},  {83, 23}, {71, 27}, {63, 29},
            {96, 30}
        }, 
        filename);

    cout << styled(" ◉ Diagrama de árbol (Disposición Vertical con Formato Condicional): ").black().bg_bright_cyan().bold() << "\n";
    std::cout << "(Altura: " << tree.height() << ")" << std::endl;
    tree.draw(
        VerticalTreeLayout{[](const auto& node) {
            auto val = node.getValue();
            if (val == 50) return styled(val).bright_magenta().bold().underline().str();
            if (val > 50)  return styled(val).bright_yellow().bold().str();
            return styled(val).bright_cyan().str();
        }},
        ConsoleGraphRenderer(cout)
    );
    std::cout << std::endl;

    cout << styled(" ◉ Diagrama de árbol (Disposición Horizontal - Dirigida): ").black().bg_bright_cyan().bold() << "\n";
    tree.draw(HorizontalTreeLayout{true}, ConsoleGraphRenderer(cout));
    std::cout << std::endl;

    cout << styled(" ◉ Diagrama de árbol (Disposición Horizontal - No dirigida): ").black().bg_bright_cyan().bold() << "\n";
    tree.draw(HorizontalTreeLayout{false}, ConsoleGraphRenderer(cout));
    std::cout << std::endl;

    // in-order
    TestTraversal(tree, 50);

    // Pruebas de eliminacion (hoja, 1 hijo, 2 hijos / raiz)
    using Node = typename TreeContainer::Node;
    vector<ValueType> to_remove = {10, 55, 20, 80, 50};
    for (const auto &val : to_remove) {
        Node &target_node = tree.FirstThat(IsEqualTo<Node, ValueType>, val);
        tree.remove(&target_node);
        string s = "Arbol tras remove(" + std::to_string(val) + "): ";
        cout << styled(" ◉ " + s).black().bg_bright_red().bold() << "\n";
        std::cout << tree << endl;
        cout << styled(" ◉ Diagrama de árbol tras eliminaciones (Colores Modulares): ").black().bg_bright_red().bold() << "\n";
        std::cout << "(Altura: " << tree.height() << ")" << std::endl;
        tree.draw(
            VerticalTreeLayout{[](const auto& node) {
                return get_modular_node_color(node.getValue());
            }},
            ConsoleGraphRenderer(cout)
        );
        std::cout << std::endl;
    }


    tree.ApplyFunction(AddOne);
    cout << "Arbol tras usar AddOne: " << tree << endl;
    tree.ApplyFunction(AddX< ValueType >, ValueType(10));
    cout << "Arbol tras usar AddX(10): " << tree << endl;

    tree.clear();
    cout << "Arbol luego de usar clear: " << tree << endl;

    // lectura
    TreeContainer new_tree;
    ifstream in(filename);
    in >> new_tree;
    in.close();
    cout << "Arbol leido desde archivo: " << new_tree << endl;

    tree = new_tree;
    cout << "Arbol tras operator=: " << tree << endl;
}

template < Order Order, typename Tree >
void TestOneOrderTour(const Tree &tree) {
    cout << "- Ascendente:  " << tree.template traverse<Order, Direction::Asc>()  << endl;
    cout << "- Descendente: " << tree.template traverse<Order, Direction::Desc>() << endl;
}

template < template < typename > class BinaryTree >
void TestAllTours() {
    PrintColored(cout, "Probar los 14 recorridos posibles:", Color::Green);
    
    // Instanciamos un unico arbol y consultamos sus 14 vistas en O(1)
    using DefaultTree = BinaryTree<BinaryTreeAscTraits<TX>>;
    using Node        = typename DefaultTree::Node;
    DefaultTree tree;

    vector< pair< typename DefaultTree::value_type, Ref > > values = {
        {50, 5}, {20, 2}, {80, 8}, {10, 1}, {30, 3}, {70, 7}, {90, 9}
    };

    InsertElements(tree, values);
    cout << "(Altura: " << tree.height() << ")" << std::endl;
    tree.draw(VerticalTreeLayout{}, ConsoleGraphRenderer(cout));
    cout << endl;

    cout << "Recorrido in-order:" << endl;
    TestOneOrderTour<Order::InOrder>(tree);
    
    cout << "Recorrido pre-order:" << endl;
    TestOneOrderTour< Order::PreOrder>(tree);

    cout << "Recorrido post-order:" << endl;
    TestOneOrderTour<Order::PostOrder>(tree);

    cout << "Recorrido pre-in-order:" << endl;
    TestOneOrderTour<Order::PreInOrder>(tree);
    
    cout << "Recorrido pre-post-order:" << endl;
    TestOneOrderTour<Order::PrePostOrder>(tree);

    cout << "Recorrido in-post-order:" << endl;
    TestOneOrderTour<Order::InPostOrder>(tree);

    cout << "Recorrido pre-in-post-order:" << endl;
    TestOneOrderTour<Order::PreInPostOrder>(tree);

    cout << "\nPruebas adicionales de Mixins (call y FirstThat) sobre vistas .traverse<>():" << endl;
    cout << "- PreOrder con .call(PrintNode): [";
    tree.template traverse< Order::PreOrder >().call(PrintNode< Node >, cout);
    cout << "]" << endl;

    Node &post_first = tree.template traverse< Order::PostOrder >().FirstThat(IsGreaterThan< Node, TX >, TX(25));
    cout << "- PostOrder .FirstThat(> 25): " << post_first << endl;

    Node &euler_desc_first = tree.template traverse< Order::PreInPostOrder, Direction::Desc >().FirstThat(IsEqualTo< Node, TX >, TX(70));
    cout << "- PreInPostOrder (Desc) .FirstThat(== 70): " << euler_desc_first << endl;
}

template < template < typename > class TreeContainer >
void TestBinaryTree(const string &filename = "tree_test.txt") {
    // instanciación con traits por defecto
    TreeContainer< BinaryTreeAscTraits< TX > > tree;
    
    TestBinaryTreeGeneral(tree, filename);
    
    // prueba de recorridos parametrizando la plantilla
    TestAllTours< TreeContainer >();
}

void DemoBinaryTree() {
    cout << styled(" █ Pruebas para BinaryTree ").black().bg_bright_yellow().bold() << "\n\n";
    TestBinaryTree<BinaryTree>("binarytree_test.txt");
    cout << endl;
    
    cout << styled(" █ Pruebas para AVLTree ").black().bg_bright_yellow().bold() << "\n\n";
    TestBinaryTree<AVLTree>("avltree_test.txt");
}

void DemoHashTable() {
    cout << styled(" █ Pruebas para HashTable ").black().bg_bright_yellow().bold() << "\n\n";

    HashTable<HashTableTraits<std::string, TX>> htable;
    htable["a"] = 42;
    htable["b"] = 7;
    htable["c"] = 18;
    htable["la hermenéutica telúrica incáica"] = 9;

    cout << "HashTable usando operator<< (Printable):" << endl;
    cout << htable << endl << endl;

    cout << "Acceso con operator[]:" << endl;
    for (const auto& pair : htable) {
        auto key = pair.getValue().getKey();
        cout << "- Clave " << quoted(key) << ": " << htable[key] << endl;
    }
    cout << "Consultar si existe:" << endl;
    cout << "- contains('b'): " << (htable.contains("b") ? "true" : "false") << endl;
    cout << "- contains('z'): " << (htable.contains("z") ? "true" : "false") << endl << endl;

    // Prueba de persistencia (escritura en archivo y lectura con Readable)
    const string filename = "hashtable_test.txt";
    ofstream of(filename, ios::trunc);
    of << htable << endl;
    of.close();

    HashTable<HashTableTraits<std::string, TX>> htable_restored;
    ifstream in(filename);
    in >> htable_restored;
    in.close();

    PrintColored(cout, "HashTable leido desde archivo (Readable):", Color::Green);
    cout << htable_restored << endl;
    cout << "- Clave 'la hermenéutica telúrica incáica' restaurada: " 
         << htable_restored["la hermenéutica telúrica incáica"] << endl;
}

void DemoMerkleTree() {
    cout << "\n";
    cout << styled(" █ Pruebas para MerkleTree ").black().bg_bright_yellow().bold() << "\n\n";

    MerkleTree<> mtree;
    mtree.push_back("Tx0: Alice -> Bob 50 BTC", 0);
    mtree.push_back("Tx1: Bob -> Charlie 20 BTC", 1);
    mtree.push_back("Tx2: Charlie -> Dave 10 BTC", 2);
    mtree.push_back("Tx3: Dave -> Eve 5 BTC", 3);

    cout << "Cantidad de hojas: " << mtree.size() << endl;
    cout << "Altura del arbol:  " << mtree.height() << endl;
    cout << "Merkle Root Hash:  " << mtree.getRootHash() << endl << endl;

    cout << "Arbol Merkle usando operator<< (Printable - Recorrido In-Order):" << endl;
    cout << mtree << endl << endl;

    // Recorrido Pre-Order mediante Euler Tour
    cout << "Recorrido Pre-Order sobre MerkleTree (.traverse<Order::PreOrder>()):" << endl;
    cout << "[";
    mtree.traverse<Order::PreOrder>().call([](const auto& node, ostream& os) {
        os << "(" << node.getValue() << ", ref=" << node.getRef() << ") ";
    }, cout);
    cout << "]" << endl << endl;

    // Generar y verificar Merkle Proof para la transacción 2
    size_t target_idx = 2;
    string target_tx = mtree.getData(target_idx);
    auto proof = mtree.getProof(target_idx);

    PrintColored(cout, "Generacion de Merkle Proof para hoja #" + to_string(target_idx) + ":", Color::Cyan);
    cout << "Dato original: " << quoted(target_tx) << endl;
    cout << "Hash de hoja:  " << mtree.getLeafHash(target_idx) << endl;
    cout << "Pasos en el camino de auditoria (audit path):" << endl;
    for (size_t i = 0; i < proof.size(); ++i) {
        cout << "  Nivel " << i + 1 << ": " << proof[i] << endl;
    }
    cout << endl;

    // Verificación con datos auténticos
    bool valid = MerkleTree<>::verify(target_tx, proof, mtree.getRootHash());
    cout << "Verificacion con dato autentico: ";
    if (valid) {
        PrintColored(cout, "VALIDA (Integridad confirmada)", Color::Green);
    } else {
        PrintColored(cout, "INVALIDA", Color::Red);
    }
    cout << endl;

    // Verificación con datos manipulados / alterados
    string forged_tx = "Tx2: Charlie -> Dave 9999 BTC";
    bool forged_valid = MerkleTree<>::verify(forged_tx, proof, mtree.getRootHash());
    cout << "Verificacion con dato falsificado (" << quoted(forged_tx) << "): ";
    if (!forged_valid) {
        PrintColored(cout, "RECHAZADA (Ataque detectado con exito)", Color::Green);
    } else {
        PrintColored(cout, "FALLO DE SEGURIDAD", Color::Red);
    }
    cout << endl;

    // Actualización de una hoja y verificación de cambio en la raíz
    string old_root = mtree.getRootHash();
    cout << "Actualizando hoja #" << target_idx << "..." << endl;
    mtree.update(target_idx, "Tx2: Charlie -> Dave 15 BTC");
    string new_root = mtree.getRootHash();
    cout << "Raiz anterior: " << old_root << endl;
    cout << "Nueva raiz:    " << new_root << endl;
    cout << "¿Raices distintas? " << (old_root != new_root ? "Si (Propagacion correcta)" : "No") << endl;

    // La prueba anterior ya no debe ser válida para la nueva raíz
    bool old_proof_on_new_root = MerkleTree<>::verify(target_tx, proof, new_root);
    cout << "¿Prueba vieja valida en nueva raiz? " 
         << (old_proof_on_new_root ? "Si" : "No (Correcto, estado invalidado)") << endl << endl;

    // Prueba de número impar de transacciones (duplicación balanceada)
    PrintColored(cout, "Prueba con cantidad impar de transacciones (5 hojas):", Color::Cyan);
    mtree.push_back("Tx4: Eve -> Frank 1 BTC", 4);
    cout << "Cantidad de hojas: " << mtree.size() << endl;
    cout << "Altura del arbol:  " << mtree.height() << endl;
    cout << "Nueva Merkle Root: " << mtree.getRootHash() << endl;

    auto proof_odd = mtree.getProof(4);
    bool valid_odd = MerkleTree<>::verify(mtree.getData(4), proof_odd, mtree.getRootHash());
    cout << "Verificacion de hoja impar (#4): " 
         << (valid_odd ? "VALIDA (OK)" : "INVALIDA") << endl;
}