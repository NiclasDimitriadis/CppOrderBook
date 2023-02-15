#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>

#include "Auxil.hpp"
#include "AtomicGuards.hpp"
#include "FixMsgTypeChecks.hpp>
#include "OrderBookBucket.hpp"


namespace OrderBook{
	template <typename msgClassVariant_, typename entryType_, std::uint32_t bookLength_, size_t cacheline_,
	bool exclusiveCacheline_, size_t newLimitOrderIndex_, size_t withdrawLimitOrderIndex_, size_t marketOrderIndex_>
	requires std::signed_integral<entryType> 
	&& MsgTypeChecks::OrderBookLimitOrderInterface<msgClassVariant, newLimitOrderIndex_, withdrawLimitOrderIndex_>
	&& MsgTypeChecks::OrderBookMarketOrderInterface<msgClassVariant, marketOrderIndex_>
	struct orderBook{
	private:
		//static members and member types
		static constexpr std::uint32_t alignment = std::max(Auxil::divisible_or_ceil(sizeof(entryType_), cacheline_), cacheline_*exclusiveCacheline);
		typedef OrderBookBucket::orderBookBucket<entryType, alignment> bucketType;
		static constexpr size_t newLimitOrderIndex = newLimitOrderIndex_;
		static constexpr size_t withdrawLimitOrderIndex = withdrawLimitOrderIndex_;
		static constexpr size_t marketOrderIndex = marketOrderIndex_;
		static constexpr std::uint32_t uintDefaultArg =  std::numeric_limits<std::uint32_t>::max();
		typedef std::variant_alternative_t<newLimitOrderIndex, msgClassVariant_> newLimitOrderClass;
		typedef std::variant_alternative_t<withdrawLimitOrderIndex, msgClassVariant_> withdrawLimitOrderClass;
		typedef std::variant_alternative_t<marketOrderIndex, msgClassVariant_> marketOrderClass;
		typedef std::tuple<newLimitOrderClass, withdrawLimitOrderClass, marketOrderClass> orderClassTuple;
		typedef std::tuple<newLimitOrderClass*, withdrawLimitOrderClass*, marketOrderClass*> orderClassPtrTuple;
		typedef std::tuple<std::uint32_t, entryType, entryType, std::uint8_t> orderResponse;
		
		//non static member variables
		std::uint32_t basePrice;
		std::optional<std::uint32_t> bestBid, lowestBid, bestOffer, HighestOffer;
		alignas(cacheline_) std::atomic<bool> modifyLock = false;
		alignas(cacheline_) std::int64_t versionCounter = 0;
		//points to memory location of the actual orderbook entries
		alignas(cacheline_) void* memoryPointer;
		std::span<OrderBookBucket::orderBookBucket<EntryType>> memSpan;
		
		//private member functions
		bool priceInRange(std::uint32_t) noexcept;
		void resetStatsIfExhausted(bool) noexcept;
		std::optional<std::uint32_t> findLiquidBucket(std::uint32_t, bool) noexcept;
		orderResponse runThroughBook(entryType, std::uint32_t = uintDefaultArg) noexcept;
		orderResponse handleNewLimitOrder(std::variant_alternative_t<newLimitOrderIndex, msgClassVariant_>) noexcept;
		orderResponse handleWithdrawLimitOrder(std::variant_alternative_t<withdrawLimitOrderIndex, msgClassVariant_>) noexcept;
		orderResponse handleMarketOrder(std::variant_alternative_t<marketOrderIndex, msgClassVariant_>) noexcept;

	public:
		//static members and member types
		typedef entryType_ entryType;
		typedef msgClassVariant_ msgClassVariant;
		static constexpr std::uint32_t bookLength = bookLength_;
		static constexpr byteLength = bookLength*alignment;
		static constexpr size_t cacheline = cacheline_;
		
		//5 special members 
		explicit orderBook(std::uint32_t) noexcept;
		~orderBook();
		//delete copying and moving, avoid checks for unspecified state
		orderBook(const orderBook&) = delete;
		orderBook& operator=(const orderBook&) = delete;
		orderBook(orderBook&&) = delete;
		orderBook& operator=(orderBook&&) = delete;
		
		//public member functions
		msgResponse processOrder(msgClassVariant) noexcept;
		const std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>> bestBidAsk() noexcept;
		const entryType volumeAtPrice(std::uint32_t) noexcept;
		bool shiftBook(int) noexcept;
	};
}



#define ORDER_BOOK_TEMPLATE_DECLARATION 	template <typename msgClassVariant_, typename entryType_, std::uint32_t bookLength_, size_t cacheline_,
		bool exclusiveCacheline_, size_t newLimitOrderIndex_, size_t withdrawLimitOrderIndex_, size_t marketOrderIndex_>
		requires std::signed_integral<entryType>
#define ORDER_BOOK OrderBook::orderBook<msgClassVariant_, entryType_, bookLength_, cacheline_, exclusiveCacheline_, newLimitOrderIndex_, withdrawLimitOrderIndex_, marketOrderIndex_>


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderBook(std::uint32_t basePrice_): basePrice{basePrice_} noexcept{
	//set initial values for prices
	this->maxPrice = this->basePrice + book_length;
	//allocate memory for the order book array, initialize span to access memory, fill array with zeros
	this->memoryPointer = std::aligned_alloc(alignment, this->byteLength);
	this->memSpan = std::span<bucketType>{(bucketType*)this->memoryPointer, this->bookLength};
	//initialize all bucket objects
	std::ranges::fill(memSpan,bucketType());
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::~orderBook(){
	//free allocated memory (RAII)
	std::free(this->memoryPointer);
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderResponse ORDER_BOOK::processOrder(msgClassVariant order) noexcept{
	const std::uint8_t index = order.index();
	const auto emptyOrder = orderClassTuple{newLimitOrderClass(0,0), withdrawLimitOrderClass(0,0), marketOrderClass(0)};
	orderClassPtrTuple orderPtrs;
	std::get<0>(orderPtrs) = index == 0 ? &std::get<0>(order) : &std::get<0>(orderClassTuple);
	std::get<1>(orderPtrs) = index == 1 ? &std::get<1>(order) : &std::get<1>(orderClassTuple);
	std::get<2>(orderPtrs) = index == 2 ? &std::get<2>(order) : &std::get<2>(orderClassTuple);
	//acquire write access to order book
	auto guard = AtomicGuards::atomicFlagGuard(this->modifyLock);
	guard.lock();
	++this->versionCounter;
	std::atomic_signal_fence(std::memory_order_acquire_release);
	auto ret = this->handleNewLimitOrder(*std::get<0>(orderPtrs));
	Auxil::add_to_tuple(ret, this->handleWithdrawLimitOrder(*std::get<1>(orderPtrs)));
	Auxil::add_to_tuple(ret, this->handleMarketOrder(*std::get<2>(orderPtrs)));
	std::atomic_signal_fence(std::memory_order_acquire_release);
	++this->versionCounter;
	guard.unlock();
	return ret;
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>> bestBidAsk() const noexcept{
	//safe version before reading
	std::int64_t initVersion{this->versionCounter}, finVersion{0};
	std::atomic_signal_fence(std::memory_order_acquire_release);
	do{
		auto ret = std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>{this->bestBid, this->bestOffer};
		finVersion = this->versionCounter;
		std::atomic_signal_fence(std::memory_order_acquire_release);
	}
	while(initVersion%2 || initVersion != finVersion);
	return ret;
};

ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::entryType volumeAtPrice(std::uint32_t price) const noexcept{
	std::int64_t initVersion, finVersion;
	bool inRange;
	std::uint32_t accessAtIndex;
	entryType ret;
	do{
		initVersion = this->versionCounter;
		std::atomic_signal_fence(std::memory_order_acquire_release);
		inRange = price >= this->basePrice && price <= (this->basePrice + bookLength);
		accessAtIndex = price - this->basePrice * inRange + 0 * !inRange;
		entryType ret = this->memSpan[accessAtIndex].getVolume() * inRange;
		std::atomic_signal_fence(std::memory_order_acquire_release);
		finVersion = this->versionCounter;
	}
	while(initVersion%2 || intiVersion != finVersion);
	return ret;
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::bool shiftBook(int shiftBy) noexcept{
	const bool shiftUp = shiftBy >= 0;
	//acquire exclusive/write access to order book
	auto guard = AtomicGuards::AtomicFlagGuard(this->modifyLock);
	guard.lock();
	//checkt if there are enough empty buckets to spare when book is shifted so no information is lost
	const bool sufficientDistanceShiftUp = (this->lowestBid.value_or(this->bestOffer.value_or(0)) - this->basePrice) <= shiftBy && shiftUp;
	const bool sufficientDistanceShiftDown = ((this->highestOffer.value_or(this->bestBid.value_or(0)) - (this->basePrice + bookLength))
		>= shiftBy && !shiftUp;
	//shift is always possible if the order book is empty
	const bool zeroVolume = !this->bestBid.has_value() && !this->bestOffer.has_value();
	const bool shiftPossible = sufficientDistanceShiftUp || sufficientDistanceShiftDown || zeroVolume;
	if(!shiftPossible)
		return shiftPossible;
	++this->versionCounter;
	std::atomic_signal_fence(std::memory_order_acquire_release);
	std::ranges::shift_right(this->memRange, shiftUp * shiftBy * shiftPossible);
	std::ranges::shift_left(this->memRange, (-1) * shiftBy * !shiftUp * shiftPossible);
	this->basePrice += shiftBy * shiftPossible;
	std::atomic_signal_fence(std::memory_order_acquire_release);
	++this->versionCounter;
	return shiftPossible;
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::bool priceInRange(std::uint32_t price) noexcept{
	return price <= (this->basePrice + bookLength) && price >= this->basePrice;
};


//check if order has exhausted entire suppy or demand on the book, use monadic operations to reset stats if necessary
ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::void resetStatsIfExhausted(bool buyOrder) noexcept{
	const bool supplyExhausted = buyOrder && this->memSpan[this->highestOffer - this->basePrice].getVolume() == 0;
	const bool demandExhausted = !buyOrder && this->memSpan[this->lowestBid - this->basePrice].getVolume() == 0;
	this->bestOffer.and_then([&](std::uint32_t arg){return supplyExhausted ? std::nullopt : arg;});
	this->highestOffer.and_then([&](std::uint32_t arg){return supplyExhausted ? std::nullopt : arg;});
	this->bestBid.and_then([&](std::uint32_t arg){return demandExhausted ? std::nullopt : arg;});
	this->lowestBid.and_then([&](std::uint32_t arg){return demandExhausted ? std::nullopt : arg;});
};


//return std::optional with index of next liquid price bucket in direction specified by second argument, returns empty optional if no liquid price bucket is found
ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::std::optional<std::uint32_t> findLiquidBucket(std::uint32_t startIndex, bool searchUp) noexcept{
	const std::int8_t increment = -1 + 2*searchUp;
	const std::uint32_t limitIndex = 0 + bookLength * searchUp;
	std::uint32_t retIndex{0}, index{startIndex + increment};
	bool success{false};
	while(!success && index != limitIndex){
		success = this->memSpan[index].getVolume() != 0;
		retIndex += success * (this->basePrice + index);
		index += increment;
	}
	return success ? retIndex : std::nullopt;
};


ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderResponse ORDER_BOOK::runThroughBook(entryType volume, std::uint32_t nBuckets) noexcept{
	//"import" order book stats as local variables, set variables to zero if no liquidity 
	const std::uint32_t basePrice_{this->basePrice}, bestBid_{this->bestBid.value_or(0)}, lowestBid_{this->lowestBid.value_or(0)},
		bestOffer_{this->bestOffer.value_or(0)}, highestOffer_{this->highestOffer.value_or(0)};
	const bool buyOrder = volume > 0;
	std::uint32_t n = (nBuckets != uintDefaultArg)*nBuckets  
		+ (nBuckets == uintDefaultArg) * buyOrder * (highestOffer_ - bestOffer_)
		+ (nBuckets == uintDefaultArg) * !buyOrder * (bestBid_ - lowestBid_);
	const int increment = 1 - 2*!buyOrder;
	std::uint32_t pIndex = buyOrder * bestOffer_+ !buyOrder * bestBid_;
	entryType desiredVol{volume}, entryType filledVol{0}, entryType orderRevenue{0};
	while(n != 0 && desiredVol != 0){
		filledVol = this->memSpan[pIndex].consumeVolume(desiredVol);
		orderRevenue += usedVol * (pIndex + this->basePrice);
		//used volume and desired volume will have opposite signs, addition hence shrinks absolute value of desired volume
		desiredVol -= usedVol;
		pIndex += increment;
		--n;
	}
	this->resetStatsIfExhausted(buyOrder);
	//update stats for bestBid/bestOffer
	this->bestOffer.transform([&](std::uint32_t arg){return buyOrder ? basePrice_+pIndex : arg;});
	this->bestBid.transform([&](std::uint32_t arg){return !buyOrder ? basePrice_+pIndex : arg;});
	return orderResponse{basePrice_ + pIndex, filledVol, orderRevenue, 0};
};


ORDER_BOOK_ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderResponse ORDER_BOOK::handleNewLimitOrder(const std::variant_alternative_t<newLimitOrderIndex, msgClassVariant_>& order) noexcept{
	std::uint32_t price = order.price;
	//check if price is out of range, if so set order volume to 0 but proceed
	const bool priceInRange = this->priceInRange(price);
	price = price*priceInRange + this->basePrice * !priceInRange
	const std::int32_t volume = order.volume * priceInRange;
	const std::uint32_t basePrice_{this->basePrice}, bestBid_{this->bestBid.value_or(0)}, lowestBid_{this->lowestBid.value_or(0)},
		bestOffer_{this->bestOffer.value_or(0)}, highestOffer_{this->highestOffer.value_or(0)};
	const bool buyOrder = volume > 0;
	//fill order volume immediately if possible
	const bool fillable = (buyOrder && price >= bestOffer) || (!buyOrder && price <= bestBid);
	orderResponse ret{0,0,0, 1*!priceInRange};
	ret = !fillable ? ret : this->runThroughBook(volume, buyOrder*(price - bestOffer) + !buyOrder * (bestBid - price));
	const entryType unfilledVol = volume - std::get<1>(ret);
	this->memSpan[price - basePrice].addVolume(unfilledVol);
	this->lowestBid.transform([&](std::uint32_t arg){return buyOrder ? std::min(arg, price) : arg;});
	this->highestOffer.transform([&](std::uint32_t arg){return !buyOrder ? std::max(arg, price) : arg;});
	return ret;
};


ORDER_BOOK_ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderResponse ORDER_BOOK::handleWithdrawLimitOrder(const std::variant_alternative_t<withdrawLimitOrderIndex, msgClassVariant_>& order) noexcept{
	std::uint32_t price = order.price;
	//check if price is out of range, if so set order volume to 0 but proceed
	const bool priceInRange = this->priceInRange(price);
	price = price*priceInRange + this->basePrice * !priceInRange
	const std::int32_t volume = order.volume * priceInRange;
	const bool withdrawSupply = volume > 0;
	const std::int32_t volumeWithdrawn = this->memSpan[price - this->basePrice].consumeLiquidity(volume);
	const bool liquidityExhausted = this->memSpan[price - this->basePrice].getVolume() == 0;
	this->bestBid.and_then([&](std::uint32_t arg){return (price != arg || !liquidityExhausted) ? arg : this->findLiquidBucket(price, false);});
	this->lowestBid.and_then([&](std::uint32_t arg){return (price != arg || !liquidityExhausted) ? arg : this->findLiquidBucket(price, true);});
	this->bestOffer.and_then([&](std::uint32_t arg){return (price != arg || !liquidityExhausted) ? arg : this->findLiquidBucket(price, true);});
	this->highestOffer.and_then([&](std::uint32_t arg){return (price != arg || !liquidityExhausted) ? arg : this->findLiquidBucket(price, false);});
	return orderResponse{price, volumeWithdrawn, 0, 1*!priceInRange};
};


ORDER_BOOK_ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderResponse ORDER_BOOK::handleMarketOrder(const std::variant_alternative_t<marketOrderIndex, msgClassVariant_>& order) noexcept{
	return this->runThroughBook(order.volume);
};


#undef ORDER_BOOK_TEMPLATE_DECLARATION
#undef ORDER_BOOK
