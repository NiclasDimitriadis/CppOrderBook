#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>

#include "AtomicGuards.hpp"
#include "Auxil.hpp"
#include "FIXmsgTypeChecks.hpp"
#include "OrderBookBucket.hpp"

namespace OrderBook {
template <typename msgClassVariant_, typename entryType_,
          std::uint32_t bookLength_, bool exclusiveCacheline_,
          size_t newLimitOrderIndex_, size_t withdrawLimitOrderIndex_,
          size_t marketOrderIndex_>
requires std::signed_integral<entryType_> &&
    MsgTypeChecks::OrderBookLimitOrderInterface<
        msgClassVariant_, newLimitOrderIndex_,
        withdrawLimitOrderIndex_>::value &&
    MsgTypeChecks::OrderBookMarketOrderInterface<
        msgClassVariant_, marketOrderIndex_>::value
 struct orderBook {
 private:
  // static members and member types
  static constexpr size_t cacheline = 64;
  static constexpr std::uint32_t alignment =
      std::max(Auxil::divisible_or_ceil(sizeof(entryType_), cacheline),
               cacheline* exclusiveCacheline_);
  using bucketType = OrderBookBucket::orderBookBucket<entryType_, alignment>;
  static constexpr size_t newLimitOrderIndex = newLimitOrderIndex_;
  static constexpr size_t withdrawLimitOrderIndex = withdrawLimitOrderIndex_;
  static constexpr size_t marketOrderIndex = marketOrderIndex_;
  using newLimitOrderClass =
      std::variant_alternative_t<newLimitOrderIndex, msgClassVariant_>;
  using withdrawLimitOrderClass =
      std::variant_alternative_t<withdrawLimitOrderIndex, msgClassVariant_>;
  using marketOrderClass =
      std::variant_alternative_t<marketOrderIndex, msgClassVariant_>;
  using orderClassTuple =
      std::tuple<newLimitOrderClass, withdrawLimitOrderClass, marketOrderClass>;
  // contains: marginal execution price, filled volume, total revenue
  // generated(price*volume), 1 if price is out of bounds of the orderbook
  using orderResponse =
      std::tuple<std::uint32_t, entryType_, entryType_, std::uint8_t>;

  // non static member variables
  std::uint32_t basePrice;
  std::optional<std::uint32_t> bestBid, lowestBid, bestOffer, highestOffer;
  alignas(64) std::atomic_flag modifyLock = false;
  alignas(64) std::int64_t versionCounter = 0;
  // points to memory location of the actual orderbook entries
  alignas(64) void* memoryPointer;
  std::span<OrderBookBucket::orderBookBucket<entryType_, alignment>> memSpan;

  // private member functions
  bool priceInRange(std::uint32_t) noexcept;
  //searches for closest buckets containing non-zero volume
  //to be used when updating stats
  __attribute__((flatten)) std::optional<std::uint32_t> findLiquidBucket(
      std::uint32_t, std::int32_t) noexcept;
  __attribute__((flatten))
  //iterates over portion of the order book to withdraw liquidity/ fill volume
  //to be used for market orders and limit orders that can be filled immediately
  orderResponse runThroughBook(entryType_, std::int32_t) noexcept;
  //inserts limt order, fills limt order if possible
  __attribute__((flatten)) orderResponse handleNewLimitOrder(
      const newLimitOrderClass&) noexcept;
  __attribute__((flatten)) orderResponse handleWithdrawLimitOrder(
      const withdrawLimitOrderClass&) noexcept;
  __attribute__((flatten)) orderResponse handleMarketOrder(
      const marketOrderClass&) noexcept;

 public:
  // static members and member types
  using entryType = entryType_;
  using msgClassVariant = msgClassVariant_;
  static constexpr std::uint32_t bookLength = bookLength_;
  static constexpr std::uint32_t byteLength = bookLength * alignment;

  // 5 special members
  explicit orderBook(std::uint32_t);
  ~orderBook();
  // delete copying and moving, avoid checks for unspecified state
  orderBook(const orderBook&) = delete;
  orderBook& operator=(const orderBook&) = delete;
  orderBook(orderBook&&) = delete;
  orderBook& operator=(orderBook&&) = delete;

  // public member functions
  __attribute__((flatten)) orderResponse processOrder(msgClassVariant) noexcept;
  std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>
  bestBidAsk() const noexcept;
  entryType volumeAtPrice(std::uint32_t) const noexcept;
  bool shiftBook(std::int32_t) noexcept;

  // checks coherence of stats among each other and with liquidty in order book
  // intended for debugging purposes
  std::uint64_t __invariantsCheck(int) const;
};
}  // namespace OrderBook

#define ORDER_BOOK_TEMPLATE_DECLARATION                                  \
  template <typename msgClassVariant_, typename entryType_,              \
            std::uint32_t bookLength_, bool exclusiveCacheline_,         \
            size_t newLimitOrderIndex_, size_t withdrawLimitOrderIndex_, \
            size_t marketOrderIndex_>                                    \
  requires std::signed_integral<entryType_> &&                           \
      MsgTypeChecks::OrderBookLimitOrderInterface<                       \
          msgClassVariant_, newLimitOrderIndex_,                         \
          withdrawLimitOrderIndex_>::value &&                            \
      MsgTypeChecks::OrderBookMarketOrderInterface<msgClassVariant_,     \
                                                   marketOrderIndex_>::value

#define ORDER_BOOK                                                          \
  OrderBook::orderBook<msgClassVariant_, entryType_, bookLength_, exclusiveCacheline_, \
            newLimitOrderIndex_, withdrawLimitOrderIndex_, marketOrderIndex_>

//namespace OrderBook {

ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::orderBook(std::uint32_t basePrice_) : basePrice{basePrice_} {
  // set initial values for prices
  // allocate memory for the order book array, initialize span to access memory,
  // fill array with zeros
  this->memoryPointer = std::aligned_alloc(alignment, this->byteLength);
  this->memSpan =
      std::span<bucketType>{(bucketType*)this->memoryPointer, this->bookLength};
  // initialize all bucket objects
  std::ranges::fill(memSpan, bucketType());
};

ORDER_BOOK_TEMPLATE_DECLARATION
ORDER_BOOK::~orderBook() {
  // free allocated memory (RAII)
  std::free(this->memoryPointer);
};


ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::orderResponse ORDER_BOOK::processOrder(
    msgClassVariant order) noexcept {
  const std::uint8_t index = order.index();
  
  //create tuple of blank orders for each type and insert the order to be processed in correct spot
  auto orders =
      orderClassTuple{newLimitOrderClass(0, 0), withdrawLimitOrderClass(0, 0),
                      marketOrderClass(0)};
  std::get<0>(orders) = index == 0 ? std::get<0>(order) : std::get<0>(orders);
  std::get<1>(orders) = index == 1 ? std::get<1>(order) : std::get<1>(orders);
  std::get<2>(orders) = index == 2 ? std::get<2>(order) : std::get<2>(orders);
  
  // acquire write access to order book, set version counter to odd value to invlaidate reads while order book is manipulated
  auto guard = AtomicGuards::AtomicFlagGuard(&this->modifyLock);
  guard.lock();
  ++this->versionCounter;
  std::atomic_signal_fence(std::memory_order_acq_rel);
  
  // call handler for all order types, use blank orders for irrelevant types
  auto ret = this->handleNewLimitOrder(std::get<0>(orders));
  Auxil::add_to_tuple(ret, this->handleWithdrawLimitOrder(std::get<1>(orders)));
  Auxil::add_to_tuple(ret, this->handleMarketOrder(std::get<2>(orders)));
  
  //release lock, set increment counter to even value
  std::atomic_signal_fence(std::memory_order_acq_rel);
  ++this->versionCounter;
  guard.unlock();

  return ret;
};


ORDER_BOOK_TEMPLATE_DECLARATION
std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>
ORDER_BOOK::bestBidAsk() const noexcept {
  // safe version before reading
  std::int64_t initVersion{this->versionCounter}, finVersion{0};
  std::atomic_signal_fence(std::memory_order_acq_rel);
  auto ret =
      std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>();
  do {
    ret =
        std::tuple<std::optional<std::uint32_t>, std::optional<std::uint32_t>>{
            this->bestBid, this->bestOffer};
    finVersion = this->versionCounter;
    std::atomic_signal_fence(std::memory_order_acq_rel);
  } while (initVersion % 2 || initVersion != finVersion);
  return ret;
};

ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::entryType ORDER_BOOK::volumeAtPrice(
    std::uint32_t price) const noexcept {
  std::int64_t initVersion, finVersion;
  bool inRange;
  std::uint32_t accessAtIndex;
  entryType ret;
  do {
    initVersion = this->versionCounter;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    inRange =
        price >= this->basePrice && price <= (this->basePrice + bookLength);
    accessAtIndex = price - this->basePrice * inRange + 0 * !inRange;
    // avoid out of bounds memory access if volume is read while book is in the
    // process of shifting
    accessAtIndex = std::min(accessAtIndex, bookLength);
    ret = inRange ? this->memSpan[accessAtIndex].getVolume() : 0;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    finVersion = this->versionCounter;
  } while (!initVersion % 2 || initVersion != finVersion);
  return ret;
};

ORDER_BOOK_TEMPLATE_DECLARATION
bool ORDER_BOOK::shiftBook(std::int32_t shiftBy) noexcept {
  const bool shiftUp = shiftBy >= 0;
  const bool posBasePrice =
      (static_cast<std::int32_t>(this->basePrice) + shiftBy) >= 0;
  // acquire exclusive/write access to order book
  auto guard = AtomicGuards::AtomicFlagGuard(&this->modifyLock);
  guard.lock();
  // check if there are enough empty buckets to spare when book is shifted so no
  // information is lost, negative shift
  const std::int32_t maxPrice = this->basePrice + bookLength;
  const std::int32_t freeBucketsTop =
      maxPrice - static_cast<std::int32_t>(this->highestOffer.value_or(
                     this->bestBid.value_or(this->basePrice)));
  const std::int32_t freeBucketsBottom =
      static_cast<std::int32_t>(
          this->lowestBid.value_or(this->bestOffer.value_or(maxPrice))) -
      this->basePrice;
  const bool sufficientDistanceShiftUp =
      shiftUp && freeBucketsBottom >= shiftBy;
  const bool sufficientDistanceShiftDown =
      !shiftUp && freeBucketsTop >= -shiftBy;
  // shift is always possible if the order book is empty and base price remains
  // >= 0
  const bool zeroVolume =
      !this->bestBid.has_value() && !this->bestOffer.has_value();
  const bool shiftPossible = (sufficientDistanceShiftUp ||
                              sufficientDistanceShiftDown || zeroVolume) &&
                             posBasePrice;
  // no need to perform actual shift if book is empty, just adjust basePrice
  const std::int32_t memShift = shiftBy * shiftPossible * !zeroVolume;
  std::atomic_signal_fence(std::memory_order_acq_rel);
  std::shift_left(this->memSpan.begin(), this->memSpan.end(),
                  shiftUp * memShift);
  std::shift_right(this->memSpan.begin(), this->memSpan.end(),
                   (-1) * !shiftUp * memShift);
  this->basePrice += shiftBy * shiftPossible;
  std::atomic_signal_fence(std::memory_order_acq_rel);
  ++this->versionCounter;
  return shiftPossible;
};

ORDER_BOOK_TEMPLATE_DECLARATION
bool ORDER_BOOK::priceInRange(std::uint32_t price) noexcept {
  return price <= (this->basePrice + bookLength) && price >= this->basePrice;
};

// return std::optional with index of first non empty price bucket from
// startIndex to endIndex returns optional containing the price (NOT INDEX!) of
// liquid bucket, empty optional if search is unsuccesful returns empty optional
// if negative endIndex is passed
ORDER_BOOK_TEMPLATE_DECLARATION
std::optional<std::uint32_t> ORDER_BOOK::findLiquidBucket(
    std::uint32_t startPrice, std::int32_t endPrice) noexcept {
  const std::int32_t startIndex{startPrice -
                                static_cast<std::int32_t>(this->basePrice)};
  const std::int32_t endIndex =
      endPrice >= 0 ? endPrice - static_cast<std::int32_t>(this->basePrice)
                    : startIndex;
  const std::int8_t increment = 1 - 2 * (startIndex > endIndex);
  // skip loop if negative/dummy index was passed
  std::int32_t retPrice{0}, index{startIndex};
  bool endIndexReached{false};
  bool success{false};
  while (!success && !endIndexReached) {
    success = this->memSpan[index].getVolume() != 0;
    retPrice += success * (this->basePrice + index);
    endIndexReached = index == endIndex;
    index += increment;
  };
  return success ? std::make_optional(static_cast<std::uint32_t>(retPrice))
                 : std::nullopt;
};

ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::orderResponse ORDER_BOOK::runThroughBook(
    entryType volume, std::int32_t endPrice) noexcept {
  const bool buyOrder = volume < 0;
  const std::int32_t startPrice = buyOrder * this->bestOffer.value_or(-1) +
                                  !buyOrder * this->bestBid.value_or(-1);
  const bool dummyRun = endPrice < 0;
  const std::int8_t increment = -1 + 2 * buyOrder;
  std::int32_t nIterations = (endPrice - startPrice) * increment;
  entryType openVol{volume * !dummyRun}, filledVol{0}, orderRevenue{0};
  std::int32_t index{startPrice - static_cast<std::int32_t>(this->basePrice)};
  std::int32_t transactionIndex{0};
  while (nIterations >= 0 && openVol != 0 && index >= 0) {
    filledVol = this->memSpan[index].consumeLiquidity(-openVol);
    transactionIndex = filledVol != 0 ? index : transactionIndex;
    orderRevenue -= filledVol * (index + this->basePrice);
    // open volume and filled volume will have opposite signs, addition hence
    // shrinks absolute value of open volume
    openVol += filledVol;
    index += increment;
    --nIterations;
  };
  // update stats for bestBid/bestOffer
  const std::int32_t flbEndPrice =
      -1 * dummyRun + !dummyRun * (buyOrder * this->highestOffer.value_or(-1) +
                                   !buyOrder * this->lowestBid.value_or(-1));

  this->bestOffer = this->bestOffer.and_then([&](std::uint32_t arg) {
    return buyOrder
               ? this->findLiquidBucket(this->bestOffer.value(), flbEndPrice)
               : arg;
  });
  this->highestOffer = this->highestOffer.and_then([&](std::uint32_t arg) {
    return this->bestOffer.has_value() ? std::make_optional(arg) : std::nullopt;
  });

  this->bestBid = this->bestBid.and_then([&](std::uint32_t arg) {
    return !buyOrder
               ? this->findLiquidBucket(this->bestBid.value(), flbEndPrice)
               : arg;
  });
  this->lowestBid = this->lowestBid.and_then([&](std::uint32_t arg) {
    return this->bestBid.has_value() ? std::make_optional(arg) : std::nullopt;
  });

  return orderResponse{!dummyRun * (this->basePrice + transactionIndex),
                       !dummyRun * (volume - openVol), orderRevenue, 0};
};

ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::orderResponse ORDER_BOOK::handleNewLimitOrder(
    const newLimitOrderClass& order) noexcept {
  std::uint32_t price = order.orderPrice;
  
  // check if price is out of range, if so set order volume to 0
  // and price to base price to avoid out of bounds memory access, then proceed regularly
  const bool priceInRange = order.orderVolume == 0 || this->priceInRange(price);
  price = price * priceInRange + this->basePrice * !priceInRange;
  const std::int32_t volume = order.orderVolume * priceInRange;
  const bool dummyOrder = volume == 0;
  const bool buyOrder = volume < 0;
  
  // fill order volume immediately if possible
  const bool crossedBook =
      !dummyOrder * ((buyOrder && this->bestOffer.has_value() &&
                      (price >= this->bestOffer.value_or(0))) ||
                     (!buyOrder && this->bestBid.has_value() &&
                      (price <= this->bestBid.value_or(0))));
  orderResponse ret{0, 0, 0, 0 + 1 * !priceInRange};
  const std::int32_t runEndPrice =
      !crossedBook * (-1) +
      crossedBook * (buyOrder * this->highestOffer.value_or(-1) +
                  !buyOrder * this->lowestBid.value_or(-1));
  ret = this->runThroughBook(volume, runEndPrice);
  
  // insert error code if price is out of range
  std::get<3>(ret) += 1 * !priceInRange;
  
  // enter liquidity into order book
  const entryType unfilledVol = volume - std::get<1>(ret);
  const bool liqAdded = unfilledVol != 0;
  !dummyOrder ? this->memSpan[price - basePrice].addLiquidity(unfilledVol)
              : void();

  // check which statistic (if any) needs to be updated
  const bool updateLowestBid =
      liqAdded && buyOrder &&
      (!this->lowestBid.has_value() || this->lowestBid.value() > price);
  const bool updateBestBid =
      liqAdded && buyOrder &&
      (!this->bestBid.has_value() || this->bestBid.value() < price);
  const bool updateHighestOffer =
      liqAdded && !buyOrder &&
      (!this->highestOffer.has_value() || this->highestOffer.value() < price);
  const bool updateBestOffer =
      liqAdded && !buyOrder &&
      (!this->bestOffer.has_value() || this->bestOffer.value() > price);

  // perform update
  this->lowestBid =
      updateLowestBid ? this->lowestBid.emplace(price) : this->lowestBid;
  this->bestBid = updateBestBid ? this->bestBid.emplace(price) : this->bestBid;
  this->highestOffer = updateHighestOffer ? this->highestOffer.emplace(price)
                                          : this->highestOffer;
  this->bestOffer =
      updateBestOffer ? this->bestOffer.emplace(price) : this->bestOffer;

  return ret;
};

ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::orderResponse ORDER_BOOK::handleWithdrawLimitOrder(
    const withdrawLimitOrderClass& order) noexcept {
  std::uint32_t price = order.orderPrice;
  
  // check if price is out of range, if so set order volume to 0 but proceed
  // set priceInRange to true for volume == 0 to avoid error code in order
  // response for default/dummy withdraw messages
  const bool hasVolume = order.orderVolume != 0;
  const bool priceInRange = !hasVolume || this->priceInRange(price);
  price = price * priceInRange + this->basePrice * !priceInRange;
  const std::int32_t volume = order.orderVolume * priceInRange;
  const bool withdrawSupply = volume > 0;
  
  //perform actual withdrawel
  const std::int32_t volumeWithdrawn =
      hasVolume
          ? this->memSpan[price - this->basePrice].consumeLiquidity(volume)
          : 0;
  
  //upate stats if necessary
  const bool liquidityExhausted =
      !hasVolume || this->memSpan[price - this->basePrice].getVolume() == 0;
  this->bestBid = this->bestBid.and_then([&](std::uint32_t arg) {
    return (price != arg || !liquidityExhausted)
               ? std::make_optional(arg)
               : this->findLiquidBucket(price, this->lowestBid.value());
  });
  this->lowestBid = this->lowestBid.and_then([&](std::uint32_t arg) {
    return (price != arg || !liquidityExhausted)
               ? std::make_optional(arg)
               : this->findLiquidBucket(price, this->bestBid.value_or(-1));
  });
  this->bestOffer = this->bestOffer.and_then([&](std::uint32_t arg) {
    return (price != arg || !liquidityExhausted)
               ? std::make_optional(arg)
               : this->findLiquidBucket(price, this->highestOffer.value());
  });
  this->highestOffer = this->highestOffer.and_then([&](std::uint32_t arg) {
    return (price != arg || !liquidityExhausted)
               ? std::make_optional(arg)
               : this->findLiquidBucket(price, this->bestOffer.value_or(-1));
  });
  return orderResponse{0, volumeWithdrawn, 0, 1 * !priceInRange};
};


ORDER_BOOK_TEMPLATE_DECLARATION
typename ORDER_BOOK::orderResponse ORDER_BOOK::handleMarketOrder(
    const marketOrderClass& order) noexcept {
  const std::int32_t volume = order.orderVolume;
  const std::int32_t runEndPrice =
      -1 * (volume == 0) + (volume < 0) * (this->highestOffer.value_or(-1)) +
      (volume > 0) * (this->lowestBid.value_or(-1));
  return this->runThroughBook(order.orderVolume, runEndPrice);
};


ORDER_BOOK_TEMPLATE_DECLARATION
std::uint64_t ORDER_BOOK::__invariantsCheck(int nIt) const {
  std::uint64_t errorCode{0};
  auto isPositive = [](bucketType b) { return b.getVolume() >= 0; };
  auto isNegative = [](bucketType b) { return b.getVolume() <= 0; };
  auto isZero = [](bucketType b) { return b.getVolume() == 0; };
  auto invalidStat = [&](std::optional<std::uint32_t> stat) {
    return stat.has_value() &&
           memSpan[stat.value() - basePrice].getVolume() == 0;
  };

  if (bestBid.has_value() && lowestBid.has_value()) {
    std::uint32_t lbIndex = lowestBid.value() - basePrice;
    std::uint32_t bbIndex = bestBid.value() - basePrice;
    auto lbSubspan = memSpan.subspan(0, lbIndex);
    auto bbSubspan = memSpan.subspan(bbIndex + 1, bookLength_ - bbIndex - 1);
    bool wrongBelow = !std::ranges::all_of(lbSubspan, isZero);
    bool wrongAbove = !std::ranges::all_of(bbSubspan, isPositive);
    if (wrongAbove) {
      std::cout << "invalid entry above best bid: " << nIt << "\n";
      errorCode += 1;
    }
    if (wrongBelow) {
      std::cout << "invalid entry below lowest bid" << nIt << "\n";
      errorCode += 10;
    }
  }

  if (bestOffer.has_value() && highestOffer.has_value()) {
    std::uint32_t hoIndex = highestOffer.value() - basePrice;
    std::uint32_t boIndex = bestOffer.value() - basePrice;
    auto boSubspan = memSpan.subspan(0, boIndex);
    auto hoSubspan = memSpan.subspan(hoIndex + 1, bookLength_ - hoIndex - 1);
    bool wrongAbove = !std::ranges::all_of(hoSubspan, isZero);
    bool wrongBelow = !std::ranges::all_of(boSubspan, isNegative);
    if (wrongAbove) {
      std::cout << "invalid entry above highest offer: " << nIt << "\n";
      errorCode += 100;
    }
    if (wrongBelow) {
      std::cout << "invalid entry below best offer" << nIt << "\n";
      errorCode += 1000;
    }
  }

  if (bestBid.has_value() && bestOffer.has_value() &&
      (bestBid.value() >= bestOffer.value())) {
    std::cout << "bestBid >= bestOffer"
              << "\n";
    errorCode += 10000;
  }

  if (bestBid.has_value() != lowestBid.has_value()) {
    std::cout << "bestBid.has_value() != lowestBid.has_value(): " << nIt
              << "\n";
    errorCode += 100000;
  };

  if (bestOffer.has_value() != highestOffer.has_value()) {
    std::cout << "bestOffer.has_value() != highestOffer.has_value(): " << nIt
              << "\n";
    errorCode += 1000000;
  };

  if (bestBid.value_or(0) < lowestBid.value_or(0)) {
    std::cout << "best bid < lowest bid: " << nIt << "\n";
    errorCode += 10000000;
  };

  if (bestOffer.value_or(0) > highestOffer.value_or(0)) {
    std::cout << "best offer > highest offer: " << nIt << "\n";
    errorCode += 100000000;
  };

  if (invalidStat(lowestBid)) {
    std::cout << "no liquidity at lowest bid: " << nIt << "\n";
    errorCode += 1000000000;
  }
  if (invalidStat(bestBid)) {
    std::cout << "no liquidity at best bid: " << nIt << "\n";
    errorCode += 10000000000;
  }
  if (invalidStat(bestOffer)) {
    std::cout << "no liquidity at best offer: " << nIt << "\n";
    errorCode += 100000000000;
  }
  if (invalidStat(highestOffer)) {
    std::cout << "no liquidity at high offer: " << nIt << "\n";
    errorCode += 1000000000000;
  }

  return errorCode;
};

//}  // namespace OrderBook

#undef ORDER_BOOK_TEMPLATE_DECLARATION
#undef ORDER_BOOK