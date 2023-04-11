// class for handling a single socket, retrieving FIX SBE messages that come
// with Simple Open Framing Header, constructing an object representing one of
// multiple types of messages in a std::variant and emplacing it in some data
// structure to be consumed elsewhere
#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>

#include "Auxil.hpp"
#include "FIXmsgTypeChecks.hpp"

namespace FIXSocketHandler {
template <typename msgClassVariant_, typename socketType_>
requires Auxil::readableAsSocket<socketType_> &&
    MsgTypeChecks::consistencyChecksAcrossMsgTypes<msgClassVariant_>::value &&
    MsgTypeChecks::alternativesDefaultConstructible<
        msgClassVariant_>::value struct fixSocketHandler {
 private:
  // static data about byte layout of FIX messages
  static constexpr std::uint32_t delimitOffset =
      std::variant_alternative_t<0, msgClassVariant_>::delimiterOffset;
  static constexpr std::uint16_t delimitValue =
      std::variant_alternative_t<0, msgClassVariant_>::delimiterValue;
  static constexpr std::uint32_t lengthOffset =
      std::variant_alternative_t<0, msgClassVariant_>::lengthOffset;
  static constexpr std::uint8_t lengthLen = sizeof(
      decltype(std::variant_alternative_t<0, msgClassVariant_>::lengthOffset));
  static constexpr std::uint32_t headerLength =
      std::variant_alternative_t<0, msgClassVariant_>::headerLength;
  static constexpr std::uint32_t bufferSize =
      MsgTypeChecks::determineMaxMsgLen<msgClassVariant_>::value;
  static constexpr std::uint32_t shortestMsgSize =
      MsgTypeChecks::determineMinMsgLen<msgClassVariant_>::value;
  static_assert(shortestMsgSize > headerLength);
  // index sequence used to generate static "loop" over message types
  static constexpr auto indexSeq =
      std::make_index_sequence<std::variant_size_v<msgClassVariant_>>();
  void* readBuffer = nullptr;
  // span used to access buffer
  std::span<std::uint8_t, bufferSize> bufferSpan;
  socketType_* socketPtr;
  __attribute__((flatten)) bool validateChecksum(
      std::uint32_t, const std::span<std::uint8_t, bufferSize>&) noexcept;
  // finds delimiter of next message if it isn't found in the expected place
  __attribute__((noinline)) int scanForDelimiter() noexcept;
  // safely discards excess bytes if an incoming message is longer than any of
  // the message types passed to buffer (and hence longer than buffer) returns
  // size of buffer for caller to assign to message-length variable so all
  // remaining bytes can be read within regular program logic
  __attribute__((noinline)) std::uint32_t discardLongMsg(std::uint32_t,
                                                         void*) noexcept;

  // member function templates to generate "loop" over all message types to find
  // and construct the correct one
  template <std::size_t... Is>
  int determineMsgType(int, std::index_sequence<Is...>) noexcept;

  template <std::size_t... Is>
  msgClassVariant_ constructVariant(int, std::span<std::uint8_t>,
                                    std::index_sequence<Is...>) noexcept;

 public:
  explicit fixSocketHandler(socketType_*);
  ~fixSocketHandler();
  fixSocketHandler(const fixSocketHandler&) = delete;
  fixSocketHandler(fixSocketHandler&&) = delete;
  fixSocketHandler& operator=(const fixSocketHandler&) = delete;
  fixSocketHandler& operator=(fixSocketHandler&&) = delete;
  using msgClassVariant = msgClassVariant_;
  typedef socketType_ socketType;
  // public interface for reading FIX messages from socket
  __attribute__((flatten)) std::optional<msgClassVariant>
  readNextMessage() noexcept;
};
};  // namespace FIXSocketHandler

#define TEMPL_TYPES                                          \
  template <typename msgClassVariant_, typename socketType_> \
  requires Auxil::readableAsSocket<socketType_> &&           \
      MsgTypeChecks::consistencyChecksAcrossMsgTypes<        \
          msgClassVariant_>::value &&                        \
      MsgTypeChecks::alternativesDefaultConstructible<msgClassVariant_>::value

#define SOCK_HANDLER \
  FIXSocketHandler::fixSocketHandler<msgClassVariant_, socketType_>

TEMPL_TYPES
std::optional<msgClassVariant_> SOCK_HANDLER::readNextMessage() noexcept {
  // read length-of-header bytes into buffer
  std::int32_t bytesRead = this->socketPtr->recv(readBuffer, headerLength);

  // verifiy that header is valid by checking whether delimeter sequence is in
  // expected place
  const std::uint16_t delimitValLocal = delimitValue;
  const auto delimitSpan = std::span<const std::uint8_t, 2>{
      reinterpret_cast<const std::uint8_t*>(&delimitValLocal), 2};

  // determine if bytes read are a valid header, find valid header if they are
  // not
  const bool validHeader = std::ranges::equal(
      this->bufferSpan.subspan(delimitOffset, 2), delimitSpan);
  bytesRead = validHeader ? bytesRead : scanForDelimiter();

  // extract message length from message header
  int msgLength =
      *(std::uint32_t*)(reinterpret_cast<void*>(&bufferSpan[lengthOffset]));
  msgLength = msgLength <= bufferSize
                  ? msgLength
                  : this->discardLongMsg(msgLength, this->readBuffer);

  // read rest of message into buffer
  void* recvDest = reinterpret_cast<void*>(&this->bufferSpan[bytesRead]);
  this->socketPtr->recv(recvDest, msgLength - bytesRead);

  // determine type of received message by checking all types in variant
  static constexpr auto variantIndexSeq =
      std::make_index_sequence<std::variant_size_v<msgClassVariant_>>();
  int msgTypeIndex = determineMsgType(msgLength, variantIndexSeq);

  // validate message via checksum, if invalid message is discarded by setting
  // type-index to -1, indicating cold path
  msgTypeIndex = validateChecksum(msgLength, bufferSpan) ? msgTypeIndex : -1;

  // enqueue message to buffer
  const auto returnVariant =
      this->constructVariant(msgTypeIndex, this->bufferSpan, variantIndexSeq);
  std::optional<msgClassVariant> ret =
      msgTypeIndex >= 0 ? std::make_optional(returnVariant) : std::nullopt;
  return ret;
};

TEMPL_TYPES
bool SOCK_HANDLER::validateChecksum(
    std::uint32_t msgLength,
    const std::span<std::uint8_t, bufferSize>& buffer) noexcept {
  auto checkSumSubspan = buffer.subspan(0, msgLength - 3);
  std::uint8_t byteSum = 0;
  byteSum =
      std::accumulate(checkSumSubspan.begin(), checkSumSubspan.end(), byteSum);
  // convert three ASCII characters at the end of FIX msg to unsigned 8 bit int
  // to compare to checksum computed above
  std::uint8_t msgChecksum = ((this->bufferSpan[msgLength - 3]) - 48) * 100 +
                             ((bufferSpan[msgLength - 2]) - 48) * 10 +
                             ((bufferSpan[msgLength - 1]) - 48);
  return byteSum == msgChecksum;
};

TEMPL_TYPES
int SOCK_HANDLER::scanForDelimiter() noexcept {
  static constexpr int bytesToKeep{headerLength - 1};
  // discard first byte
  auto searchSpan = this->bufferSpan.subspan(0, shortestMsgSize);
  std::shift_left(searchSpan.begin(), searchSpan.end(), 1);
  void* recvDest = reinterpret_cast<void*>(&searchSpan[bytesToKeep]);
  this->socketPtr->recv(recvDest, shortestMsgSize - bytesToKeep);
  const std::uint16_t delimitValLocal = delimitValue;
  const auto delimitValSpan = std::span<const std::uint8_t, 2>{
      reinterpret_cast<const std::uint8_t*>(&delimitValLocal), 2};
  auto searchResult = std::ranges::search(searchSpan, delimitValSpan);
  bool delimiterFound = !searchResult.empty();
  while (!delimiterFound) {
    std::shift_left(searchSpan.begin(), searchSpan.end(),
                    shortestMsgSize - bytesToKeep);
    this->socketPtr->recv(recvDest, shortestMsgSize - bytesToKeep);
    searchResult = std::ranges::search(searchSpan, delimitValSpan);
    delimiterFound = !searchResult.empty();
  }
  const std::uint32_t positionOfHeader =
      std::distance(searchSpan.begin(), std::ranges::get<0>(searchResult)) -
      delimitOffset;
  std::shift_left(searchSpan.begin(), searchSpan.end(), positionOfHeader);
  return shortestMsgSize - positionOfHeader;
};

TEMPL_TYPES
std::uint32_t SOCK_HANDLER::discardLongMsg(std::uint32_t msgLength,
                                           void* bufferPtr) noexcept {
  int excessBytes = msgLength - bufferSize;
  const int availableBuffer = bufferSize - headerLength;
  std::uint32_t bytesRead{0};
  while (excessBytes > 0) {
    bytesRead = this->socketPtr->recv(
        reinterpret_cast<void*>(&this->bufferSpan[headerLength]),
        std::min(availableBuffer, excessBytes));
    excessBytes -= bytesRead;
  }
  return this->bufferSize;
};

// returns index of matching alternative, -1 if no match found
// argument var not needed for logic at runtime, but necessary to allow implicit
// decution of variant type Variant_  and implicit template specialization this
// allows the compiler to deduce all the indices in Is
TEMPL_TYPES
template <std::size_t... Is>
int SOCK_HANDLER::determineMsgType(int compareTo,
                                   std::index_sequence<Is...>) noexcept {
  int ret = -1;
  // lambda needs to be invoked with trailing "()" to work within fold
  // expression
  (
      [&]() {
        if (compareTo ==
            std::variant_alternative_t<Is, msgClassVariant>::msgLength) {
          ret = Is;
        }
      }(),
      ...);
  return ret;
};

// returns variant containing message class specified by index argument, default
// constructed variant if index is not valid for variant
TEMPL_TYPES
template <std::size_t... Is>
typename SOCK_HANDLER::msgClassVariant SOCK_HANDLER::constructVariant(
    int variantIndex, std::span<std::uint8_t> argument,
    std::index_sequence<Is...>) noexcept {
  msgClassVariant ret;
  (
      [&]() {
        if (Is == variantIndex) {
          ret.template emplace<
              std::variant_alternative_t<Is, msgClassVariant_>>(argument);
        }
      }(),
      ...);
  return ret;
};

TEMPL_TYPES
SOCK_HANDLER::fixSocketHandler(socketType* socketPtrArg)
    : socketPtr{socketPtrArg},
      bufferSpan{reinterpret_cast<std::uint8_t*>(readBuffer), bufferSize} {
  this->readBuffer = std::aligned_alloc(64, bufferSize);
  this->bufferSpan = std::span<std::uint8_t, bufferSize>{
      reinterpret_cast<std::uint8_t*>(readBuffer), bufferSize};
};

TEMPL_TYPES
SOCK_HANDLER::~fixSocketHandler() { std::free(this->readBuffer); };

#undef TEMPL_TYPES
#undef SOCK_HANDLER