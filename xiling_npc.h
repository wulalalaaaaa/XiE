#pragma once
#include<string>
#include<vector>
#include<memory>
#include"basetools.h"
#include"xiling_player.h"

//当时这里在做水晶球模块中的npc角色，但是还没有做完，后来也没回来继续做

extern std::vector<jineng>alljineng;

class xiling_npc {
	std::string m_chenghao;
	std::string m_name;
	std::string m_show;
	int m_health;
	int m_healthmax;
	int m_magic;
	int m_magicmax;
	
	int m_liliang;
	int m_zhili;
	
	int m_wufang;
	int m_fafang;

public:
	int m_jinbi;

	std::vector<std::unique_ptr<wupin>>beibao;			//背包
	std::vector<std::unique_ptr<jineng>>jinengbiao;		//技能表

	std::vector<zhuangtai>buff;			//状态表

	std::vector<zhuangtai_acceptance>buff_acceptance;
	std::vector<zhuangtai_attack>buff_attack;

	xiling_player* m_player;//附身者


	xiling_npc()//纯路人，平民
		:m_chenghao(""),m_name("无名"),m_show("纯路人，请不要打我。"), m_health(100), m_healthmax(100), m_magic(10), m_magicmax(10), m_liliang(15), m_zhili(15),
		m_wufang(10),m_fafang(5),m_jinbi(50),m_player(nullptr) {}
	xiling_npc(const std::string& chenghao,const std::string& name,const std::string& show,int m_health,int m_magic,int m_liliang,int m_zhili,
		int m_wufang,int m_fafang,int m_jinbi)
		:m_chenghao(chenghao), m_name(name),m_show(show), m_health(m_health), m_healthmax(m_health), m_magic(m_magic), m_magicmax(m_magic),
		m_liliang(m_liliang), m_zhili(m_zhili),m_wufang(m_wufang), m_fafang(m_fafang), m_jinbi(m_jinbi),m_player(nullptr) {
		jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[0], m_player));
		jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[1], m_player));
	}
	xiling_npc(const std::string& chenghao, const std::string& name, const std::string& show, int m_health, int m_magic, int m_liliang, int m_zhili,
		int m_wufang, int m_fafang, int m_jinbi,xiling_player* one)
		:m_chenghao(chenghao), m_name(name), m_show(show), m_health(m_health), m_healthmax(m_health), m_magic(m_magic), m_magicmax(m_magic),
		m_liliang(m_liliang), m_zhili(m_zhili), m_wufang(m_wufang), m_fafang(m_fafang), m_jinbi(m_jinbi),m_player(one) {
		jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[0], m_player));
		jinengbiao.emplace_back(std::make_unique<jineng>(alljineng[1], m_player));
	}
	xiling_npc(const xiling_npc& other)
		:m_chenghao(other.m_chenghao),m_name(other.m_name),m_show(other.m_show),m_health(other.m_health),
		m_healthmax(other.m_healthmax),m_magic(other.m_magic),m_magicmax(other.m_magicmax),m_liliang(other.m_liliang),
		m_zhili(other.m_zhili),m_wufang(other.m_wufang),m_fafang(other.m_fafang),m_jinbi(other.m_jinbi),buff(other.buff),
		buff_acceptance(other.buff_acceptance),buff_attack(other.buff_attack),m_player(other.m_player)
	{
		if (!other.beibao.empty()) {
			for (int i = 0; i < other.beibao.size(); i++) {
				beibao.emplace_back(std::make_unique<wupin>(*other.beibao[i]));
			}
		}
		for (int i = 0; i < other.jinengbiao.size(); i++) {
			jinengbiao.emplace_back(std::make_unique<jineng>(*other.jinengbiao[i]));
		}

	}
	//xiling_npc(xiling_npc&&) = delete;
	xiling_npc operator=(const xiling_npc& other) {
		this->m_chenghao = other.m_chenghao;
		m_name = other.m_name; m_show = other.m_show; m_health = other.m_health;
		m_healthmax = other.m_healthmax; m_magic = other.m_magic;
		m_magicmax = other.m_magicmax; m_liliang = other.m_liliang;
		m_zhili = other.m_zhili; m_wufang = other.m_wufang;
		m_fafang = other.m_fafang; m_jinbi = other.m_jinbi;
		buff = other.buff;
		buff_acceptance = other.buff_acceptance;
		buff_attack = other.buff_attack;
		m_player = other.m_player;
		if (!other.beibao.empty()) {
			for (int i = 0; i < other.beibao.size(); i++) {
				beibao.emplace_back(std::make_unique<wupin>(*other.beibao[i]));
			}
		}
		for (int i = 0; i < other.jinengbiao.size(); i++) {
			jinengbiao.emplace_back(std::make_unique<jineng>(*other.jinengbiao[i]));
		}
		return *this;
	}
	//xiling_npc& operator=(xiling_npc&& one) {
	//	//xiling_npc a(one.m_chenghao, one.m_name, one.m_show, one.m_health, one.m_magic, one.m_liliang, one.m_zhili, one.m_wufang, one.m_fafang, one.m_jinbi);
	//	return xiling_npc(one);
	//}
	
	~xiling_npc() {

	}

	//体力
	constexpr int tili() {
		return static_cast<int>(m_healthmax / 10);
	}
	//精神
	constexpr int jingshen() {
		return static_cast<int>(m_magicmax / 10);
	}
	//单纯的名字（无称号）
	constexpr std::string& name() {
		return m_name;
	}
	//名字加称号
	std::string showName();
	//侦查失败后的？属性
	std::string showFalse();
	//所有属性
	std::string showall();


	//成功返回1，失败返回0；
	bool setPlayer(xiling_player* one) {
		if (m_player == nullptr) {
			m_player = one;
			m_name += "(" + one->name + ")";
			return 1;
		}
		else {
			return 0;
		}
	}




};
