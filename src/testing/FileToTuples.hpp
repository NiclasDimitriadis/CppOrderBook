#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Auxil.hpp"

namespace FileToTuples{
	template<typename LineTuple, int... Is>
	void fileToTuplesLogic(const std::string& filePath, std::vector<LineTuple>& retVector, std::integer_sequence<int, Is...>, char delimiter){
		std::ifstream inputStream(filePath);
		if (!inputStream.is_open()){
			std::cout << "unable to open file " << filePath << "\n";
			return;
		}
		//generate sequence for indeces of line tuple to use in fold expression
		std::string line;
		std::string buffer;
		while(std::getline(inputStream, line)){
			std::istringstream sstream(line);
			LineTuple tempTuple;
			(([&](){
				std::getline(sstream, buffer, delimiter);
				//insert 0 if csv entry is empty or missing
				//use type of tuple entry to find the matching overload of function template stoArith to convert string to respective type
				try{std::get<Is>(tempTuple) = stoArith<std::tuple_element_t<Is, LineTuple>>(buffer);}
				catch (const std::exception& ex) {std::get<Is>(tempTuple) = 0;}
				})(),...);//invoke lambda, repeat for every index in Is 
			retVector.push_back(tempTuple);
		};
	};
	
	//wrapper function that inserts indices of tuple as parameter pack
	template<typename LineTuple>
	requires Auxil::validate_tuple<LineTuple, std::is_arithmetic>::value
	std::vector<LineTuple> fileToVecOfTuples(const std::string& filePath, char delimiter = ','){
		static constexpr auto Is =  std::make_integer_sequence<int, std::tuple_size_v<LineTuple>>();
		std::vector<LineTuple> retVector;
		fileToTuplesLogic(filePath, retVector, Is, delimiter);
		return retVector;
	};
}