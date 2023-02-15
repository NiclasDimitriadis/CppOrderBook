//class for handling a single socket, retrieving FIX SBE messages that come with Simple Open Framing Header,
//constructing an object representing one of multiple types of messages in a std::variant
//and emplacing it in some data structure to be consumed elsewhere
#pragma once 

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>

#include "Auxil.hpp"
#incldue "FIXmsgTypeChecks.hpp"

namespace rng = std::ranges;

namespace fixSocketHandler
{
    template <typename msgClassVariant_, typename socketType_,
	typename connectionErrorHandler, typename readErrorHandler>
    requires  Auxil::readableAsSocket<socketType>
	&& std::invocable<connectionErrorHandler> && std::invocable<readErrorHandler>
	&&MsgTypeChecks::consistencyChecksAcrossMsgTypes<msgClassVariant_>::value
	&&MsgTypeChecks::alternativeDefaultConstructible<msgClassVariant_>::value
    struct fixSocketHandler
    {
    private:
		static constexpr std::uint32_t delimitOffset = std::variant_alternative_t<0, msgClassVariant_>::delimiterOffset;
        static constexpr std::uint32_t deliminitLen = std::variant_alternative_t<0, msgClassVariant_>::delimiterLength;
        static constexpr std::uint16_t delimitValue = std::variant_alternative_t<0, msgClassVariant_>::delimiterValue;
		std::span<std::uint8_t, 2> delimitSpan;
		std::uint8_t readBuffer[this->bufferSize];
        static constexpr std::uint32_t lengthOffset = std::variant_alternative_t<0, msgClassVariant_>::lengthOffset;
		static constexpr std::uint8_t lengthLen = sizeof(decltype(std::variant_alternative_t<0, msgClassVariant_>::lengthOffset));
        static constexpr std::uint32_t headerLength = std::variant_alternative_t<0, msgClassVariant_>::headerLength;
		static constexpr std::uint32_t bufferSize = MsgTypeChecks::determineMaxMsgLen<msgClassVariant_>;
		static constexpr auto indexSeq = std::make_index_sequence<std::variant_size_v<msgClassVariant_>>;
        const socketType* socketPtr;
		sockaddr_in sockaddr;
		//functors containing mechanisms for handling different errors regarding the socket
		connectionErrorHandler conn_err_hdlr;
		readErrorHandler rd_err_hdlr;
        std::optional<uint32_t> seqIncludesHeader(std::span<std::uint8_t, bufferSize> sequence) const noexcept;
        bool validateChecksum(std::uint32_t, const std::span<std::uint8_t, this->bufferSize>&) noexcept;
        std::int32_t scanForDelimiter(std::int32_t, const std::span<const std::byte, this->bufferSize>&) noexcept;
		//safely discards excess bytes if an incoming message is longer than any of the message types passed to buffer (and hence longer than buffer)
		//returns size of buffer for caller to assign to message-length variable so all remaining bytes can be read within regular program logic
        std::uint32_t discardLongMessage(std::uint32_t, std::uint8_t*) noexcept;
		
		template <std::size_t... Is>
		int determineMsgType(const int&, std::index_sequence<Is...>) noexcept;
		
		template <std::size_t... Is>
		msgClassVariant constructVariant(int, std::span<std::uint8_t>, std::index_sequence<Is...>) noexcept;
		
    public:
		explicit fixSocketHandler(socketType*) noexcept;
		typedef msgClassVariant_ msgClassVariant;
		typedef destBufferType_ destBufferType;
		typedef socketType_ socketType;
        std::optional<sockHandler::msgClassVariant> readNextMessage() noexcept;
    };
}

#define sockHandler fixSocketHandler::fixSocketHandler<messageClass, socketType>

template <typename messageClass, typename socketType>
std::optional<sockHandler::msgClassVariant> sockHandler::readNextMessage() noexcept
{
	auto bufferSpan  =std::span<std::uint8_t, this->bufferSize>{this->readBuffer};
	//index sequence used for visiting all variant alternatives
	//read length-of-header bytes into buffer
	std::int32_t bytesRead = this->*socketPtr.recv(this->readBuffer, this->headerLenght);
	//invoke error-handler object if reading the necessary # of bytes fails
	if(bytesRead < this->headerLength){this->rd_err_hdlr();};
	// verifiy that header is valid by checking whether delimeter sequence is in expected place
	const bool validHeader = rng::equal(bufferSpan.subspan(delimitOffset, delimitLen), this->delimitSpan);
	bytesRead = validHeader ? bytesRead : scanForDelimiter(bytesRead, bufferSpan);
	// extract message length from message header
	int msgLength = *(std::uint32_t *) (&readSpan[this->lengthOffset]);
	msgLength = msgLength <= this->bufferSize ? msgLength : this->discardLongMessage(msgLength, this->readBuffer); 
	//read rest of message into buffer
	bytesRead += this->*socketPtr.recv(&this->readBuffer[bytesRead], msgLength - this->headerLength);
	//invoke error-handler object if reading the necessary # of bytes fails
	if(bytesRead < (msgLength - this->headerLenght)){this->rd_err_hdlr();};
	//determine type of received message by checking all types in variant
	static constexpr auto variantIndexSeq = std::make_index_sequence<std::variant_size_v<msgClassVariant_>>;
	int msgTypeIndex = determineMsgType(msgLength, variantIndexSeq);
	//validate message via checksum, if invalid message is discarded by setting type-index to -1, indicating cold path
	msgTypeIndex = validateChecksum(msgLength, bufferSpan) ? msgTypeIndex : -1;
	//enqueue message to buffer
	const auto returnVariant = this->constructVariant(msgTypeIndex);
	std::optional<msgClassVariant> ret = msgTypeIndex >=0 ? returnVariant : std::nullopt;
	return ret;
}

template <typename messageClass, typename socketType>
bool sockHandler::validateChecksum(std::uint32_t msgLenght, const std::span<std::uint8_t, bufferSize> &buffer) noexcept
{
    const std::uint8_t byteSum = std::ranges::fold_left(buffer.subspan(0, msgLength - 4), (std::uint8_t)0, std::plus<std::uint8_t>);
	//convert three ASCII characters at the end of FIX msg to unsigned 8 bit int to compare to checksum computed above 
	std::uint8_t msgChecksum = ((buffer[bufferSize-3]) - 48)*100 
	+ ((buffer[bufferSize-2]) - 48)*10
	+ ((buffer[bufferSize-1]) - 48);
	return byteSum == msgChecksum;
}

template <typename messageClass, typename socketType>
void sockHandler::scanForDelimiter(std::uint32_t bytesInBuffer, std::span<std::uint8_t, bufferSize> &bufferSpan) noexcept
{
    bool delimiterFound{false};
    auto searchResult = rng::search(bufferSpan, delimitValue);
    bool delimiterFound = !searchResult.is_empty();
    int bytesToSearch = bytesInBuffer;
    int bytesToKeep{this->delimitOffset + this->delimitLength - 1};
    // if no valid delimiter in expected place, read max. # of bytes in message, try to find a delimiter
    while (!delimiterFound)
    {
        std::shift_left(bufferSpan, bytesToSearch - bytesToKeep);
        bytesToSearch = recv(this->*socketPtr, &buffer[bytesToKeep], this->bufferSize - bytesToKeep);
        searchResult = rng::search(bufferSpan, delimitValue);
        delimiterFound = !searchResult.is_empty();
    }
    bytesToKeep = std::distance(bufferSpan.begin(), rng::get<0>(searchResult));
    std::shift_left(bufferSpan, bytesToSearch - bytesToKeep);
    return bytesToKeep;
}

template <typename messageClass, typename socketType>
std::uint32_t sockHandler::discardLongMsg(std::uint32_t msgLength, std::uint8_t* bufferPtr) noexcept{
	const int excessBytes = msgLength - this->bufferSize;
	const int availableBuffer = this->bufferSize - this->headerLength;
	std::uint32_t bytesRead{0};
	while(excessBytes > 0){
		bytesRead = this->*socketPtr.recv(&buffer[this->headerLength], std::min(availableBuffer, excessBytes));
		if(bytesRead != std::min(availableBuffer, excessBytes)){this->rd_err_hdlr();};
		excessBytes -= bytesRead;
	}
	return this->bufferSize;
};

//returns index of matching alternative, -1 if no match found
//argument var not needed for logic at runtime, but necessary to allow implicit decution of variant type Variant_  and implicit template specialization 
//this allows the compiler to deduce all the indices in Is 
template <typename messageClass, typename socketType>
template <std::size_t... Is>
int sockHandler::determineMsgType(const int& compare_to , std::index_sequence<Is...>) noexcept{
	int ret = -1;
	//lambda needs to be invoked with trailing "()" to work within fold expression
	([&](){if(compare_to == std::variant_alternative_t<Is, msgClassVariant>::msgLength){ret = Is;}}(),...);
	return ret;
}

//returns variant containing message class specified by index argument, default constructed variant if index is not valid for variant
template <typename messageClass, typename socketType>
template <std::size_t... Is>
sockHandler::msgClassVariant sockHandler::constructVariant(int variantIndex, std::span<std::uint8_t> argument, std::index_sequence<Is...>) noexcept{
	msgClassVariant ret;	
	([&](int index){if (index == variantIndex){ret.emplace<index>(argument);}}(Is),...);
} 

template <typename messageClass, typename socketType>
sockHandler::fixSocketHandler(socketType* socketPtrArg): socketPtr{socketPtrArg) noexcept{
		this->delimitSpan = std::span<std::uint8_t, 2>{reinterpret_cast<std::uint8_t*>(&delimitValue), 2};
		if constexpr (std::endian::native == std::endian::little)
			std::ranges::reverse(this->delimitSpan);
	};

#undef msgHandler