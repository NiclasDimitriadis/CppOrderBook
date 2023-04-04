#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>



void printVecStats(std::vector<double> vec){
	int maxElemIndex = std::distance(vec.begin(), std::max_element(vec.begin(), vec.end()));
	int minElemIndex = std::distance(vec.begin(), std::min_element(vec.begin(), vec.end()));
	std::sort(vec.begin(), vec.end());
	int vecSize = vec.size();
	double mean = std::accumulate(vec.begin(), vec.end(), 0.0)/vecSize;
	double median = (vec[std::floor(vecSize * 0.5)] + vec[std::ceil(vecSize * 0.5)])/2;
	double min = vec[0];
	double max = vec[vecSize - 1];
	double onePcQuantile = (vec[std::floor(vecSize * 0.01)] + vec[std::ceil(vecSize * 0.01)])/2;
	double ninetyninePcQuantile = (vec[std::floor(vecSize * 0.99)] + vec[std::ceil(vecSize * 0.99)])/2;
	std::transform(vec.begin(), vec.end(), vec.begin(),
		[&](double arg){return std::pow(mean - arg, 2);});
	double stdDev = std::sqrt(std::accumulate(vec.begin(), vec.end(), 0.0)/(vecSize - 1));
	
	std::cout << "mean: " << mean << "\n";
	std::cout << "median: " << median << "\n";
	std::cout << "max: " << max << " at index: " << maxElemIndex << "\n";
	std::cout << "min: " << min << " at index: " << minElemIndex << "\n";
	std::cout << "1 percent quantile: " << onePcQuantile << "\n";
	std::cout << "99 percent quantile: " << ninetyninePcQuantile << "\n";
	std::cout << "standard deviation: " << stdDev << "\n";
 }