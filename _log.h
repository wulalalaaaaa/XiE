#pragma once
#include<iostream>
#include<string>

//
//#define log(x) std::cout<<x<<std::endl

inline std::string defaultlog();

//报错等级
enum errorlevel {
	//正常
	green,
	//合法但有病
	okBut,
	//不允许但可以继续运行
	notAllow,
	//警告,出错了，但勉强还可以运行
	warning,
	//错误，你要是还想继续运行，随便吧，直到卡异常断出来
	error
};

template<int N ,typename T = std::string>
class _logs {
	T m_level[N]; 
	std::string m_more;
	int i = 0;
	
public:
		_logs() : m_more(defaultlog()) {
			for (; i < N; i++)m_level[i] = "[???]"; i = 0;}
		//errors + warnings + notAllows 务必 == N;无监管是否合法,这里的各个more之间用\n隔开，先error到notallow的顺序
		_logs(const std::string& more, int errors = 0, int warnings = 0, int notAllows = 0, int green = 0)
			:m_more(more) {
			i = N - 1;
			while (green--) { m_level[i] = "[ √ ]", i--; }
			while (notAllows--) { m_level[i] = "[notAllow]", i--; }
			while (warnings--) { m_level[i] = "[warning]", i--; }
			while (errors--) { m_level[i] = "[error]", i--; }i = 0;}
		

	void m_log() {
		
		std::string linshi(""); 
		int j = 0;
			while (j<N) {
				int begin = i;
				while (m_more[i] != '\n')  i++; 
				linshi = m_more.substr(begin, begin + i);
				i++;
				std::cout << m_level[j] << linshi << std::endl;
				linshi="";
				j++;
			}
	}

};

template<typename T = std::string>
class _log {
	T m_level;
	std::string m_more;

public:
	_log() :m_level("[error]"), m_more(defaultlog()) {}
	//需要日期文件位置等，可以在"more"后面+ defaultlog()作为more参数
	_log(const T& level,const std::string& more)
		:m_level(level), m_more(more) {}

	void m_log() {
		std::cout << m_level << m_more << std::endl;
	}
};

inline std::string defaultlog() {	
	std::string out("file: ");
	out += __FILE__;
	out += " time: ";
	out += __DATE__;
	out += __TIME__;
	return out;
}



