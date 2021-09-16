/*
ByteQueue is a high-performance data structure for managing queues of bytes
in a small, fixed amount of memory (2048 bytes). It supports the functions:
create_queue, enqueue_byte, dequeue_byte, and destroy_queue in O(1) time.
~
by Nicolas Ayllon
*/

#include <iostream>
// Classes
class ByteQueueFragment;
class FragmentPool;
// Errors
void on_out_of_memory() {
  printf("[!] out of memory, no queue created\n");
}
void on_illegal_operation() {
  printf("[!] queue empty, no byte dequeued\n");
}

/***************************/
/* D E C L A R A T I O N S */ 
/***************************/
/*
                             Pool
          ┌┄┄┄┄┄┄┄┄┄┄┄┄┄ 64 fragments ┄┄┄┄┄┄┄┄┄┄┄┄┐
          ┌────────┬────────┬────────┬───┬────────┐
          │fragment│fragment│fragment│...│fragment│ = 2048 bytes
          └────────┴────────┴────────┴───┴────────┘
               ↑        ↑        ↑            ↑ 
              32       32       32           32

FragmentPool holds a 2048-byte data array (unsigned char data[2048]).
It allocates and deallocates 32-byte chunks for ByteQueueFragments.
64 fragments fit into the pool, enough for the assumed max of 64 queues.

FragmentPool also stores a pointer to the head the free list of unallocated
fragments, allowing for fast O(1) allocation.
*/
//
class FragmentPool {

  public:
    // construction and allocation
    FragmentPool();
    ByteQueueFragment* allocate();
    void deallocate(void* ptr);
    // memory calculations
    char getIndexInPool(void* ptr);
    ByteQueueFragment* getPointerAtIndex(char idx);
    // erase
    void eraseFragment(void* ptr);
    void erasePool();

  private:
    unsigned char data[2048];
    // bool used[64]; // testing only
    ByteQueueFragment* nextFreeFragment;
  // testing
  friend void printDataBlock();
};


/*
A queue is made from a linked list of ByteQueueFragments.

          ┌┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ Queue ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┐
          ┌────────┐  ┌────────┐       ┌────────┐
          │fragment│->│fragment│->...->│fragment│
          └────────┘  └────────┘       └────────┘  
               ↑                            ↑ 
             front                        back

When in use, the 32-byte fragment uses 28 bytes for bytes in the queue 
and 4 bytes for tracking.

          ┌┄┄┄┄┄┄┄┄┄┄┄┄┄┄ Fragment ┄┄┄┄┄┄┄┄┄┄┄┄┄┐
          ┌─┬─┬─┬─┬─────────────────────────────┐
          │B│N│f│b│         queue bytes         │ = 32 bytes
          └─┴─┴─┴─┴─────────────────────────────┘ 
           ↑ ↑ ↑ ↑               ↑
           1 1 1 1              28

    The 4 tracking bytes store indices in the pool and fragment's byte array:

        B  index of the back fragment in the pool (0-63) 
        N  index of the next fragment in the pool (0-63) 
        f  index of the front byte in the fragment's byte array (0-27)
        b  index of the back byte in the fragment's byte array (0-27)

    Note: -1 refers to no index. For example, the last fragment 
    in the queue has no next fragment, so N = -1.

When not in use, the same memory acts as a free list, storing a pointer to 
the next unallocated fragment.
*/
//
class ByteQueueFragment {

  friend class FragmentPool; 
  private:
    // A static instance of FragmentPool handles memory allocation
    // and deallocation for ByteQueueFragments.
    static FragmentPool pool;
    // ByteQueueFragment's constructor is private,
    // FragmentPool handles creation
    ByteQueueFragment() {};
    // Use union to use the same 32-bytes to store queue data when used, 
    // or as part of a free list when unused.
    union {
      // filled when used
      struct {
        char m_backFragmentIdx;   // 1 byte, range 0-63
        char m_nextFragmentIdx;   // 1 byte, range 0-63
        char m_frontItemIdx;      // 1 byte, range 0-27
        char m_backItemIdx;       // 1 byte, range 0-27
        unsigned char bytes[28];  // 28 bytes
      } whenUsed;
      // when not used, points to next available memory
      ByteQueueFragment* next;
    } chunk;
    // Get
    char getBackFragmentIdx();
    char getNextFragmentIdx();
    char getFrontItemIdx();
    char getBackItemIdx();
    unsigned char getByte(char idx);
    unsigned char getFrontByte();
    ByteQueueFragment* getBackFragment();
    ByteQueueFragment* getNextFragment();
    // Set
    void setBackFragmentIdx(char backFragmentIdx);
    void setNextFragmentIdx(char nextFragmentIdx);
    void setFrontItemIdx(char frontItemIdx);
    void setBackItemIdx(char backItemIdx);
    void incrementFrontItemIdx();
    void incrementBackItemIdx();
    void clearBytes();
    void setByte(char idx, char byte);
    // Testing & state
    bool isEmpty();
    bool isFrontItemAtEnd();
    bool isBackItemAtEnd();
    bool isValidByteIndex(char idx);
    bool isValidFragmentIndex(char idx);
    // Sets an unused fragment's memory pointing to next in free list
    void setNextFree(ByteQueueFragment* nextFragment);

    // Operations
    friend ByteQueueFragment* create_queue();
    friend void enqueue_byte(ByteQueueFragment*& front, unsigned char byte);
    friend unsigned char dequeue_byte(ByteQueueFragment*& front);
    friend void destroy_queue(ByteQueueFragment*& front);
    // Testing
    friend void printDataBlock();
};
// static member initialized out of class
FragmentPool ByteQueueFragment::pool = FragmentPool();




/*************************/
/* D E F I N I T I O N S */ 
/*************************/

/* * * * * * * * Fragment Pool * * * * * * * */

FragmentPool::FragmentPool() {
  erasePool();
  // memset(&used, false, 64); // (testing only) initialize used array
  // Set each 32-byte unused memory chunks pointing to next in free list
  size_t numFragments = sizeof(data) / sizeof(ByteQueueFragment);
  ByteQueueFragment* start = reinterpret_cast<ByteQueueFragment*>(&data);
  ByteQueueFragment* fragment = start;
  for(int i = 1; i < numFragments; ++i) {
    fragment[i-1].setNextFree(&fragment[i]);
  }
  fragment[numFragments-1].setNextFree(nullptr);
  nextFreeFragment = start;
}

ByteQueueFragment* FragmentPool::allocate() {
  if(nextFreeFragment == nullptr) {
    on_out_of_memory();
    return nullptr;
  }
  ByteQueueFragment* freeFragment = nextFreeFragment;
  nextFreeFragment = nextFreeFragment->chunk.next;
  // used[getIndexInPool(freeFragment)] = true; // testing only
  return freeFragment;
}

void FragmentPool::deallocate(void* ptr) {
  eraseFragment(ptr);
  reinterpret_cast<ByteQueueFragment*>(ptr)->setNextFree(nextFreeFragment);
  nextFreeFragment = reinterpret_cast<ByteQueueFragment*>(ptr);
  // used[getIndexInPool(ptr)] = false; // testing only
}

char FragmentPool::getIndexInPool(void* ptr) {
  return 
    reinterpret_cast<ByteQueueFragment*>(ptr) 
    - reinterpret_cast<ByteQueueFragment*>(&data);
}

ByteQueueFragment* FragmentPool::getPointerAtIndex(char idx) {
  return 
    reinterpret_cast<ByteQueueFragment*>(&data) 
    + idx;
}

void FragmentPool::eraseFragment(void* ptr) {
  memset(ptr, 0, sizeof(ByteQueueFragment));
}

void FragmentPool::erasePool() {
  memset(&data, 0, sizeof(data));
}

/* * * * * * * * ByteQueueFragment * * * * * * * */

// Get
char ByteQueueFragment::getBackFragmentIdx() { 
  return chunk.whenUsed.m_backFragmentIdx; 
}

char ByteQueueFragment::getNextFragmentIdx() { 
  return chunk.whenUsed.m_nextFragmentIdx; 
}

char ByteQueueFragment::getFrontItemIdx() { 
  return chunk.whenUsed.m_frontItemIdx;    
}

char ByteQueueFragment::getBackItemIdx() { 
  return chunk.whenUsed.m_backItemIdx;     
}

unsigned char ByteQueueFragment::getByte(char idx) {
  return chunk.whenUsed.bytes[idx];
}

unsigned char ByteQueueFragment::getFrontByte() {
  return getByte(getFrontItemIdx());
}

ByteQueueFragment* ByteQueueFragment::getBackFragment() {
  if(getBackFragmentIdx() == -1) return nullptr;
  return ByteQueueFragment::pool.getPointerAtIndex(getBackFragmentIdx());
}

ByteQueueFragment* ByteQueueFragment::getNextFragment() {
  if(getNextFragmentIdx() == -1) return nullptr;
  return ByteQueueFragment::pool.getPointerAtIndex(getNextFragmentIdx());
}

// Set
void ByteQueueFragment::setBackFragmentIdx(char backFragmentIdx) {
  chunk.whenUsed.m_backFragmentIdx = backFragmentIdx; 
}

void ByteQueueFragment::setNextFragmentIdx(char nextFragmentIdx) {
  chunk.whenUsed.m_nextFragmentIdx = nextFragmentIdx; 
}

void ByteQueueFragment::setFrontItemIdx(char frontItemIdx) { 
  chunk.whenUsed.m_frontItemIdx = frontItemIdx;
}

void ByteQueueFragment::setBackItemIdx(char backItemIdx) {
  chunk.whenUsed.m_backItemIdx = backItemIdx;
}

void ByteQueueFragment::incrementFrontItemIdx() {
  chunk.whenUsed.m_frontItemIdx++;
}

void ByteQueueFragment::incrementBackItemIdx() {
  chunk.whenUsed.m_backItemIdx++;
}

void ByteQueueFragment::clearBytes() { 
  memset(chunk.whenUsed.bytes, 0, 28); 
}

void ByteQueueFragment::setByte(char idx, char byte) {
  // if(!isValidByteIndex(idx)) return; // testing
  chunk.whenUsed.bytes[idx] = byte;
}

// Testing & State
bool ByteQueueFragment::isEmpty() {
  return getFrontItemIdx() == -1 || getBackItemIdx() < getFrontItemIdx();
}

bool ByteQueueFragment::isFrontItemAtEnd() {
  return getFrontItemIdx() == sizeof(chunk.whenUsed.bytes) - 1; // == 27
}

bool ByteQueueFragment::isBackItemAtEnd() {
  return getBackItemIdx() == sizeof(chunk.whenUsed.bytes) - 1; // == 27
}

bool ByteQueueFragment::isValidByteIndex(char idx) {
  bool isValidIndex = (0 <= idx && idx < 28);
  if(!isValidIndex) {
    printf("invalid byte index %i not in range [0, 27]\n", idx);
  }
  return isValidIndex;
}

bool ByteQueueFragment::isValidFragmentIndex(char idx) {
  bool isValidIndex = (0 <= idx && idx < 64);
  if(!isValidIndex) {
    printf("invalid fragment index %i not in range [0, 63]\n", idx);
  }
  return isValidIndex;
}


// For using the same memory in the free list
void ByteQueueFragment::setNextFree(ByteQueueFragment* nextFragment) {
  chunk.next = nextFragment;
}



/* * * * * * * * * * Operations * * * * * * * * * */
/* * * * * * * * (Friend Functions) * * * * * * * */

ByteQueueFragment* create_queue() {
  // Allocate memory
  ByteQueueFragment* newFragment = ByteQueueFragment::pool.allocate();
  if(newFragment == nullptr) { 
    return nullptr; 
  }
  // Construct fragment
  char indexInPool = ByteQueueFragment::pool.getIndexInPool(newFragment);
  newFragment->setBackFragmentIdx(indexInPool);
  newFragment->setNextFragmentIdx(-1);
  newFragment->setFrontItemIdx(-1);
  newFragment->setBackItemIdx(-1);
  newFragment->clearBytes();
  return newFragment;
}


// Pass by reference to update front in case it was nullptr and got allocated
void enqueue_byte(ByteQueueFragment*& front, unsigned char byte) {
  // If front points to no queue (it's been deallocated)
  if(front == nullptr) {
    // First try to create it.
    front = create_queue();
    // If there really is no more memory, give up.
    if(front == nullptr) return;
  }

  ByteQueueFragment* currentBack = front->getBackFragment();
  // If back fragment has last byte at end of array, allocate new fragment
  if(currentBack->isBackItemAtEnd()) {
    ByteQueueFragment* newBack = ByteQueueFragment::pool.allocate();
    if(newBack == nullptr) return; // avoid crash for failed allocation
    // Update indices in front and old back to point to new back
    char newBackFragmentIdx = 
      ByteQueueFragment::pool.getIndexInPool(newBack);
    front->setBackFragmentIdx(newBackFragmentIdx);
    currentBack->setNextFragmentIdx(newBackFragmentIdx);
    // Initialize new back fragment
    newBack->setBackFragmentIdx(-1);
    newBack->setNextFragmentIdx(-1);
    newBack->setFrontItemIdx(-1);
    newBack->setBackItemIdx(0); // first item in the new back fragment
    newBack->clearBytes();
    newBack->setByte(0, byte);
    return;
  }
  // Front fragment empty, so set byte at index 0, update frontItem & backItem
  if(front->getFrontItemIdx() == -1) {
    front->setFrontItemIdx(0);
    front->setBackItemIdx(0);
    front->setByte(0, byte);
    return;
  }
  // Current back fragment is not empty
  currentBack->incrementBackItemIdx(); // -1 -> 0 in empty fragment
  currentBack->setByte(currentBack->getBackItemIdx(), byte);
}


// Note:
// dequeue_byte preemptively deallocates memory as soon as the queue is empty,
// which sets front = nullptr.
// enqueue_byte will reallocate memory if bytes are added to the empty queue.
//
// (Pass by reference to update front when last byte in fragment is dequeued.)
unsigned char dequeue_byte(ByteQueueFragment*& front) {
  // Handle nullptr or empty queue
  if(front == nullptr || front->isEmpty()) {
    on_illegal_operation();
    return 0;
  }
  unsigned char dequeuedByte = front->getFrontByte();
  // Dequeued byte was the last in the fragment
  if(front->isFrontItemAtEnd()) {
    // There is no next fragment.
    // Deallocate. A new one will be allocated on next enqueue.
    if(front->getNextFragmentIdx() == -1) {
      ByteQueueFragment::pool.deallocate(front);
      front = nullptr;
    }
    // There is a next fragment.
    // Update the next fragment with front's data, and set it as the new front.
    else { 
      ByteQueueFragment* newFront = front->getNextFragment();
      newFront->setBackFragmentIdx(front->getBackFragmentIdx());
      // No need for setNextFragment() of next fragment. Unaffected by dequeue.
      newFront->setFrontItemIdx(0);
      // No need for setBackItemIdx(), done in enqueue_byte
      ByteQueueFragment::pool.deallocate(front);
      front = newFront;
    }
    return dequeuedByte;
  }

  // Dequeued byte was NOT the last item in fragment, so increment index
  front->incrementFrontItemIdx();
  // If the queue is now empty, deallocate it.
  if(front->isEmpty()) {
    ByteQueueFragment::pool.deallocate(front);
    front = nullptr;
  }
  return dequeuedByte;
}

// Pass by reference to update front to nullptr once queue is destroyed
void destroy_queue(ByteQueueFragment*& front) {
  while(front != nullptr) {
    ByteQueueFragment* fragmentToDeallocate = front;
    front = front->getNextFragment();
    ByteQueueFragment::pool.deallocate(fragmentToDeallocate);
  }
}



/*****************/
/* T E S T I N G */ 
/*****************/

void printDataBlock() {
  // print j values
  std::cout << "       j:";
  for(int j = 0; j < 32; ++j) { printf("%4i", j); }
  std::cout << '\n';
  // print box top
  std::cout << "        ┌─";
  for(int j = 0; j < 32; ++j) { printf("────"); }
  std::cout << '\n';
  // print i labels
  for(int i = 0; i < 64; ++i) {
    // bool used = ByteQueueFragment::pool.used[i];
    // std::string icon = used ? "●" : "○";
    // printf("%s  i:%3i│", &icon, i);
    bool used = true;       // when used array is commented out
    printf("   i:%3i│", i); // when used array is commented out
    // print bytes in row i
    if(used) {
      for(int j = 0; j < 32; ++j) {
        unsigned char val = ByteQueueFragment::pool.data[32*i + j];
        if(j < 4) {
          printf("%4i", (char)val);
          continue;
        }
        printf("%4i", val);
      }
    }
    else {
      for(int j = 0; j < 32; ++j) {
        unsigned char val = ByteQueueFragment::pool.data[32*i + j];
        printf("%4x", val);
      }
    }
    std::cout << '\n';
  }
}

/***********/
/* M A I N */ 
/***********/

int main() {
  //Provided test
  ByteQueueFragment* q0 = create_queue();
  enqueue_byte(q0, 0);
  enqueue_byte(q0, 1);
  ByteQueueFragment* q1 = create_queue();
  enqueue_byte(q1, 3);
  enqueue_byte(q0, 2);
  enqueue_byte(q1, 4);
  printf("%d", dequeue_byte(q0));
  printf("%d\n", dequeue_byte(q0));
  enqueue_byte(q0, 5);
  enqueue_byte(q1, 6);
  printf("%d", dequeue_byte(q0));
  printf("%d\n", dequeue_byte(q0));
  destroy_queue(q0);
  printf("%d", dequeue_byte(q1));
  printf("%d", dequeue_byte(q1));
  printf("%d\n", dequeue_byte(q1));
  destroy_queue(q1);
  //printDataBlock();
  return 0;
}
