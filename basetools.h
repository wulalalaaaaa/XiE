#pragma once
#include<string>

template<typename T>
std::string tostring(T value) {

	static const char digits[19] = {
		'9','8','7','6','5','4','3','2','1','0',
		'1','2','3','4','5','6','7','8','9'
	};
	static const char* zero = digits + 9;//zero->'0'

	char buf[24];      //不考虑线程安全的情况时，可以改成静态变量
	T i = value;
	char* p = buf + 24;
	*--p = '\0';
	do {
		int lsd = i % 10;
		i /= 10;
		*--p = zero[lsd];
	} while (i != 0);
	if (value < 0)
		*--p = '-';
	return std::string(p);
}


template<typename T>
T getint(std::string& a) {

	T num_caishu = 0;
	for (int i = 0; i <= a.size() - 1; i++)
	{
		if (a.at(i) >= '0' && a.at(i) <= '9')
		{
			num_caishu = num_caishu * 10 + a.at(i) - 48;
		}
	}
	return num_caishu;
}