#### requirements:
 - project has been compiled using g++12.1.0 and tested on Ubuntu 22.04.2 LTS
 - compilation requires C++23 support
 - CppOrderBook is intended for a x86_64 platform

#### project description:
Limited scope project demonstrating reading an order-message from a socket, constructing a message
object, possibly handing it off to another thread through a seqlock-queue and inserting it into an
order book. 
The project employs compile time checks using both classic template metaprogramming
and C\++20 concepts. Cutting edge C\++ features were incorporated if possible, such as monadic operations
on `std::optional` (hence the project requiring C++23 for compilation) and interfacing with heap allocated memory via `std::span`.
Being the de facto industry standard, the byte-layout of messages in the socket buffer complies
with the FIX-protocol, more specifically little-endian FIX Simple-Binary-Encoding (SBE) and the FIX
Simple-Open-Framing-Header. A precise XML specification of the message types used can be found in the misc folder.  
For simplicity reasons (i.e. requiring only a single header file include), doctest was
chosen as a unit testing framework.
 

Special emphasis was put on techniques enabling low-latency. Those include:
 - avoiding branches (or, more realistically, keeping them short) even at the expense
 of some unnecessary computation in each run or a less favorable theoretical algorithmic complexity
 - avoiding system calls outside the set-up and tear-down phases of the project, primarily by objects
 allocating all their required heap memory during construction and holding on to it until destruction
 and using std::atomic_flag as a lock instead of some flavor of mutex
 - memory-aligning objects in accordance with a 64-byte cacheline
 - ensuring fast multithreaded access to components that support concurrency, namely the seqlock-queue which enables wait-free enqueueing and lock-free dequeueing and the order book which supports lock-free reads via a seqlock as well (a lock is used required for manipulating book entries though) 




#### components:

##### FIXSocketHandler::fixSocketHandler:
 `template <typename msgClassVariant_, typename socketType_>
 struct fixSocketHandler`
###### template parameters:
- msgClassVariant_: std::variant of supported message type classes, subject to compile time
 checks regarding constructors and static members variables
- socketType_: class of socket that messages will be read from, needs to satisfy concept Auxil::readablesAsSocket

###### description:
- constructs FIX message objects from bytes read out of an object of a type providing a socket like interface
- reads FIX messages in little-endian Simple Binary Encoding(SBE) and the Simple Open Framing Header(SOFH)  
- holds pointer to a socket-type object

###### interfaces:
- `std::optional<MsgClassVar> readNextMessage()`:
 returns an optional including the a message object within a variant when a message of a type 
 contained in MSGClassVar is read an validated via checksum
 or an empty std::optional if message with a valid header of a type not included or a message is 
 invalidated via checksum

###### limitations:
- not able to correctly handle an incoming message with valid header and checksum if the message is
 shorter than the shortest message contained in msgClassVar_



##### SeqLockElement::seqLockElement:
`template<typename contentType_, std::uint32_t alignment>
struct seqLockElement`
###### template parameters:
- contentType_: type of objects to be entered into seqlock queue, must satisfy constraints
 std::is_default_constructible_v and std::is_trivially_copyable_v for construction of empty 
 queue and insertion respectively
- alignment: alignment of element in queue, use 0 for default alignment

###### description: 
- encapsulates single entry of seq-lock queue and a version number
- memory heap-allocated during construction, released during destruction

###### interfaces:
- `void insert(contentType_ newContent)`:
 inserts new content into element and
 increments version number by 2, will overwrite previous content
- `std::tuple<std::optional<contentType_>,std::int64_t> read(std::int64_t prevVersion)`:
 returns current version number and and optional with content if argument isnt's larger than current version number or empty optional otherwise. If the version number of the last element read from queue is given as an argument, this ensures that no empty element is read and no element is read twice.
 


##### SeqLockQueue::seqLockQueue:
 `template <typename contentType_, std::uint32_t length_, bool shareCacheline> 
 struct seqLockQueue`

###### template parameters:
- contentType_: type of content queue contains
- length_: number of elements in queue, must be a power of two
- shareCacheline: each element in queue will be aligned to a full cacheline if shareCacheline == false
 
###### description:
- single-producer single-consumer queue utilizing a ring buffer that allows for wait-free enqueueing in lock-free dequeueing
 
###### interfaces:
- `void enqueue(contentType_ content)`:
 inserts content into queue
- `std::optional<contentType> dequeue()`:
 dequeues and returns an element, returns empty optional if queue is empty

###### limitations:
- no checks for queue overflowing, content will be overwritten and lost
- if size of content type is small and/or memory used is not a big issue, one should consider
 sizing the queue big enough to avoid wrapping altogether during its lifetime
- while extending functionality to enable multiple readers would be quite straightforward
 by introducing a class that keeps track of read indices and version numbers for every reader,
 allowing for multiple writers would seriously complicate the underlying logic and likely do away
 with wait free enqueueing
 
 
 
#### FIXmsgClasses:
##### description:
- three classes representint different types of orders/messages for the order book:
   -  adding liquidity
  -  withdrawing liquidity added previously
  - filling a market order
- all classes contain static data specifying the byte layout of incoming messages to be used by the socketHandler class
- all classes can be constructed from a std::span of 8-bit integers(for use by socketHandler), default constructed without arguments or constructed from arguments representing their respective  non-static members (volume and possibly price)



#### AtomicGuards::AtomicFlagGuard:
##### description: 
- roughly analogous to `std::lock_guard` for `std::atomic_flag` instead of mutex
- holds pointer to `std::atomic_flag` given as argument to constructor (can be nullptr)
- has to be locked and can be unlocked by calls to member instead construction and destruction
- supports moving but not copying

##### interfaces:
- `bool lock()`: returns false if pointer to flag is nullptr or flag is already set by object,
 sets flag and returns true otherwise, will block/spin until flag is released by other thread
- `bool unlock()`: clears flag and returns true, returns false if pointer to flag is nullptr or flag
 wasn't set in the first place 
- `void rebind(std::atomic_flag* otherflag_ptr)`: calls `unlock()` and sets pointer to` otherflag_ptr`



#### OrderBookBucket:orderBookBucket:
`template <typename EntryType, std::uint32_t alignment>
struct alignas(alignment) orderBookBucket`

##### template parameters:
- `EntryType`: represents volume in bucket, must be signed integral
- `aligment`: alignment of bucket in order book, use 0 for default alignment

##### description:
- encapsulates volume in for single price point in an order book as well as the logic to change
 and query volume
- negative volume represents supply, positive volume represents demand
 
##### interfaces:
- `EntryType consumeLiquidity(EntryType fillVolume)`: removes liquidity of an amount specified by `fillVolume` and returns the amount of liquidity removed. Always reduces the absolute value of volume contained in bucket. Returns 0 if volume contained does not match the sign of `fillVolume`
- `void addLiquidity(EntryType addVolume)`: simply adds addVolume to volume already contained in bucket
- `EntryType getVolume()`: returns volume currently contained in bucket 



#### OrderBook::orderBook:
`template <typename msgClassVariant_, typename entryType_,
          std::uint32_t bookLength_, bool exclusiveCacheline_,
          size_t newLimitOrderIndex_, size_t withdrawLimitOrderIndex_,
          size_t marketOrderIndex_>
struct orderBook`

##### template parameters:
- `msgClassVariant_`: variant containing supported message classes (same as in socketHandler class template)
- `entryType_`: same as in orderBookBucket
- `bookLength_`: number of price buckets contained in order book
- `exclusiveCacheline_`: if true, each bucket will be aligned to a full cacheline (64 bytes)
- `newLimitOrderIndex_`, `withdrawLimitOrderIndex_`, `marketOrderIndex_`: indices of respective message class in `msgClassVariant_`
 
##### description: 
- contains the bid/ask volume for some asset in a range of price buckets relative to some base price
- provides functionality for querying volume for specific price and adusting volume by processing order objects
- negative volume represents demand, positive volume represents supply
- price is represented by an unsigned integer and must therefore always be positive
- contained price range is determined by base price (argument of constructor) book length
- supports adding volume for specific price, withdrawing/cancelling volume for specific price and
 filling market orders using volume contained in book
- keeps track of best bid, lowest bid, best offer and highest offer at all times
- supports lock-free queries for volume, relies on `std::atomic_flag` as lock when processing orders

##### invariants:
- represent conditions that need to hold to avoid a crossed book and maintain the integrity of stats kept 
- if there is a best bid, there is a lowest bid and vice versa
- if there is a best offer, there is a highest offer and vice versa
- best offer > best bid, highest offer >= best offer, lowest bid <= best bid
- no positive volume below best offer, no negative volume above best bid
- no non-zero volume above highest offer or below lowest bid


##### interfaces:
- `entryType volumeAtPrice(std::uint32_t price)`: returns volume contained in order book at price specified by argument
- `std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>bestBidAsk()`:
 returns tuple of two std::optionals containing the prices of best bid and best offer
 if book contains demand and supply respectively and empty std:optionals otherwise 
- `bool shiftBook(std::int32_t shiftBy)`: changes base price of book by `shiftBy` and shifts all volume within memory. Not possible if shifting would turn base price negative or result in non-zero volume buckets falling out of bounds. Returns true if successful, false otherwise
- `std::tuple<std::uint32_t, entryType_, entryType_, std::uint8_t> processOrder(msgClassVariant_ order)`:
 processes order contained in argument, returns tuple containing:
    -  marginal executions price/ last price at which any volume contained in order was filled
    -  filled volume: volume filled right when order was processed
    -  order revenue: price * volume for any volume contained in order that was filled while processing
    -  0 if order price was within bounds, 1 if it wasn't (other three entries must be 0 in this case)
- `std::uint64_t __invariantsCheck(int nIt)`: intended for debugging purposes,
 checks invariants layed out above aren't violated, prints an error message containing
 argument nIt (most likely an iteration index) for any violation and returns a sum of error codes
 of all violations
